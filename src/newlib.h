#ifndef NEWLIB_H
#define NEWLIB_H

#include <iosfwd>
#include <vector>

#include "bit_feature.h"
#include "comm_para.hpp"

// #define SYNCWORD 0xFADCCAFD
// // #define ENDCWORD 0xDFACCDAF
// #define ENDCWORD 0xAFCDACDF
#define FILLWORD 0xFFFFFFFF

typedef std::vector<BitFeature> LibPointVec;

struct VideoLibRec {
	short video_num;
	short num_of_frames;
	LibPointVec point_vec;
};

struct VideoLibVec : std::vector<VideoLibRec> {
	VideoLibVec(int nbits = MAX_FEATURE_BITS) :
		 point_(nbits), state_(SYNC), total_frames_(0) {}

	void reset(int nbits) {
		point_.set_bits(nbits);
		state_ = SYNC;
		total_frames_ = 0;
		clear();
	}

	void store(unsigned int word, bool from_net = false);
	
	operator   bool () const { return state_ == SYNC; }	// Õý³£×´Ì¬
	bool  operator !() const { return state_ != SYNC; }	// Òì³£×´Ì¬
	int total_frames() const { return total_frames_; }

private:
	enum State { SYNC, NUM, DESC };

	BitFeature point_;
	int state_, np_, nw_;
	int total_frames_;
};

int saveLibNew(const VideoLibRec& video_rec, std::ostream& of);
int saveLibNew(const VideoLibRec& video_rec, const std::string& file_name);

int loadLibNew(int nbits, VideoLibVec& video_lib, const std::string& lib_file);

#endif
