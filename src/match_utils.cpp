#include <iostream>
#include <fstream>
//#include <bitset>
#include <functional>
#include <boost/dynamic_bitset.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <opencv/cv.h>	
#include <opencv/highgui.h>

#include "match_utils.h"
#include "log.h"

#include "utils.h"

using namespace std;
using namespace boost;

int THREADS = 12;

// 2月3日聚合方法重要变化：FPGA增加了输入数据缓冲，这使得输出结果的顺序难以控制
// 新方案FPGA每批做31位聚合，CPU对结果计数即可，不需再做拼接
// 代码中仍保留拼接功能，输出结果更好看些

MatchArgs ARGS;

void MatchArgs::load(const string& arg_file) {
	using namespace boost::property_tree;
	ptree pt;
	read_info(arg_file, pt);

	feature_bits   = pt.get<int>("feature_bits", 32);
	frame_thres    = pt.get<int>("frame_thres", 1);
//	sequence_thres = pt.get<int>("sequence_thres", 0);
	min_match_len  = pt.get<int>("min_match_len", 8);
//	max_skip       = pt.get<int>("max_skip", 0);
	batch_len      = pt.get<int>("batch_len", 32);
	overlap_len    = pt.get<int>("overlap_len", 0);
//	diag_test_bits = pt.get<int>("diag_test_bits", 3);
	diag_test_thres= pt.get<int>("diag_test_thres", 20);
}

// =====================================================================================
// Utility functions

// 将图像转换为不超过640*480的灰度图
IplImage* transformImage(IplImage* img) {
	const int max_width  = 640;
	const int max_height = 640;	//480;
	if (!img) return img;
	IplImage* oimg = img;	// 由VideoCapcture得到的image不能释放
/*	if (img->height > img->width && img->height > max_height) {		// 旋转90°
		IplImage* rimg = cvCreateImage(cvSize(img->height, img->width), img->depth, img->nChannels);
		float m[6] = {0, -1, img->width / 2., 1, 0, img->height / 2.};
		CvMat M = cvMat( 2, 3, CV_32F, m );
		cvGetQuadrangleSubPix(img, rimg, &M);
//		cvReleaseImage(&img);
		img = rimg;
	}
*/	if (img->width > max_width || img->height > max_height) {		// 缩放
		float csc = std::min((float)max_width / img->width, (float)max_height / img->height);
		IplImage* simg = cvCreateImage(cvSize(img->width * csc, img->height * csc), img->depth, img->nChannels);
		cvResize(img, simg, CV_INTER_AREA);
		if (img != oimg)
			cvReleaseImage(&img);
		img = simg;
	}

//	if (img->depth == IPL_DEPTH_8U && img->nChannels == 1)
//		return img;				// 已经是灰度图
	// 转为灰度图
	IplImage* gray8 = cvCreateImage( cvGetSize(img), IPL_DEPTH_8U, 1 );
	if( img->nChannels == 1 )
		cvConvert( img, gray8 );
	else 
		cvCvtColor( img, gray8, CV_BGR2GRAY );
	if (img != oimg)
	cvReleaseImage(&img);
	return gray8;
}

template <typename LIB>
int saveNewVideoLib(int nbits, const string& video_name, int video_num, LIB& lib) {
	VideoLibRec video_rec;
	video_rec.video_num = video_num;
	
	VideoCapcture capture(video_name, 0, 0, 1);
	FDU_LOG(INFO) << video_name << " : Frame Count = "<< capture.frame_count <<", fps = " << capture.fps;

	while(IplImage* img = capture.get_frame()) {
		FDU_LOG(INFO) << "write frame " << capture.frame_pos  << " to lib, timestamp = " << capture.time_stamp << " s";
		video_rec.point_vec.push_back(order_feature(nbits, img));
	}

	video_rec.num_of_frames = video_rec.point_vec.size();
	FDU_LOG(INFO) << "video " << video_rec.video_num << " write " << video_rec.num_of_frames << " frames in lib "; 
	saveLibNew(video_rec, lib);
	return 0;
}

// =====================================================================================
// VideoCapcture

