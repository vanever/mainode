#ifndef SPEED_MANAGER_HPP
#define SPEED_MANAGER_HPP

#include <vector>

class BitFeatureSender;

class SpeedManager
{

public:

	explicit SpeedManager (const char * file = 0);

	unsigned get_next_pause_time() const;
	unsigned original_interval() const { return interval_; }
	void set_weight(unsigned v);

private:

	unsigned next(unsigned idx) const { return (idx+1) % speedlist_.size(); }

	mutable unsigned idx_;
	unsigned interval_;
	unsigned weight_;
	std::vector< unsigned > speedlist_;

};

#endif
