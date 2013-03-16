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

using boost::asio::ip::tcp;

class CommandRecvService
{

public:

	CommandRecvService (bool use_tcp = false);

	/// pre-condition: CommArg must be valid
	void run();

	/// singleton
	/// @return 
	static CommandRecvService & instance();

	void stop_io_service() { io_.stop(); }
	static CommandPtr make_command(u_char * buff, size_t length);

private:

	bool end();
	void command_execute_loop();

	boost::asio::io_service io_;
	tcp::acceptor acceptor_;
	bool use_tcp_;

};

#endif
