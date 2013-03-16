#ifndef BITS_HPP
#define BITS_HPP

#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <boost/format.hpp>
#include "log.h"

// template<unsigned int N> // N = 8 * m, m is integer
struct bits
{
	static const unsigned char MASK = 0x07;
	static const unsigned char SHIFT = 3;

	bits(int n) { set_size(n); clr(); }
	bits(unsigned char * p, int len) { set_size(len << SHIFT); memcpy(byte_ptr(), p, len); }
	void clr (int i) { ASSERTS(i < N);        bytes[i >> SHIFT] &= ~(0x80 >> (i & MASK)); }
	void set (int i) { ASSERTS(i < N);        bytes[i >> SHIFT] |=  (0x80 >> (i & MASK)); }
	bool test(int i) { ASSERTS(i < N); return bytes[i >> SHIFT] &   (0x80 >> (i & MASK)); }
	void clr()       { memset(byte_ptr(), 0x00, N >> SHIFT); }
	void set()       { memset(byte_ptr(), 0xFF, N >> SHIFT); }
	int to_int() {
		ASSERTS(N == 32);
		return *(int *)byte_ptr();
	}
	std::string to_string() {
		std::string s;
		for (int i = 0; i < (N >> SHIFT); ++i)
			s += (boost::format("%02x ") % (int)bytes[i]).str();
		return s;
	}

	void set_size(int n)
	{
		N = n;
		ASSERTS(N > 0 && N % 8 == 0);
		bytes.resize(N >> SHIFT);
	}

	unsigned char * byte_ptr() { return &bytes[0]; }

	int N;

private:

	std::vector< unsigned char > bytes;

};

#endif
