/// @file match_stream.hpp
///
/// CFA communication Stream Line
///
/// you need to notice that two special nodes:
/// extracter and matcher, which is implemented by FPGA,
/// they cannot accept blocking communications
///
/// that's to say you cannot perform "put" operation when they are busy
/// and you must be capable of receiving anything from them immediately
/// as main thread is used to receive all packets
/// therefore main thread should not be blocked
/// you need to be careful of performing "put" operation in main thread
/// 
/// @author Chaofan Yu
/// @version 1.0
/// @date 2012-11-26

#ifndef MATCH_CHAIN_HPP
#define MATCH_CHAIN_HPP

#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/thread_time.hpp>
#include "packet.hpp"
#include <boost/thread.hpp>
#include <string>
#include "matcher_manager.hpp"
#include <fstream>
#include "lib_load_service.hpp"
#include "app_manager.hpp"

class Server;

typedef boost::asio::ip::udp::endpoint endpoint;

/// Stream node in stream line
class StreamNode
{

public:

	StreamNode()
	{
	}

	virtual ~StreamNode() {}

	/// thread to run
	///
	/// template method
	/// to avoid cases that user forget to notify next node
	void run()
	{
		do_run_task();
		do_end_task();
	}

	/// run task
	virtual void do_run_task () = 0;

	/// end task
	/// often implemented as notifying box that all data are put
	virtual void do_end_task () = 0;

};

/// StreamLine
class StreamLine
{

public:

	~StreamLine()
	{
		wait_stream_end();
	}

	void add_node(StreamNode * node)
	{
		nodes_.push_back( node );
	}

	void start_running_stream()
	{
		foreach (StreamNode * node, nodes_)
			tg.create_thread(boost::bind(&StreamNode::run, node));
	}

	void wait_stream_end()
	{
		tg.join_all();
	}

private:

	boost::thread_group tg;
	std::vector< StreamNode * > nodes_;

};

class NewLibLoader : public StreamNode
{

public:

	NewLibLoader (const VideoLibVec & lib);

	virtual void do_run_task ();
	virtual void do_end_task ();

private:

	void load_lib( const VideoLibVec & file );
	const VideoLibVec & lib_;

};

//--------------------------------------------------------------------------------------- 

/// Surf Lib Points Broadcaster
/// get packets from broad_box and broad it out
class NewLibSender : public StreamNode
{

public:

	NewLibSender( const endpoint & dest );

	virtual void do_run_task ();
	virtual void do_end_task ();

private:

	void send_packets();
	const endpoint dest_;

};

//--------------------------------------------------------------------------------------- 

//----------------------------------------------------------------------------------

class ConnectionBuilder : public StreamNode
{

public:

	typedef boost::mutex::scoped_lock lock;

//	ConnectionBuilder ( const endpoint & dest );
	ConnectionBuilder ( MatcherPtr m );
	~ConnectionBuilder();

	virtual void do_run_task ();
	virtual void do_end_task ();

	void set_built(bool b);

	static ConnectionBuilder * get_build_task(const endpoint & e )
	{
		std::map<endpoint, ConnectionBuilder *>::iterator it = conn_task_.find(e);
		if (it != conn_task_.end())
		{
			return it->second;
		}
		else
			return 0;
	}

private:

	void do_build_connection(const endpoint & e);

	const endpoint dest_;
	int num_timeout_;
	bool built_;
	Packet pkt_;
	boost::mutex monitor_;
	boost::condition c_wait_;

	static std::map<endpoint, ConnectionBuilder *> conn_task_;

};

//---------------------------------------------------------------------------------- 
// 

class BitFeatureLoader : public StreamNode
{

public:

	BitFeatureLoader(AppDomain *);

	virtual void do_run_task ();
	virtual void do_end_task ();

private:

	void load_loop(const VideoLibVec & videos);
	void load_section(unsigned short vid, unsigned short frompos, const BitFeature * bit_vec, unsigned size, MatcherPtr matcher, unsigned slice_idx);

	std::size_t image_cnt_;
	AppDomain * app_;
	VideoLibVec vec_;

};

class SpeedManager;

class BitFeatureSender : public StreamNode
{

public:

	BitFeatureSender (AppDomain *);
	~BitFeatureSender();

	virtual void do_run_task ();
	virtual void do_end_task ();

	void set_dest_addr( const endpoint & addr );
	const endpoint dest_addr() const;
	void update_pause_time();
	unsigned pause_time() const { return paused_time_; }
	void change_update_interval(unsigned v);

private:

	void send_packets_loop();

	AppDomain * app_;
	SpeedManager * speed_manager_;
	endpoint dest_addr_;
	unsigned paused_time_;
	unsigned last_task_id_;

};

#endif
