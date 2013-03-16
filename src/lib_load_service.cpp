#include "lib_load_service.hpp"
#include "match_stream.hpp"
#include "server.hpp"

LibLoadService::LibLoadService()
	: lib_valid_( false )
{
}

LibLoadService::LibLoadService(const LibLoadService & l)
{
}

void LibLoadService::load_lib_to_mem(const std::string & path)
{
	lock lk(monitor_);
	ASSERT(!lib_valid_, "lib already load");
	loadLibNew(CommArg::comm_arg().nbits, video_lib_, path);
	lib_valid_ = true;
	c_wait_.notify_one();
}

void LibLoadService::load_lib_to_matcher( MatcherPtr m )
{
	Server::instance().surf_lib_window()->reset();
	FDU_LOG(INFO) << "begin send libs to " << m->to_endpoint();
	MatcherManager::instance().set_matcher_state( *m, Matcher::LIB_LOADING );
	StreamLine send_lib_line;
	NewLibLoader loader;
	NewLibSender sender(m->to_endpoint());
	send_lib_line.add_node( &loader );
	send_lib_line.add_node( &sender );
	send_lib_line.start_running_stream();

	send_lib_line.wait_stream_end();
	FDU_LOG(INFO) << "successfully send libs to " << m->to_endpoint();
}

bool LibLoadService::end()
{
	bool server_end = Server::instance().all_comm_end();
	bool no_matcher_wait_lib = !MatcherManager::instance().find_one_matcher_at_state(Matcher::WAIT_LIB_LOAD);
	return server_end && no_matcher_wait_lib;
}

void LibLoadService::notify_wait_matcher()
{
	lock lk(monitor_);
	c_wait_.notify_one();
}

void LibLoadService::run()
{
	lock lk(monitor_);
	while (!end())
	{
		MatcherPtr m;
		while ( !(m = MatcherManager::instance().find_one_matcher_at_state(Matcher::WAIT_LIB_LOAD)) || !lib_valid_ )
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
		load_lib_to_matcher( m );
	}
	FDU_LOG(INFO)
		<< "\n---------------------------------"
		<< "\n-- LibLoadService close"
		<< "\n---------------------------------";
}
