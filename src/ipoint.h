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

#ifndef IPOINT_H
#define IPOINT_H

#include <vector>
#include <utility>	// pair

//-------------------------------------------------------

struct Ipoint {
	Ipoint() : orientation(0), clusterIndex(0), match(0) {};
	Ipoint(float xx, float yy, float ss, int ll)
		: x(xx), y(yy), scale(ss), laplacian(ll), orientation(0), clusterIndex(0), match(0) {};

	float x, y;				//! Coordinates of the detected interest point
	float scale;			//! Detected scale
	float orientation;		//! Orientation measured anti-clockwise from +ve x-axis
	float sin_ori, cos_ori;	//! sin(orientation) and cos(orientation)
	int laplacian;			//! Sign of laplacian for fast matching purposes
	float descriptor[64];	//! Vector of descriptor components
	float dx, dy;			//! Placeholders for point motion
	int clusterIndex;		//! Used to store cluster index
	const void* match;		//! matched ipoint
};

struct LibIpt {				// Simplified ipoint for surf library
	float descriptor[64];	//! Vector of descriptor components
};

typedef std::vector<Ipoint> IpVec;
typedef std::vector<LibIpt> LibIpVec;

struct EDist {		// Euclidean distance
	template<typename I>
	float operator () (const Ipoint& lhs, const I& rhs) const {
		float sum = 0.f;
		for(int i = 0; i < 64; ++i) {
			float dd = lhs.descriptor[i] - rhs.descriptor[i];
			sum += dd * dd;
		}
		return sum;		// sqrt(sum);
	};
	float threshold() const { return 0.42; }
};

struct MDist {		// Manhattan distance
	template<typename I>
	float operator () (const Ipoint& lhs, const I& rhs) const {
		float sum = 0.f;
		for(int i = 0; i < 64; ++i) {
//			float dd = lhs.descriptor[i] - rhs.descriptor[i];
			sum += fabs(lhs.descriptor[i] - rhs.descriptor[i]);
//			if (lhs.descriptor[i] > rhs.descriptor[i])
//				sum += lhs.descriptor[i] - rhs.descriptor[i];
//			else
//				sum += rhs.descriptor[i] - lhs.descriptor[i];
		}
		return sum;
	};
	float threshold() const { return 0.6; }
};

template<typename D = EDist, typename I = Ipoint>
class Matches {
public:
	typedef std::vector<I> IpVec2;

	Matches() {}
	Matches(IpVec& ipts1, IpVec2& ipts2) : ipts1_(&ipts1), ipts2_(&ipts2) {}

	void Run(bool mt = true);
	int size() const { return matches_.size(); }
	const Ipoint& ipt1(int i) const { return *matches_[i].first;  }
	const I& ipt2(int i) const { return *matches_[i].second; }

private:
	void matchThread(int tid = -1);
	void storeMatch(Ipoint* ipt)
	{ matches_.push_back(std::make_pair(ipt, (const I*)ipt->match)); }

	D dist;
	IpVec*  ipts1_;
	IpVec2* ipts2_;
	std::vector<std::pair<const Ipoint*, const I*> > matches_;
};

//-------------------------------------------------------


#endif
