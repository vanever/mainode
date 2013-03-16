#include "command_recv_service.hpp"
#include "server.hpp"
#include "dcsp_fifo.h"

using namespace boost::asio::ip;
using namespace std;
using namespace DCSP;

CommandRecvService & CommandRecvService::instance()
{
	static CommandRecvService c(false);	// using DCSP FIFO as communication
	return c;
}

bool CommandRecvService::end()
{
	bool server_end = Server::instance().all_comm_end();
	bool no_command = Server::instance().command_queue()->box_empty();
	return server_end && no_command;
}

CommandRecvService::CommandRecvService(bool use_tcp)
	: io_()
	, acceptor_( io_, tcp::endpoint(ip::address(ip::address::from_string(CommArg::comm_arg().command_bind_ip )), CommArg::comm_arg().command_bind_port) )
	, use_tcp_(use_tcp)
{
}

void CommandRecvService::command_execute_loop()
{
	CommandQueue * queue = Server::instance().command_queue();
	while (CommandPtr c = queue->get())
	{
		c->execute();
		c->reply();
	}
}

void CommandRecvService::run()
{
	//! open command executing
	boost::thread t(
			boost::bind(&CommandRecvService::command_execute_loop, this)
			);
	t.detach();

	if (use_tcp_)
	{
		tcp::socket sock(io_);
		acceptor_.accept( sock );
		tcp::endpoint e = sock.remote_endpoint();
		FDU_LOG(INFO) << "CommandRecvService: accept remote " << e;
		while (!end())
		{
			u_char buff[1500];
			boost::system::error_code ec;
			size_t length = sock.read_some(boost::asio::buffer(buff), ec);	// block
			if (ec)
			{
				FDU_LOG(ERR) << "error read data from " << e;
				continue;
			}
			// TODO
			// add src_code sink_code judgement
			if (CommandPtr c = make_command(buff, length))
			{
				Server::instance().command_queue()->put( c );
			}
		}
	}
	else
	{
		ASSERT (dcsp_fifo_init(1) >= 0, "dcsp init error");
		while (!end())
		{
			static const unsigned SIZE = 1500;
			u_char buff[SIZE];
			int length = dcsp_fifo_read((char *)buff, SIZE);	// block
			if (length < 0)
			{
				FDU_LOG(ERR) << "error read data from dcsp fifo";
				continue;
			}
			// TODO
			// add src_code sink_code judgement
			if (CommandPtr c = make_command(buff, length))
			{
				Server::instance().command_queue()->put( c );
			}
		}
	}
}

CommandPtr CommandRecvService::make_command(u_char * buff, size_t length)
{
	const unsigned num_head_bytes = /*use_tcp_ ? 1 + 1 + 4 + 4 :*/ 4 + 4;
	const unsigned num_msg_info   = 2 + 2 + 2;

	u_char * p = buff + num_head_bytes;
	unsigned short msg_type   = ntohs(cast<unsigned short>(p)); p += sizeof(unsigned short);
	unsigned short msg_code   = ntohs(cast<unsigned short>(p)); p += sizeof(unsigned short);
	unsigned short msg_length = ntohs(cast<unsigned short>(p)); p += sizeof(unsigned short);

	u_char * msg_contents = p;
	unsigned msg_key = (msg_type << 16) + msg_code;
	if (msg_length != length - num_head_bytes - num_msg_info)
		FDU_LOG(WARN) << "message length not matched";

	Command::CMDTYPE ctype;
	CommandPtr c;

	switch (msg_key)
	{
		case MSG_START_MAIN_NODE:	// start run
			{
				ctype = Command::START_MAIN_NODE;
				c = CommandPtr(new Command( ctype ));
			}
			break;
		case MSG_CONFIG_MAIN_NODE:	// load config
			{
				ctype = Command::CONFIG_MAIN_NODE;
				c = CommandPtr(new Command( ctype ));
				c->set_config_bytes(vector<u_char>(msg_contents, msg_contents + 6));	// magic number need confirm
				unsigned num_ip = ntohs( cast<unsigned short>(buff + num_head_bytes + num_msg_info + 6) );
				if (num_ip * sizeof(unsigned) != msg_length - 8)
					FDU_LOG(WARN) << "ip contents length not matched";
				unsigned * pip = (unsigned *)(buff + num_head_bytes + num_msg_info + 8);
				for (unsigned i = 0; i < num_ip; i++)
				{
					c->add_target_node(udp::endpoint(
								ip::address::from_string(Command::unsigned_to_string(ntohl(pip[i]))), CommArg::comm_arg().fpga_port
								));
				}
			}
			break;
		case MSG_ADD_NODES:	// add node
		case MSG_DEL_NODES:	// del node
			{
				ctype = msg_key == MSG_ADD_NODES ? Command::ADD_NODES : Command::DEL_NODES;
				c = CommandPtr(new Command( ctype ));
				unsigned num_ip = 1;
				unsigned * pip = (unsigned *)(p);
				for (unsigned i = 0; i < num_ip; i++)
				{
					c->add_target_node(udp::endpoint(
								ip::address::from_string(Command::unsigned_to_string(ntohl(pip[i]))), CommArg::comm_arg().fpga_port
								));
				}
			}
///*{{{*/
//			{
//				ctype = msg_key == MSG_ADD_NODES ? Command::ADD_NODES : Command::DEL_NODES;
//				c = CommandPtr(new Command( ctype ));
//				unsigned num_ip = cast<unsigned short>(p); p += sizeof(unsigned short);
//				if (num_ip * sizeof(unsigned) + sizeof(unsigned short) != msg_length)
//					FDU_LOG(WARN) << "ip contents length not matched";
//				unsigned * pip = (unsigned *)(p);
//				for (unsigned i = 0; i < num_ip; i++)
//				{
//					c->add_target_node(udp::endpoint(
//								ip::address::from_string(Command::unsigned_to_string(pip[i])), CommArg::comm_arg().fpga_port
//								));
//				}
//			}
///*}}}*/
			break;
		default:
			FDU_LOG(ERR) << boost::format("unknown message key: %08x, skip this message") % msg_key;
			break;
	}
	return c;
}
