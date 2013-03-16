#ifndef CONNECTION_BUILD_SERVICE_HPP
#define CONNECTION_BUILD_SERVICE_HPP

#include "matcher_manager.hpp"
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/thread_time.hpp>
#include "packet.hpp"

class ConnectionBuilderService
{

public:

	typedef boost::mutex::scoped_lock lock;

	void run();

	void notify_wait_matcher();

	static ConnectionBuilderService & instance()
	{
		static ConnectionBuilderService c;
		return c;
	}

private:

	bool end();

	void connect_matcher( MatcherPtr m );

	boost::mutex monitor_;
	boost::condition c_wait_;

};

#endif