VideoCapcture::VideoCapcture(const string& videoFileName, int capBegin, int capEnd, double capFps)
	: fps(0), time_stamp(0), frame_pos(0), cap_end(capEnd), cap_step(capFps)
{
	capture = cvCaptureFromFile(videoFileName.c_str());
	if(!capture) {
		FDU_LOG(ERR) << "No Capture";
		return;
	}
	fps = ceil(cvGetCaptureProperty(capture, CV_CAP_PROP_FPS));			// 保持与建库程序的一致性
	frame_count = cvGetCaptureProperty( capture, CV_CAP_PROP_FRAME_COUNT);	//总帧数

	if (cap_end <= 0)
		cap_end += frame_count / fps + 1;

	next_frame = capBegin * fps;
	if (capFps > 0)
		cap_step = fps / capFps;
	// 初始定位
	for (double test_frame = next_frame - fps; test_frame > 0; test_frame -= fps) {
		cvSetCaptureProperty(capture, CV_CAP_PROP_POS_FRAMES, test_frame);
		cvQueryFrame(capture);
		frame_pos = cvGetCaptureProperty(capture, CV_CAP_PROP_POS_FRAMES);
		if (frame_pos < next_frame) break;
	}
	FDU_LOG(DEBUG) << "cap_step: " << cap_step << "  frame_pos: " << frame_pos << "  next_frame: " << next_frame;
}

VideoCapcture::~VideoCapcture() { cvReleaseCapture(&capture); }

IplImage* VideoCapcture::get_frame() {
	if(!capture) return 0;
	IplImage* img;
//	FDU_LOG(DEBUG) << "cap_step: " << cap_step << "  frame_pos: " << frame_pos << "  next_frame: " << next_frame;
	if (cap_step < 0 && frame_pos > 0)
		cvSetCaptureProperty(capture, CV_CAP_PROP_POS_FRAMES, (double)frame_pos + 1);
	do {
		img = cvQueryFrame(capture);
		if(!img) return 0;
		frame_pos = cvGetCaptureProperty(capture, CV_CAP_PROP_POS_FRAMES);
	} while (frame_pos < next_frame);
	next_frame += cap_step;
	time_stamp = frame_pos / fps;
	return time_stamp > cap_end ? 0 : img;
}

// =====================================================================================
// SectionManager

void MatchSection::merge(const MatchSection& rhs) {
	ASSERTS(same_sequence(*this, rhs));
	int new_end = max(test_begin + len, rhs.test_begin + rhs.len);
	test_begin = min(test_begin, rhs.test_begin);
	lib_begin = min(lib_begin, rhs.lib_begin);
	len = new_end - test_begin;
}

ostream& operator << (ostream& os, const MatchSection& sec) {
	return os << " toMatch " << sec.test_pos() <<" - " << sec.test_pos() + sec.len
		 << " Lib " << sec.lib_id() << ": " << sec.lib_pos() <<" - " << sec.lib_pos() + sec.len;
}

inline bool less (const MatchSection& lhs, const MatchSection& rhs)
{ return lhs < rhs; }  

// 算法假定：新加入的段是单位长度（31）的小段
// 链中的段各不相邻，按起始位置排序
// 加入处理：无合并，简单插入；与左侧段合并；与右侧段合并；三段合并
void SectionManager::add_section(const MatchSection& sec) {
	FDU_LOG(DEBUG) << "add_section: test_pos = " << sec.test_id() << ":" << sec.test_pos()
				   << " lib_pos = " << sec.lib_id() << ":" << sec.lib_pos();

	enum merge_flag { none = 0, left, right, all };
	int flag = none;
	Chain& ch = chains_[sec.offset_id()];
	
	// 使用反向迭代器可同时得到待插入位置两侧的元素
	Chain::reverse_iterator rp = find_if(ch.rbegin(), ch.rend(), boost::bind(::less, _1, boost::ref(sec)));
	Chain::iterator p = rp.base();

	if (rp != ch.rend() && sec_dist(sec, *rp) <= 0) {
//		FDU_LOG(DEBUG) << "merge left";
		rp->merge(sec);
		flag += left;
	}

	if (p != ch.end() && sec_dist(sec, *p) <= 0) {
//		FDU_LOG(DEBUG) << "merge right";
		p->merge(sec);
		flag += right;
	}

	if (flag == none) {
//		FDU_LOG(DEBUG) << "insert";
		ch.insert(p, sec);
	}
	else if (flag == all) {
//		FDU_LOG(DEBUG) << "merge all";
		rp->merge(*p);
		ch.erase(p);
	}
}

