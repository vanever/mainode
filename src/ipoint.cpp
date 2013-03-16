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

#include <float.h>
#include "utils.h"
#include "ipoint.h"

template<typename D, typename I>
void Matches<D, I>::matchThread(int tid) {
	Range rng = threadRange(ipts1_->size(), tid);
	for(int i = rng.begin; i < rng.end; i++) {
		Ipoint& ipt1 = (*ipts1_)[i];
		const I* match;
		float d, d1 =FLT_MAX, d2 = FLT_MAX;
		for(typename IpVec2::iterator it = ipts2_->begin(); it != ipts2_->end(); ++it) {
			I& ipt2 = *it;
			d = dist(ipt1,ipt2);  

			if(d < d1) {		// if this feature matches better than current best
				d2 = d1;
				d1 = d;
				match = &ipt2;
			}
			else if(d < d2) // this feature matches better than second best
				d2 = d;
		}
		// If match has a d1:d2 ratio < 0.65 ipoints are a match
		if(d1 < d2 * dist.threshold())
			ipt1.match = match;
	}
}

#include <boost/thread.hpp>
using namespace boost;

//! Populate matches_ with matched ipts 
template<typename D, typename I>
void Matches<D, I>::Run(bool mt) {
//	if (ipts1_->size() > ipts2_->size())
//		std::swap(ipts1_, ipts2_);

	if (mt) {
		thread_group tg;
		for(int tid = 0; tid < THREADS; ++tid)
			tg.create_thread(bind(&Matches::matchThread, this, tid));
		tg.join_all();
	}
	else
		matchThread();

	for(IpVec::iterator it = ipts1_->begin(); it != ipts1_->end(); ++it) {
		if(it->match) {
			// Store the change in position
			storeMatch(&*it);
			it->match = 0;
		}
	}
}

template<>
void Matches<EDist, Ipoint>::storeMatch(Ipoint* ipt) {
	const Ipoint* match = (const Ipoint*)ipt->match;
	ipt->dx = match->x - ipt->x; 
	ipt->dy = match->y - ipt->y;
	matches_.push_back(std::make_pair(ipt, match));
}

template<>
void Matches<MDist, Ipoint>::storeMatch(Ipoint* ipt) {
	const Ipoint* match = (const Ipoint*)ipt->match;
	ipt->dx = match->x - ipt->x; 
	ipt->dy = match->y - ipt->y;
	matches_.push_back(std::make_pair(ipt, match));
}

// explicitly instantiate Matches
template class Matches<EDist, Ipoint>;
template class Matches<MDist, Ipoint>;
template class Matches<EDist, LibIpt>;
template class Matches<MDist, LibIpt>;
