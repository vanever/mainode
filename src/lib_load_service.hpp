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
	typedef DomainType AppId;
	typedef std::map<DomainType, VideoLibVec> LibMap;

	void run();

	void notify_wait_matcher();

	static LibLoadService & instance()
	{
		static LibLoadService c;
		return c;
	}

	void load_domain_lib_to_mem(DomainType d);

private:

	LibLoadService();

	LibLoadService(const LibLoadService &);

	bool end();
	void load_lib_to_matcher( MatcherPtr m, const VideoLibVec & lib );
	void load_lib_to_mem(const std::string & path, VideoLibVec & lib);

	bool lib_valid_;
	LibMap libmap_;
	boost::mutex monitor_;
	boost::condition c_wait_;

};

#endif
