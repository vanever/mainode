#include <boost/bind.hpp>
#include "server.hpp"
#include <fstream>
#include <set>
#include <iomanip>
#include <boost/thread.hpp>
#include <boost/format.hpp>
#include <boost/date_time.hpp>
#include <boost/timer/timer.hpp>
#include "comm_arg.hpp"
#include "comm_para.hpp"
#include "match_utils.h"
#include "match_stream.hpp"
#include "connection_build_service.hpp"
#include "lib_load_service.hpp"
#include "packet_handle_center.hpp"
#include "command_recv_service.hpp"
#include "mini_timer.hpp"

#include <opencv/cv.h>
#include <opencv/highgui.h>

using namespace std;
using namespace boost;

Server * Server::server_ = 0;

void Server::start()
{
}

void Server::start_main()
{
	open_services();
	io_.run();	// start IO
}

void Server::open_services()
{
	//! open receiving
	socket_.async_receive_from(recv_buffer_, end_point_,
			boost::bind(&Server::recv_diverse_handler, this, _1, _2));

	//! ConnectionBuilderService
	boost::thread t1(
			boost::bind(&ConnectionBuilderService::run, &ConnectionBuilderService::instance())
			);
	t1.detach();

	//! LibLoadService
	boost::thread t2(
			boost::bind(&LibLoadService::run, &LibLoadService::instance())
			);
	t2.detach();

	//! PacketHandleCenter
	boost::thread t3(
			boost::bind(&PacketHandleCenter::run, &PacketHandleCenter::instance())
			);
	t3.detach();

	//! CommandRecvService
	boost::thread t4(
			boost::bind(&CommandRecvService::run, &CommandRecvService::instance())
			);
	t4.detach();
}

void Server::recv_diverse_handler(const boost::system::error_code& ec, size_t length)
{
	if (ec)
	{
		ASSERT(0, ec.message());
		return;
	}

	if (all_comm_end())
	{
		return;
	}

//	disable filter
	if ( !MatcherManager::instance().exists(end_point_) )
	{	// filter 
		FDU_LOG(DEBUG) << "filter packet from " << end_point_;
		socket_.async_receive_from(recv_buffer_, end_point_,
				boost::bind(&Server::recv_diverse_handler, this, _1, _2));
		return;
	}

	recv_pkt_.set_total_len( length + UDP_HEAD_SIZE );	// need confirm
	recv_pkt_.set_from_point(end_point_);
//	FDU_LOG(DEBUG) << "got " << (boost::format("%02x") % (int)recv_pkt_.type()) << " pkt, queue size: " << packet_queue()->num_elements();
	Server::instance().packet_queue()->put(Packet::make_shared_packet(recv_pkt_));
	socket_.async_receive_from(recv_buffer_, end_point_,
			boost::bind(&Server::recv_diverse_handler, this, _1, _2));
}

void dummy_handler(const boost::system::error_code& ) {}

void Server::send_handler(const boost::system::error_code& ec, size_t length)
{
	if (ec) {
		ASSERT(0, ec.message());
	}
	else {
		// do nothing
	}
}

namespace {
	void dump_packets( Packet & pkt, ostream & out ) {
		static unsigned cnt = 0;
		out << "dump " << ++cnt << " packet" << endl;
		u_char * pd = pkt.data_ptr();
		for (int i = 0; i < pkt.data_len(); ++i) {
			out << boost::format("%02x ") % (int)(pd[i]);
			if ( (i + 1) % 16 == 0 )
				out << endl;
		}
		out << endl;
	}
	void dump_packets( char * pc, unsigned len, ostream & out ) {
		static unsigned cnt = 0;
		out << "dump " << ++cnt << " packet" << endl;
		u_char * pd = (u_char *)pc;
		for (int i = 0; i < len; ++i) {
			out << boost::format("%02x ") % (int)(pd[i]);
			if ( (i + 1) % 16 == 0 )
				out << endl;
		}
		out << endl;
	}
}

void Server::send( const boost::asio::mutable_buffers_1 & buffer, const udp::endpoint & dest )
{
	static boost::mutex monitor;
	boost::mutex::scoped_lock lk(monitor);

// #define DUMP_ALL_SEND_PACKET
#ifdef DUMP_ALL_SEND_PACKET
	static ofstream out("all_sent_pkts");
	if (out) {
		char * pc    = boost::asio::buffer_cast<char *>(buffer);
		unsigned len = boost::asio::buffer_size(buffer);
		dump_packets(pc, len, out);
	}
#endif

	send_sock_.async_send_to( buffer, dest,
			//			boost::bind(&Server::send_handler, this, _1, _2)
			boost::bind(dummy_handler, _1)
			);
}

bool Server::all_comm_end() const
{
	return all_comm_end_;
}

void Server::mark_all_comm_end()
{
	all_comm_end_ = true;
	packet_queue()->notify_all_put();
	command_queue()->notify_all_put();
	command_window()->notify_all_put();
	ConnectionBuilderService::instance().notify_wait_matcher();
	LibLoadService::instance().notify_wait_matcher();
	CommandRecvService::instance().stop_io_service();
	io_.stop(); // stop receive handler
	MiniTimer::instance().stop();
	c_wait_.notify_one();
}
