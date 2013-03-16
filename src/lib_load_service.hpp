#ifndef LIB_LOAD_SERVICE_HPP
#define LIB_LOAD_SERVICE_HPP

#include "matcher_manager.hpp"
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/thread_time.hpp>
#include "packet.hpp"
#include "newlib.h"

class LibLoadService
{

public:

	typedef boost::mutex::scoped_lock lock;

	void run();

	void notify_wait_matcher();

	static LibLoadService & instance()
	{
		static LibLoadService c;
		return c;
	}

	const VideoLibVec & video_lib()
	{
		ASSERT(lib_valid_, "lib not ready");
		return video_lib_;
	}

	void load_lib_to_mem(const std::string & path);

private:

	LibLoadService();
	LibLoadService(const LibLoadService &);
	bool end();
	void load_lib_to_matcher( MatcherPtr m );

	bool lib_valid_;
	VideoLibVec video_lib_;
	boost::mutex monitor_;
	boost::condition c_wait_;

};

#endif
