#include <iostream>
#include <fstream>

#include "newlib.h"
#include "log.h"

extern "C" {
	unsigned short ntohs(unsigned short);
}

using namespace std;

int saveLibNew( const VideoLibRec& video_rec, std::ostream& of )
{
	unsigned int sync = SYNCWORD;
	of.write((char*)&sync, sizeof(int));
	of.write((char*)&video_rec.video_num, sizeof(short));
	of.write((char*)&video_rec.num_of_frames, sizeof(short));
	for (int i = 0; i < video_rec.point_vec.size(); ++i) {
		of.write((char*)&video_rec.point_vec[i], video_rec.point_vec[i].nbytes());
	}
	return 0;
}

int saveLibNew( const VideoLibRec& video_rec, const std::string& file_name )
{
	ofstream outfile(file_name.c_str(), ios_base::app | ios_base::binary);
	if(!outfile) {
		FDU_LOG(ERR) << "Can't open " << file_name << " file for output.";
		return -1;
	}
	return saveLibNew(video_rec, outfile);
}

void VideoLibVec::store(unsigned int word, bool from_net) {
	switch (state_) {
	case SYNC:
		if (word == SYNCWORD)
			++state_;
		else if (word != SYNCWORD_END || word != FILLWORD)
			FDU_LOG(WARN) << "syncword expected";
		break;
	case NUM:
		if (word == SYNCWORD)
			FDU_LOG(WARN) << "unexpected syncword in NUM state";
		else {
			VideoLibRec rec;
			if (from_net) {
				rec.video_num = ntohs(word & 0xFFFF);
				rec.num_of_frames = ntohs((word >> 16) & 0xFFFF);
			}
			else {
				rec.video_num = word & 0xFFFF;
				rec.num_of_frames = (word >> 16) & 0xFFFF;
			}
//			FDU_LOG(DEBUG) << "video_num: " << rec.video_num << "num_of_frames: " << rec.num_of_frames;
			if (rec.num_of_frames > 0) {
				np_ = nw_ = 0;
				point_.clear();
				push_back(rec);
				++state_;
			}
			else {
				FDU_LOG(WARN) << "empty video";
				state_ = SYNC;
			}
		}
		break;
	case DESC:
		point_.words[nw_] = word;
		if (++nw_ == point_.nwords()) {
			nw_ = 0;
			back().point_vec.push_back(point_);
			++total_frames_;
			if (++np_ == back().num_of_frames)
				state_ = SYNC;
		}
	}
}

int loadLibNew( int nbits, VideoLibVec& video_lib, const string& lib_file )
{
	FDU_LOG(INFO) << "loading new lib " << lib_file;
	ifstream infile(lib_file.c_str(), ios_base::binary);
	if(!infile) {
		FDU_LOG(ERR) << "Can't open " << lib_file << " file for input.";
		return -1;
	}

	video_lib.reset(nbits);
	unsigned int buf;
	while ( infile.read((char*)&buf, sizeof(int)) )
		video_lib.store(buf);
	if (!video_lib)
		FDU_LOG(WARN) << "unexpected file end";

	FDU_LOG(INFO) << "total num of videos: " << video_lib.size();
	FDU_LOG(INFO) << "total num of frames: " << video_lib.total_frames();

	return video_lib.total_frames();
}
