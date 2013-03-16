#include "connection_build_service.hpp"
#include "match_stream.hpp"
#include "server.hpp"

void ConnectionBuilderService::connect_matcher( MatcherPtr m )
{
	ConnectionBuilder builder(m);
	boost::thread t( boost::bind(&ConnectionBuilder::run, &builder) );
	t.join();
}

bool ConnectionBuilderService::end()
{
	bool server_end = Server::instance().all_comm_end();
	bool no_matcher_wait_conn = !MatcherManager::instance().find_one_matcher_at_state(Matcher::UNCONNECTED);
	return server_end && no_matcher_wait_conn;
}

void ConnectionBuilderService::notify_wait_matcher()
{
	lock lk(monitor_);
	c_wait_.notify_one();
}

void ConnectionBuilderService::run()
{
	lock lk(monitor_);
	while (!end())
	{
		MatcherPtr m;
		while ( !(m = MatcherManager::instance().find_one_matcher_at_state(Matcher::UNCONNECTED)) )
		{
			if (end())
			{
				FDU_LOG(INFO)
					<< "\n---------------------------------"
					<< "\n-- LibLoadService close"
					<< "\n---------------------------------";
				return;		// end
			}
			c_wait_.wait(lk);
		}
		connect_matcher( m );
	}
	FDU_LOG(INFO)
		<< "\n---------------------------------"
		<< "\n-- ConnectionBuilderService close"
		<< "\n---------------------------------";
}
