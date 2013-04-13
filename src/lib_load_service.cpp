#include "lib_load_service.hpp"
#include "match_stream.hpp"
#include "server.hpp"
#include <boost/format.hpp>

using namespace std;

LibLoadService::LibLoadService()
	: lib_valid_( false )
{
}

LibLoadService::LibLoadService(const LibLoadService & l)
{
}

void LibLoadService::load_domain_lib_to_mem(DomainType d)
{
	libmap_.insert(make_pair(d, VideoLibVec()));
	if (AppDomainPtr pa = AppManager::instance().get_domain_by_appid(d))
	{
		load_lib_to_mem(pa->lib_file(), libmap_[d]);
	}
	else
		FDU_LOG(ERR) << __func__ << " App not found: " << d;
}

// Toto: load lib for each app
void LibLoadService::load_lib_to_mem(const std::string & path, VideoLibVec & lib)
{
	unsigned nbits = CommArg::comm_arg().nbits;
//	loadLibNew(nbits, lib, (boost::format(CommArg::comm_arg().path_format) % path % nbits).str());
	loadLibNew(nbits, lib, path);

	// add a dummy rec with code = 0 && contents = 0
	// it's used for judging whether the result should be calculated
	VideoLibRec rec;
	rec.video_num = 0x00000000;
	rec.num_of_frames = 32;	// max merge unit
	for (int i = 0; i < rec.num_of_frames; i++)
	{
		BitFeature bf(nbits);
		bf.clear();
		rec.point_vec.push_back(bf);
	}
	lib.push_back( rec );
}

void LibLoadService::load_lib_to_matcher( MatcherPtr m, const VideoLibVec & lib )
{
	Server::instance().surf_lib_window()->reset();
	FDU_LOG(INFO) << "begin send libs to " << m->to_endpoint();
	MatcherManager::instance().set_matcher_state( *m, Matcher::LIB_LOADING );
	StreamLine send_lib_line;
	NewLibLoader loader(lib);
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
		while ( !(m = MatcherManager::instance().find_one_matcher_at_state(Matcher::WAIT_LIB_LOAD)) )
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
		LibMap::const_iterator it = libmap_.find(m->domain_id());
		while ((it = libmap_.find(m->domain_id())) == libmap_.end())
		{
			load_domain_lib_to_mem(m->domain_id());
		}
		load_lib_to_matcher( m, it->second );
	}
	FDU_LOG(INFO)
		<< "\n---------------------------------"
		<< "\n-- LibLoadService close"
		<< "\n---------------------------------";
}
