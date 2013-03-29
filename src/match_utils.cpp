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

// 2��3�վۺϷ�����Ҫ�仯��FPGA�������������ݻ��壬��ʹ����������˳�����Կ���
// �·���FPGAÿ����31λ�ۺϣ�CPU�Խ���������ɣ���������ƴ��
// �������Ա���ƴ�ӹ��ܣ����������ÿ�Щ

MatchArgs ARGS;

int THREADS = 6;

void MatchArgs::load(const string& arg_file) {
	using namespace boost::property_tree;
	ptree pt;
	read_info(arg_file, pt);

	feature_bits   = pt.get<int>("feature_bits", 32);
	frame_thres    = pt.get<int>("frame_thres", 1);
	min_match_len  = pt.get<int>("min_match_len", 8);
	batch_len      = pt.get<int>("batch_len", 32);
	overlap_len    = pt.get<int>("overlap_len", 0);
	diag_test_thres= pt.get<int>("diag_test_thres", 20);
}

static const int min_acc = 1, max_acc = 4;
static const int min_cnv = 1, max_cnv = 4;
static const int args_table[][4] = {
	{ 32, 64, 128, 128 },	// 0: feature_bits
	{ 8, 16, 32, 32 }		// 1: batch_len
};

void MatchArgs::set_accuracy(int a) {
	if (a < min_acc || a > max_acc)
		FDU_LOG(ERR) << "set_accuracy: wrong accuracy: " << a;
	else feature_bits = args_table[0][a - min_acc];
}
void MatchArgs::set_converge(int c) {
	if (c < min_cnv || c > max_cnv)
		FDU_LOG(ERR) << "set_converge: wrong converge: " << c;
	else batch_len = args_table[1][c - min_cnv];
}

// =====================================================================================
// Utility functions

// ��ͼ��ת��Ϊ������640*480�ĻҶ�ͼ
IplImage* transformImage(IplImage* img) {
	const int max_width  = 640;
	const int max_height = 640;	//480;
	if (!img) return img;
	IplImage* oimg = img;	// ��VideoCapcture�õ���image�����ͷ�
/*	if (img->height > img->width && img->height > max_height) {		// ��ת90��
		IplImage* rimg = cvCreateImage(cvSize(img->height, img->width), img->depth, img->nChannels);
		float m[6] = {0, -1, img->width / 2., 1, 0, img->height / 2.};
		CvMat M = cvMat( 2, 3, CV_32F, m );
		cvGetQuadrangleSubPix(img, rimg, &M);
//		cvReleaseImage(&img);
		img = rimg;
	}
*/	if (img->width > max_width || img->height > max_height) {		// ����
		float csc = std::min((float)max_width / img->width, (float)max_height / img->height);
		IplImage* simg = cvCreateImage(cvSize(img->width * csc, img->height * csc), img->depth, img->nChannels);
		cvResize(img, simg, CV_INTER_AREA);
		if (img != oimg)
			cvReleaseImage(&img);
		img = simg;
	}

//	if (img->depth == IPL_DEPTH_8U && img->nChannels == 1)
//		return img;				// �Ѿ��ǻҶ�ͼ
	// תΪ�Ҷ�ͼ
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
	fps = ceil(cvGetCaptureProperty(capture, CV_CAP_PROP_FPS));			// �����뽨������һ����
	frame_count = cvGetCaptureProperty( capture, CV_CAP_PROP_FRAME_COUNT);	//��֡��

	if (cap_end <= 0)
		cap_end += frame_count / fps + 1;

	next_frame = capBegin * fps;
	if (capFps > 0)
		cap_step = fps / capFps;
	// ��ʼ��λ
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

// �㷨�ٶ����¼���Ķ��ǵ�λ���ȣ�31����С��
// ���еĶθ������ڣ�����ʼλ������
// ���봦���޺ϲ����򵥲��룻�����κϲ������Ҳ�κϲ������κϲ�
void SectionManager::add_section(const MatchSection& sec) {
	FDU_LOG(DEBUG) << "add_section: test_pos = " << sec.test_id() << ":" << sec.test_pos()
				   << " lib_pos = " << sec.lib_id() << ":" << sec.lib_pos();

	enum merge_flag { none = 0, left, right, all };
	int flag = none;
	Chain& ch = chains_[sec.offset_id()];
	
	// ʹ�÷����������ͬʱ�õ�������λ�������Ԫ��
	auto rp = find_if(ch.rbegin(), ch.rend(), boost::bind(::less, _1, boost::ref(sec)));
	auto p = rp.base();

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
	for (auto p = chains_.cbegin(); p != chains_.cend(); ++p) {
		int test_id = (p->first >> 32) & 0xffff;
		foreach (const MatchSection& sec, p->second) {
			if (sec.len >= ARGS.min_match_len)
				results_.insert(make_pair(test_id, sec.lib_id()));
		}
	}
	return results_;
}

void SectionManager::output_chains(ostream& out, int test_id) {
	int ns = 0;
	auto p = chains_.lower_bound((long long)test_id << 32);
	for (; p != chains_.end() && ((p->first >> 32) & 0xffff) == test_id; ++p) {
		foreach (const MatchSection& sec, p->second) {
			if (sec.len < ARGS.min_match_len) continue;
			out << "section "<< ++ns << " \t Length: " << sec.len << endl;
			out << "LibVideo " << sec.lib_id() << ": " << sec.lib_pos() <<" - " << sec.lib_pos() + sec.len << endl;
			out << "VideotoMatch " << sec.test_pos() <<" - " << sec.test_pos() + sec.len << endl;
		}
	}
}

void SectionManager::delete_chains(int test_id) {
	auto beg = chains_.lower_bound((long long)test_id << 32);
	if (beg == chains_.end()) return;
	auto end = chains_.lower_bound((long long)(test_id + 1) << 32);
	chains_.erase(beg, end);
}

// ----------------------

void SectionManagerSimp::add_section(const MatchSection& sec) {
	FDU_LOG(DEBUG) << "add_section: test_pos = " <<sec.video_id() <<":" << sec.slice_id() << ":" << sec.test_pos()
				   << " lib_pos = " << sec.lib_id() << ":" << sec.lib_pos();
	++results_[sec.offset_id()];
}

void SectionManagerSimp::output_results(ostream& out, int test_id) {
	test_id &= 0xfffff;
	auto end = results_.lower_bound((long long)(test_id + 1) << 32);
	for (auto p = results_.lower_bound((long long)test_id << 32); p != end; ++p) {
		if (p->second < slice_thres_) continue;
		MatchSection sec(test_id << 12, p->first & 0xffffffff);
		out << "VideotoMatch " << sec.video_id() <<":" << sec.slice_id() << endl;
		out << "LibVideo " << sec.lib_id() << ":" << sec.lib_pos() << endl;
	}
}

void SectionManagerSimp::delete_results(int test_id) {
	test_id &= 0xfffff;
	auto beg = results_.lower_bound((long long)test_id << 32);
	if (beg == results_.end()) return;
	auto end = results_.lower_bound((long long)(test_id + 1) << 32);
	results_.erase(beg, end);
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
