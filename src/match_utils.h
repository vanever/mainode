#ifndef MATCH_UTILS_HPP
#define MATCH_UTILS_HPP

#include <string>
#include <iosfwd>
#include <list>
#include <map>
#include <set>

#include "fdutils.h"
#include "integral.h"
#include "newlib.h"

struct MatchArgs {
	int feature_bits;		// 特征精度
	int frame_thres;		// 单帧匹配阈值
	int min_match_len;		// 最小连续匹配帧数阈值
	int batch_len;			// 每组匹配帧数
	int overlap_len;		// 相邻两组间重叠帧数
	int diag_test_thres;	// 每组匹配结果判断阈值
	
	int slice_len;			// 再聚合视频长度
	int slice_thres;		// 再聚合阈值

	void load(const string& arg_file);
	void set_accuracy(int a);
	void set_converge(int c);
};

extern MatchArgs ARGS;

// =====================================================================================
// Utility functions

IplImage* transformImage(IplImage* img);

template <typename LIB>
int saveNewVideoLib(int nbits, const std::string& video_name, int video_num, LIB& lib);

struct CvCapture;

struct VideoCapcture {
	// capFps : 抽帧速率，若为0或大于视频fps，则按视频fps抽取，小于0则表示提取关键帧
	VideoCapcture(const string& videoFileName, int framePosBegin, int framePosEnd, double capFps = 1.);
	~VideoCapcture();
	IplImage* get_frame();
	double frame_count;
	double fps;
	double time_stamp;
	int frame_pos;
private:
	int cap_end;
	double next_frame;
	double cap_step;
	CvCapture* capture;
};

/* 新方案匹配结果直接使用MatchSection即可
struct MatchResult {
	int test_begin, lib_begin;
	int match_len;
	unsigned int match_bits;
	MatchResult(int tb, int lb, int ln, unsigned int bits) :
		test_begin(tb), lib_begin(lb), match_len(ln), match_bits(bits) {}
};
*/

// 待测视频编号规则：
// 视频号：10位；片段号（起始位置分钟数）：10位；片断内位置（秒）：12位
// 允许的最大片段长度为68分钟
inline int test_num(int vid, int sid, int pos)
{ return (vid << 22) | (sid << 12) | pos; }

struct MatchSection {
	int test_begin, lib_begin;
	int len;
	MatchSection(int tb = 0, int lb = 0, int ln = 0) :
		test_begin(tb), lib_begin(lb), len(ln) {}
	int test_id()  const { return (test_begin >> 12) & 0xfffff; }	// 含视频号和片段号
	int video_id() const { return test_id()  >> 10; }
	int slice_id() const { return test_id()  & 0x3ff; }
	int lib_id()   const { return (lib_begin >> 16) & 0xffff;  }
	int test_pos() const { return test_begin & 0xfff;  }
	int lib_pos()  const { return lib_begin  & 0xffff; }
	long long offset_id()  const {
		return ((long long)test_id() << 32) | ((long long)lib_id() << 16)
			 | ((lib_pos() - test_pos()) & 0xffff);
	}
	void merge(const MatchSection& rhs);
};
inline bool same_sequence(const MatchSection& lhs, const MatchSection& rhs)
{ return lhs.offset_id() == rhs.offset_id(); }

inline bool operator < (const MatchSection& lhs, const MatchSection& rhs)
{ return same_sequence(lhs, rhs) ? lhs.test_begin < rhs.test_begin : lhs.offset_id() < rhs.offset_id(); }  

// 2个匹配段间的间隔距离，小于0代表有重叠
inline int sec_dist(const MatchSection& lhs, const MatchSection& rhs) {
	return  lhs.test_begin < rhs.test_begin ?
			rhs.test_begin - lhs.test_begin - lhs.len :
			lhs.test_begin - rhs.test_begin - rhs.len;
}

extern std::ostream& operator << (std::ostream& os, const MatchSection& sec);

typedef std::pair<int, int> MatchResult;		// 简化的匹配结果：(待测视频编号, 库视频编号)
typedef std::set<MatchResult> MatchResults;

class SectionManager {
public:
	typedef std::list<MatchSection> Chain;		// 有序链表

	void add_section(const MatchSection& sec);

	const MatchResults& gather_results();
	const MatchResults& results() const { return results_; }

	void output_chains(std::ostream& os, int test_id);	// test_id为待测视频编号（帧编号前20位）
	void delete_chains(int test_id);
	void clear() { chains_.clear(); }

private:
	std::map<long long, Chain> chains_;			// 使用offset_id作为key
	MatchResults results_;
};

class SectionManagerSimp {
public:
	SectionManagerSimp(int thres) : slice_thres_(thres) {}
	void add_section(const MatchSection& sec);

	void output_results(std::ostream& os, int test_id);	// test_id为待测视频编号（帧编号前20位）
	std::vector<std::string> xml_results(int test_id, int slice_len);
	void delete_results(int test_id);
	void clear() { results_.clear(); }

private:
	int slice_thres_;
	std::map<long long, int> results_;			// 使用offset_id作为key
};

class VideoMatcher {
public:
	VideoMatcher(const VideoLibVec& lib) : lib_(lib) {}
	// 单个视频匹配
	void match_video(int video_id, const std::string& video_file, int frame_start = 0, int frame_end = 0);
	void match_video(int video_id, const LibPointVec& video_points, int frame_start = 0, int frame_end = 0);
	void match_frames(const LibPointVec& frames, int frame_begin);
//	void gather_section(const MatchResult& res);
	void print_result();
	const MatchResults& results() const { return sec_man_.results(); }
	void clear() { sec_man_.clear(); }

private:
	const VideoLibVec& lib_;
	SectionManager sec_man_;
};

#endif // MATCH_UTILS_HPP
