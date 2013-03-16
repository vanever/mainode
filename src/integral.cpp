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

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <memory.h>

//#include "utils.h"
#include "integral.h"

//-------------------------------------------------------

void GrayImage::init(const IplImage* img) {
	if (!img) return;

	IplImage* gray8 = cvCreateImage( cvGetSize(img), IPL_DEPTH_8U, 1 );

	if( img->nChannels == 1 )
		cvConvert( img, gray8 );
	else 
		cvCvtColor( img, gray8, CV_BGR2GRAY );

	reset(gray8->width, gray8->height);
	char* psrc = gray8->imageData;
	unsigned char* pdst = data_;
	for (int row = 0; row < height(); ++row) {
		memcpy(pdst, psrc, width());
		psrc += gray8->widthStep;
		pdst += width();
	}
	cvReleaseImage( &gray8 );
}

void GrayImage::load(const char* file_name) {
	IplImage* img = cvLoadImage(file_name);
	init(img);
	cvReleaseImage( &img );
}

//! Computes the integral image of image img
void IntImage::init(const GrayImage& img) {
	const int maxWidth = 960;
	int scale = 1;
	while (maxWidth * scale < img.width())
		scale *= 2;

	Image<int>::reset(img.width() / scale, img.height() / scale);

	for (int i = 0; i < height(); ++i)
		for (int j = 0, rs = 0; j < width(); j++) {
			for (int k = 0; k < scale; ++k)
				for (int l = 0; l < scale; ++l)
					rs += img(i * scale + k, j * scale + l);
			pixel(i, j) = rs / (scale*scale) + (i > 0 ? pixel(i - 1, j) : 0);
		}
}
