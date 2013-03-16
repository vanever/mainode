/*********************************************************** 
*  --- OpenSURF ---                                       *
*  This library is distributed under the GNU GPL. Please   *
*  use the contact form at http://www.chrisevansdev.com    *
*  for more information.                                   *
*                                                          *
*  C. Evans, Research Into Robust Visual Features,         *
*  MSc University of Bristol, 2008.                        *
*                                                          *
************************************************************/

#ifndef INTEGRAL_H
#define INTEGRAL_H

//#include <algorithm>  // std::min/max
//#include <boost/move/move.hpp>
// GrayImage提供了move支持，需要编译器支持C++11
//#include "utils.h"

// undefine VS macros
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

static const int INTSCALE = 256;

template<typename T> class Image {
public:
	Image() : width_(0), height_(0), data_(0) {}
	Image(int w, int h) : width_(w), height_(h), data_(new T[w*h]) {}

//	Image(Image&& x) : data_(x.data_), width_(x.width_), height_(x.height_)
//	{ x.data_ = 0; }
	~Image() { delete[] data_; }

//	Image& operator = (Image&& x) {
//		if (&x == this) return *this;
//		delete[] data_;
//		width_ = x.width_;
//		height_ = x.height_;
//		data_ = x.data_;
//		x.data_ = 0;
//	}

	void reset(int w, int h) { width_ = w; height_ = h; delete[] data_; data_ = new T[w*h]; }
	int width()  const { return width_;  }
	int height() const { return height_; }
	int size()   const { return width_ * height_; }
	bool operator!() const { return size() == 0;  }		// test image validation
	T  operator()(int row, int col) const { return pixel(row, col); }	// get the pixel
	T& operator()(int row, int col)       { return pixel(row, col); }	// get/set the pixel

protected:
	T  pixel(int row, int col) const { return data_[row * width_ + col]; }	// get the pixel
	T& pixel(int row, int col)       { return data_[row * width_ + col]; }	// get/set the pixel

	T*  data_;              // Pointer to image data
private:
	int width_;             // Image width in pixels
	int height_;            // Image height in pixels
};

struct _IplImage;
typedef struct _IplImage IplImage;

class GrayImage : public Image<unsigned char> {
public:
	GrayImage() {}
	GrayImage(const IplImage* img) { init(img); }
	GrayImage(const char* file_name) { load(file_name); }

	void init(const IplImage* img);
	void load(const char* file_name);
};

struct IntImage : Image<int> {				// Integral image
	IntImage() {}
	IntImage(const GrayImage& src) { init(src); }
	void init(const GrayImage& src);

	int at(int r, int c) const {	// get the pixel with bound check
		if (r < 0 || c < 0) return 0;
		if (r >= height()) r = height() - 1;
		if (c >= width())  c = width() - 1;
		return pixel(r, c);
	}

	int BoxIntegral(int row, int col, int rows, int cols) const {
		// The subtraction by one for row/col is because row/col is inclusive.
		int r1 = row - 1;
		int c1 = col - 1;
		int r2 = row + rows - 1;
		int c2 = col + cols - 1;

		return at(r1, c1) - at(r1, c2) - at(r2, c1) + at(r2, c2);
	}
};

#endif
