#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include "comm_arg.hpp"
#include "packet.hpp"
#include "log.h"
#include "comm_box.hpp"
#include "comm_window.hpp"
#include "match_analyser.hpp"
#include "matcher_manager.hpp"
#include <boost/foreach.hpp>

using boost::asio::ip::udp;
using namespace boost::asio;
typedef boost::asio::ip::address addr_type;

class BitFeatureLoader  ;
class BitFeatureSender  ;

/// Server
/// 
/// Server class are responsible for:
/// build match stream lines
/// receive all kinds of packets
/// serve as a mediator for all kinds of communication boxes
class Server : public boost::noncopyable
{

public:

	Server (CommArg * arg)
		: arg_( arg )                                            
		, io_(  )                                                
		, local_point_( ip::address::from_string(arg->command_bind_ip), arg->server_port )
		, mega_point_( ip::address::from_string(arg->mega_bind_ip), arg->server_port )
		, ib_point_( ip::address::from_string(arg->ib_bind_ip), arg->server_port )
//		, socket_( io_, local_point_ )
		, mega_socket_( io_, mega_point_ )
		, ib_socket_( io_, ib_point_ )
//		, send_sock_( io_, udp::endpoint(ip::address::from_string(arg->bind_ip), arg->send_port) )
		, mega_send_sock_( io_, udp::endpoint(ip::address::from_string(arg->mega_bind_ip), arg->send_port) )
		, ib_send_sock_( io_, udp::endpoint(ip::address::from_string(arg->ib_bind_ip), arg->send_port) )
//		, deadline_( io_ )                                       
//		, recv_pkt_()                                         
		, mega_recv_pkt_()                                         
		, ib_recv_pkt_()                                         
		, mega_recv_buffer_( mega_recv_pkt_.to_udp_buffer() )                  
		, ib_recv_buffer_( ib_recv_pkt_.to_udp_buffer() )                  
//		, end_point_()
		, mega_remote_point_(  )                          
		, ib_remote_point_(  )                          

		, all_comm_end_          ( false )
		, bitfeature_all_sent_   ( false )
		, quit_wanted_           ( false )

		, surf_lib_window_     ()
		, bitfeature_window_   ()
		, command_window_      ()

        , bitfeature_loader_   ( 0 )
        , bitfeature_sender_   ( 0 )
	{
		FDU_LOG(DEBUG) << "server constructed";
	}

	void start();
	void start_main();
	void start_match_stream();

	//--------------------------------------------------------------------------------------- 
	// utils
	void send( const boost::asio::mutable_buffers_1 & buffer, const udp::endpoint & dest, MATCHER_TYPE type );

	void open_services();

	//----------------------------------------------------------------------------
	// interface for server to serve as a midiator for chain nodes

	boost::asio::io_service & io() { return io_; }

	const boost::asio::ip::address addr() const { return local_point_.address(); }
	unsigned short                 port() const { return local_point_.port(); }
	const udp::endpoint  local_endpoint() const { return local_point_; }

	CommArg * arg() { return arg_; }

	// external FPGA
	bool bitfeature_all_sent() const { return bitfeature_all_sent_; }
	void mark_bitfeatures_all_sent() { bitfeature_all_sent_ = true; }
	bool wanna_quit() const { return quit_wanted_; }

	bool all_comm_end() const;
	void mark_all_comm_end();

	//! send handler for all async_send_to
	//! @param   ec
	//! @param   length
	void send_handler(const boost::system::error_code& ec, size_t length);

	static Server & instance()
	{
		ASSERTS( server_ );
		return *server_;
	}

	static void construct_server( CommArg * arg )
	{
		ASSERTS( server_ == 0 );
		server_ = new Server( arg );
	}

	void wait_all_end()
	{
		boost::mutex::scoped_lock lk(monitor_);
		while (!all_comm_end())
			c_wait_.wait(lk);
	}

	//----------------------------------------------------------------------------
	// break encapsulation
	// however it's easy for me to do some change to code as server now is an midiator
	// I do not need to encapsulate a function for each couple-relationships
	// just use those members directly in code

	PacketQueue        * packet_queue()        { return &pkt_queue_; }
	CommandQueue       * command_queue()       { return &cmd_queue_; }
	SurfLibWindow      * surf_lib_window()     { return &surf_lib_window_; }	
	BitFeatureWindow   * bitfeature_window()   { return &bitfeature_window_; }
	CommandWindow      * command_window()      { return &command_window_; }

	BitFeatureLoader   * bitfeature_loader()   const { return bitfeature_loader_; }
	BitFeatureSender   * bitfeature_sender()   const { return bitfeature_sender_; }

private:

	//! trying receive all kinds of response or data after sending the first image
	//! @param   ec
	//! @param   length
//	void recv_diverse_handler(const boost::system::error_code& ec, size_t length);
	void mega_recv_diverse_handler(const boost::system::error_code& ec, size_t length);
	void ib_recv_diverse_handler(const boost::system::error_code& ec, size_t length);

	//----------------------------------------------------------------------------
	// basic
	CommArg * arg_;									   	// arg
	boost::asio::io_service io_;                        // io service
	boost::asio::ip::udp::endpoint local_point_;				
	boost::asio::ip::udp::endpoint mega_point_;				
	boost::asio::ip::udp::endpoint ib_point_;				
//	boost::asio::ip::udp::socket socket_;               // main thread socket
	boost::asio::ip::udp::socket mega_socket_;               
	boost::asio::ip::udp::socket ib_socket_;               
//	boost::asio::ip::udp::socket send_sock_;            
	boost::asio::ip::udp::socket mega_send_sock_;      
	boost::asio::ip::udp::socket ib_send_sock_;       
//	boost::asio::deadline_timer deadline_;           
//	Packet recv_pkt_;                               
	Packet mega_recv_pkt_;                         
	Packet ib_recv_pkt_;                          
//	boost::asio::mutable_buffers_1 recv_buffer_; 
	boost::asio::mutable_buffers_1 mega_recv_buffer_;        
	boost::asio::mutable_buffers_1 ib_recv_buffer_;        
//	boost::asio::ip::udp::endpoint end_point_;			
	boost::asio::ip::udp::endpoint mega_remote_point_;
	boost::asio::ip::udp::endpoint ib_remote_point_;

	bool all_comm_end_;									// all communicatioin end flag
	bool bitfeature_all_sent_;
	bool quit_wanted_;

	//---------------------------------------------------------------------------
	// comm boxes

	PacketQueue           pkt_queue_;
	CommandQueue          cmd_queue_;

	//----------------------------------------------------------------------------
	// packets windows

	SurfLibWindow         surf_lib_window_;				// window for sending surf lib points
	BitFeatureWindow      bitfeature_window_;			// window for sending bitfeature
	CommandWindow         command_window_;				// window for sending msg to DCSP

	//----------------------------------------------------------------------------
	// stream node

	BitFeatureLoader   * bitfeature_loader_;
	BitFeatureSender   * bitfeature_sender_;

	//--------------------------------------------------------------------------------------- 
	boost::mutex monitor_;
	boost::condition c_wait_;

	static Server * server_;

};

#endif