const MatchResults& SectionManager::gather_results() {
	 results_.clear();
	for (ChainMap::const_iterator p = chains_.begin(); p != chains_.end(); ++p) {
		int test_id = (p->first >> 32) & 0xffff;
		foreach (const MatchSection& sec, p->second) {
			if (sec.len >= ARGS.min_match_len)
				results_.insert(make_pair(test_id, sec.lib_id()));
		}
	}
	return results_;
}

void SectionManager::output_chains(ostream& out, int video_id) {
	int ns = 0;
	ChainMap::iterator p = chains_.lower_bound((long long)video_id << 32);
	for (; p != chains_.end() && ((p->first >> 32) & 0xffff) == video_id; ++p) {
		foreach (const MatchSection& sec, p->second) {
			if (sec.len < ARGS.min_match_len) continue;
			out << "section "<< ++ns << " \t Length: " << sec.len << endl;
			out << "LibVideo " << sec.lib_id() << ": " << sec.lib_pos() <<" - " << sec.lib_pos() + sec.len << endl;
			out << "VideotoMatch " << sec.test_pos() <<" - " << sec.test_pos() + sec.len << endl;
		}
	}
}

void SectionManager::delete_chains(int test_id) {
	ChainMap::iterator beg = chains_.lower_bound((long long)test_id << 32);
	if (beg == chains_.end()) return;
	ChainMap::iterator end = chains_.lower_bound((long long)(test_id + 1) << 32);
	chains_.erase(beg, end);
}

// =====================================================================================
// VideoMatcher

void VideoMatcher::match_frames(const LibPointVec& frames, int frame_begin) {
	int match_len = frames.size();
	for (int vi = 0; vi < lib_.size(); ++vi) {
		const LibPointVec& vlf = lib_[vi].point_vec;
//FDU_LOG(DEBUG) << "video: " << lib_[vi].video_num << "  frames: " << vlf.size();
		for (int li = 0; li <= (int)vlf.size() - match_len; ++li) {
			int matches = 0;
			for (int fi = 0; fi < match_len; ++fi) {
				if (hamming_distance(frames[fi], vlf[li + fi]) <= ARGS.frame_thres)
					++matches;
			}
			if (matches >= ARGS.diag_test_thres)
				sec_man_.add_section(MatchSection(frame_begin, (lib_[vi].video_num << 16) + li, match_len));
		}
	}
}

void VideoMatcher::match_video(int video_id, const std::string& video_file, int frame_start, int frame_end) {
	LibPointVec video_points;
	VideoCapcture capture(video_file, frame_start, frame_end);
	FDU_LOG(INFO) << video_file << " : Frame Count = "<< capture.frame_count <<", fps = " << capture.fps;
	while(IplImage* img = /*transformImage*/(capture.get_frame())) {
		FDU_LOG(INFO) << "Matching framePos = " << capture.frame_pos <<", TimeStamp = " << capture.time_stamp << "s";
		video_points.push_back(order_feature(ARGS.feature_bits, img));
	}
	match_video(video_id, video_points, frame_start, frame_end);
}

void VideoMatcher::match_video(int video_id, const LibPointVec& video_points, int frame_start, int frame_end) {
//	const LibPointVec& video_points = video_to_match[0].point_vec;
	if (frame_end <= 0 || frame_end > video_points.size())
		frame_end = video_points.size();
	LibPointVec frames;
	for (int i = frame_start; i < frame_end; ++i) {
		frames.push_back(video_points[i]);
		if (frames.size() >= ARGS.batch_len) {
			match_frames(frames, (video_id << 16) + i - frames.size() + 1);
			frames.erase(frames.begin(), frames.end() - ARGS.overlap_len);
		}
	}
//	ofstream out("match_result.txt");
//	out << "Match result" << endl;
//	sec_man_.output_chains(out, 0);
//	sec_man_.gather_results();
}

template int saveNewVideoLib<const string>(int , const string& , int , const string& );
