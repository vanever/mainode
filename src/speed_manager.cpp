#include "speed_manager.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>
#include <string>

#include "comm_arg.hpp"
#include "log.h"
#include "utils.h"

using namespace std;

SpeedManager::SpeedManager (const char * file)
	: idx_(0)
	, interval_(500)	// default 500 * 20ms
	, weight_(1)
{
	if (file == 0)
	{
		speedlist_.push_back(CommArg::comm_arg().packet_pause);
	}
	else
	{
		using namespace boost::property_tree;
		ptree pt;
		read_info(file, pt);
		interval_ = pt.get<unsigned>("interval");
		weight_ = pt.get<unsigned>("weight");
		string speeds = pt.get<string>("speedlist");
		vector<string> speed_vec;
		boost::split(speed_vec, speeds, boost::is_any_of(" \t"));
		foreach (const string & speed, speed_vec)
		{
			if (speed.size())
				speedlist_.push_back(boost::lexical_cast<unsigned>(speed));
		}
		ASSERT(speedlist_.size(), "empty speed");
		FDU_LOG(DEBUG) << "speedlist: " << speeds;
	}

}

unsigned SpeedManager::get_next_pause_time() const
{
	idx_ = next(idx_);
	return speedlist_[idx_];
}
