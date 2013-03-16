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
	int feature_bits;		// ��������
	int frame_thres;		// ��֡ƥ����ֵ
//	int sequence_thres;		// ����ƥ��εĺϲ����ò�����ƴ���㷨Ӱ��̫�����ȡ��
	int min_match_len;		// ��С����ƥ��֡����ֵ
//	int max_skip;			// ��������������ƥ��֡�������·�������Ч
	int batch_len;		// ÿ��ƥ��֡��
	int overlap_len;		// ����������ص�֡��
//	int diag_test_bits;		// ÿ��ƥ�������Խ��ߣ��ж�λ�������·�������Ч
	int diag_test_thres;	// ÿ��ƥ�����ж���ֵ

	void load(const string& arg_file);
};

extern MatchArgs ARGS;

// =====================================================================================
// Utility functions

IplImage* transformImage(IplImage* img);

template <typename LIB>
int saveNewVideoLib(int nbits, const std::string& video_name, int video_num, LIB& lib);

struct CvCapture;

struct VideoCapcture {
	// capFps : ��֡���ʣ���Ϊ0�������Ƶfps������Ƶfps��ȡ��С��0���ʾ��ȡ�ؼ�֡
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

/* �·���ƥ����ֱ��ʹ��MatchSection����
struct MatchResult {
	int test_begin, lib_begin;
	int match_len;
	unsigned int match_bits;
	MatchResult(int tb, int lb, int ln, unsigned int bits) :
		test_begin(tb), lib_begin(lb), match_len(ln), match_bits(bits) {}
};
*/

// ������Ƶ��Ź���
// ��Ƶ�ţ�10λ��Ƭ�κţ���ʼλ�÷���������10λ��Ƭ����λ�ã��룩��12λ
// ��������Ƭ�γ���Ϊ68����
inline int test_num(int vid, int sid, int pos)
{ return (vid << 22) | (sid << 12) | pos; }

struct MatchSection {
	int test_begin, lib_begin;
	int len;
	MatchSection(int tb = 0, int lb = 0, int ln = 0) :
		test_begin(tb), lib_begin(lb), len(ln) {}
	int test_id() const { return test_begin >> 16; }	// ����Ƶ�ź�Ƭ�κ�
	int lib_id()  const { return lib_begin  >> 16; }
	int test_pos() const { return test_begin & 0xffff; }
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

// 2��ƥ��μ�ļ�����룬С��0�������ص�
inline int sec_dist(const MatchSection& lhs, const MatchSection& rhs) {
	return  lhs.test_begin < rhs.test_begin ?
			rhs.test_begin - lhs.test_begin - lhs.len :
			lhs.test_begin - rhs.test_begin - rhs.len;
}

extern std::ostream& operator << (std::ostream& os, const MatchSection& sec);

typedef std::pair<int, int> MatchResult;		// �򻯵�ƥ������(������Ƶ���, ����Ƶ���)
typedef std::set<MatchResult> MatchResults;

class SectionManager {
public:
	typedef std::list<MatchSection> Chain;		// ��������
	typedef std::map<long long, Chain> ChainMap;

	void add_section(const MatchSection& sec);

	const MatchResults& gather_results();
	const MatchResults& results() const { return results_; }

	void output_chains(std::ostream& os, int test_id);
	void delete_chains(int test_id);
	void clear() { chains_.clear(); }

private:
	std::map<long long, Chain> chains_;			// ʹ��offset_id��Ϊkey
	MatchResults results_;
};

class VideoMatcher {
public:
	VideoMatcher(const VideoLibVec& lib) : lib_(lib) {}
	// ������Ƶƥ��
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
