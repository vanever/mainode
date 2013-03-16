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

#ifndef UTILS_H
#define UTILS_H


//-----------------------------------------------------------------------
//

#ifndef foreach
#include <boost/foreach.hpp>
#define foreach(val, con) BOOST_FOREACH(val, con)
#endif

#include "comm_para.hpp"
#include "log.h"

typedef unsigned frame_type;

template< class T >
T & cast(void * p)
{
	return *(T *)p;
}

namespace UTILS
{

	template< class descriptor_type >
	struct image
	{
		image() : idx(0) {}

		struct point
		{
			descriptor_type descriptor[64];
		};

		frame_type frame_index;
		// int   frame_index;
		std::vector<point> points;

		unsigned video_id () const { return frame_index >> 24; }
		unsigned frame_pos() const { return frame_index & 0x00FFFFFF; }

		void swap(image & other)
		{
			if (this != &other)
			{
				std::swap(frame_index, other.frame_index);
				points.swap( other.points );
			}
		}

		void add_descriptor(descriptor_type v)
		{
			buff.descriptor[idx++] = v;
			if (idx == 64)
			{
				idx = 0;
				points.push_back(buff);
			}
		}

		void check_end_image()
		{
			ASSERTS(idx == 0);
		}

	private:

		int idx;
		point buff;

	};

};

typedef UTILS::image< lib_descriptor_type >   lib_image;
typedef UTILS::image< match_descriptor_type > match_image;

//-----------------------------------------------------------------

#include "fdutils.h"
#include "ipoint.h"

struct _IplImage;
typedef struct _IplImage IplImage;

//! Show the provided image and wait for keypress
void showImage(const IplImage *img);

//! Show the provided image in titled window and wait for keypress
void showImage(char *title, const IplImage *img);

//! Draw a single feature on the image
void drawIpoint(IplImage *img, const Ipoint &ipt, int tailSize = 0);

//! Draw all the Ipoints in the provided vector
void drawIpoints(IplImage *img, std::vector<Ipoint> &ipts, int tailSize = 0);

//! Draw descriptor windows around Ipoints in the provided vector
void drawWindows(IplImage *img, std::vector<Ipoint> &ipts);

// Draw the FPS figure on the image (requires at least 2 calls)
void drawFPS(IplImage *img);

//! Draw a Point at feature location
void drawPoint(IplImage *img, const Ipoint &ipt);

//! Draw a Point at all features
void drawPoints(IplImage *img, std::vector<Ipoint> &ipts);

//! Save the SURF features to file
void saveSurf(char *filename, std::vector<Ipoint> &ipts);
void saveSurf(std::ostream& outfile, std::vector<Ipoint> &ipts);

//! Load the SURF features from file
void loadSurf(char *filename, std::vector<Ipoint> &ipts);


#endif
