#ifndef FUDANFPGA_UTILS_H_
#define FUDANFPGA_UTILS_H_

#include <string>
#include <vector>
#include <iostream>
#include <typeinfo>		// bad_cast
#include <math.h>

using std::string;
using std::size_t;


//-------------------------------------------------------

//! Round float to nearest integer
inline int fRound(float flt) { return (int) floor(flt+0.5f); }

// abs for int and float
template<typename T> inline T abs_(T x) { return x >= 0 ? x : -x; }


//-------------------------------------------------------
// Multi-thread support

extern int THREADS;

struct Range {
	int begin, end;
	Range(int e) : begin(0), end(e) {}
	Range(int b, int e) : begin(b), end(e) {}
};

inline Range threadRange(Range drng, int tid) {
	if (tid < 0) return drng;	// single thread
	// multi thread
	float slice = float(drng.end - drng.begin) / THREADS;
	return Range(drng.begin + fRound(slice * tid),
				 drng.begin + fRound(slice * (tid + 1)));
}

//-------------------------------------------------------
// Half-precision floating-point number

typedef unsigned short halfp;		// 16 bit
typedef unsigned int  uint32;		// 32 bit


// 简化的半精度转换，不考虑NaN和subnormal
static halfp single2half(float f) {
	uint32 x = *(uint32*)&f;
	halfp  s =  (x & 0x80000000u) >> 16;				// 符号
	int   es = ((x & 0x7F800000u) >> 23) - 127 + 15;	// 指数
	halfp  m =  (x & 0x007FFFFFu) >> 13;				// 尾数
	halfp  r =  (x >> 12) & 1;							// 舍入

	if (es <= 0)    return s;					// 下溢，返回零
	if (es >= 0x1F) return s | (0x1F << 10);	// 上溢，返回无穷大

	halfp e = es << 10;

	return (s | e | m) + r;
}

static float half2single(halfp h) {
	uint32 s = (h & 0x8000u) << 16;
	uint32 e = (h & 0x7C00u) >> 10;
	uint32 m = (h & 0x03FFu) << 13;

	if (e == 0x1F)   m = 0, e = 0xFFu;		// 无穷大
	else if (e == 0) m = 0;					// 零
	else             e += 127 - 15;			// 规格化数

	uint32 f = s | (e << 23) | m;

	return *(float*)&f;
}


//-------------------------------------------------------
// convert between enum & string

template <typename E>
class EnumStringMap {
	const char * const * _str_table;
	const int _size;
	const E   _def;
public:
	template <typename SA>
	explicit EnumStringMap(const SA& st, E default_value = (E)0)
		: _str_table(st), _size(sizeof(st)/sizeof(char*)), _def(default_value) {}

	E getEnum(const string& str) const {
		for (int i = 0; i < _size; ++i)
			if (str == _str_table[i])
				return str.empty() ? _def : static_cast<E>(i);
		throw std::bad_cast();
	}

	const char* getString(E val) const {
		return val == _def && (val < 0 || val >= _size) ? "" : _str_table[val];
	}

	E readEnum(std::istream& s) const {
		string str;
		s >> std::ws;
		if (!s.eof()) s >> str;		// empty input
		return getEnum(str);
	}

	std::ostream& writeEnum(std::ostream& s, E val) { return s << getString(val); }
};

#endif /*FUDANFPGA_UTILS_H_*/
