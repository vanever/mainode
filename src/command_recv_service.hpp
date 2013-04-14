#ifndef COMMAND_RECV_SERVICE_HPP
#define COMMAND_RECV_SERVICE_HPP

#include "matcher_manager.hpp"
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/thread_time.hpp>
#include "packet.hpp"
#include <vector>
#include <boost/asio.hpp>
#include "command.hpp"

using boost::asio::local::datagram_protocol;

class CommandRecvService
{

public:

	CommandRecvService ();

	/// pre-condition: CommArg must be valid
	void run();

	/// singleton
	/// @return 
	static CommandRecvService & instance();

	void stop_io_service() { io_.stop(); }
	static CommandPtr make_command(u_char * buff, unsigned length, unsigned msg_id);

	void send( const boost::asio::const_buffers_1 & buffer );

private:

	bool end();
	void command_execute_loop();
	void send_packets_loop();
	void debug_command_recv();

	boost::asio::io_service io_;
	datagram_protocol::endpoint client_;
	datagram_protocol::endpoint server_;
	datagram_protocol::endpoint sender_;
	datagram_protocol::socket sock_;

};

#endif
