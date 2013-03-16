#ifndef BitFeature_H
#define BitFeature_H

#include <algorithm>
#include <iostream>
#include <bitset>

#include "integral.h"
#include "log.h"

#define MIN_FEATURE_BITS  32
#define MAX_FEATURE_BITS 128

class BitFeature {
public:
	typedef unsigned char Byte;			// 8 bit
	typedef unsigned int  Word;			// 32 bit
	typedef unsigned long long DWord;	// 64 bit

	union {
		Byte   bytes[MAX_FEATURE_BITS/ 8];
		Word   words[MAX_FEATURE_BITS/32];
		DWord dwords[MAX_FEATURE_BITS/64];
	};

	explicit BitFeature(int nbits) { set_bits(nbits); }

	void set_bits(int n) {
		ASSERTS(n >= MIN_FEATURE_BITS && n <= MAX_FEATURE_BITS && ((n & (n - 1)) == 0));
		nwords_ = n / 32;
		clear();
	}

	void clear() { dwords[0] = dwords[1] = 0; }

	int nwords() const { return nwords_; }
	int nbytes() const { return nwords_ * 4; }

private:
	int nwords_;
};

inline BitFeature operator ^ (const BitFeature& lhs, const BitFeature& rhs) {
	ASSERTSD(lhs.nwords() == rhs.nwords());
	BitFeature result(lhs);
	result.dwords[0] ^= rhs.dwords[0];
	result.dwords[1] ^= rhs.dwords[1];
	return result;
}

inline std::ostream& operator << (std::ostream& os, const BitFeature& f) {
	for (int i = 0; i < f.nwords(); ++i)
		os << std::bitset<32>(f.words[i]);
	return os;
}

BitFeature order_feature(int nbits, const GrayImage& gimg);
BitFeature order_feature(int nbits, const IplImage* img);

int hamming_distance(const BitFeature& f1, const BitFeature& f2);

#endif