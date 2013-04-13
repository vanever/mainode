#include "command_recv_service.hpp"
#include "server.hpp"

#define SERVER_SOCK_FILE "/tmp/DCSP_FIFO_A_SOCK"
#define CLIENT_SOCK_FILE "/tmp/DCSP_FIFO_B_SOCK"

using namespace boost::asio::ip;
using namespace std;
using namespace DCSP;

namespace {
	unsigned norm_dcsp_msg_id( unsigned id )
	{
		return ntohl(id);
	}
}

const unsigned MSG_SHOW_APP_RESULT = 0x00000070;

CommandRecvService & CommandRecvService::instance()
{
	static CommandRecvService c;	// using DCSP FIFO as communication
	return c;
}

bool CommandRecvService::end()
{
	bool server_end = Server::instance().all_comm_end();
	bool no_command = Server::instance().command_queue()->box_empty();
	return server_end && no_command;
}

void CommandRecvService::send( const boost::asio::const_buffers_1 & buffer )
{
	sock_.send_to(buffer, server_);
}

CommandRecvService::CommandRecvService()
	: io_()
	, client_(CLIENT_SOCK_FILE)
	, server_(SERVER_SOCK_FILE)
	, sender_()
	, sock_( io_, datagram_protocol() )
{
	unlink(CLIENT_SOCK_FILE);
	sock_.bind(client_);
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

void CommandRecvService::send_packets_loop()
{
	CommandWindow * window = Server::instance().command_window();
	while (DCSPPacketPtr pkt = window->get())
	{
		send(pkt->to_buffer());
	}
}

void CommandRecvService::run()
{
	//! open command executing
	boost::thread t(
			boost::bind(&CommandRecvService::command_execute_loop, this)
			);
	t.detach();
	boost::thread t2(
			boost::bind(&CommandRecvService::send_packets_loop, this)
			);
	t2.detach();

	DCSPPacket pkt;
	U08 * bytes = (U08 *)&pkt;

	while (!end())
	{
		boost::system::error_code ec;
		size_t length = sock_.receive_from(boost::asio::buffer(bytes, sizeof(DCSPPacket)), sender_);	// block
		if (length > 0)
		{
			pkt.do_ntoh();
			CommArg::comm_arg().pdss_id = pkt.src_id;		// record PDSS_ID
			CommArg::comm_arg().mainode_id = pkt.snk_id;	// record MAINODE_ID
			U16 msg_len = pkt.msg_len;
			if (msg_len != length - 14)
			{
				FDU_LOG(WARN) << "message length not match: " << msg_len << " wanted is " << length - 14;
			}
			if (CommandPtr c = make_command(pkt.msg, msg_len, pkt.msg_id))
			{
				Server::instance().command_queue()->put( c );
			}
		}
	}
}

CommandPtr CommandRecvService::make_command(u_char * msg, unsigned length, unsigned msg_id)
{
	CommandPtr c;

	switch (msg_id)
	{
		case MSG_CREATE_DOMAIN:	// load config
			{
				if (length != sizeof(DomainInfo))
					FDU_LOG(WARN) << "domain info size not match";
				DomainInfo * d = new DomainInfo;
				memcpy( d, msg, sizeof(DomainInfo));
				c = CommandPtr( new DomainCreateCommand(DomainInfoPtr(d)) );
			}
			break;
		case MSG_ADD_NODES:	// add node
			{
				AppId d = cast<AppId>(msg); msg += sizeof(AppId);
				U32 num_node = ntohl(cast<U32>(msg)); msg += sizeof(U32);
				if (length != num_node * 4 + 14)
					FDU_LOG(ERR) << "length not match in MSG_ADD_NODES";
				AddNodesCommand * cmd = new AddNodesCommand(d);
				U32 * nodes = (U32 *)msg;
				for (int i = 0; i < num_node; i++)
				{
					cmd->add_target_node(nodes[i]);
				}
				c = CommandPtr(cmd);
			}
			break;
		case MSG_DEL_NODES:	// del node
			{
				//AppId d = cast<AppId>(msg); msg += sizeof(AppId);
				U32 num_node = ntohl(cast<U32>(msg)); msg += sizeof(U32);
				if (length != num_node * 4 + 4)
					FDU_LOG(ERR) << "length not match in MSG_ADD_NODES";
				RemoveNodesCommand * cmd = new RemoveNodesCommand(/*d*/);
				U32 * nodes = (U32 *)msg;
				for (int i = 0; i < num_node; i++)
				{
					cmd->add_target_node(nodes[i]);
				}
				c = CommandPtr(cmd);
			}
			break;
		case MSG_TELL_LOADS:	// tell loads
			{
				if (!CommArg::comm_arg().update_load) break;
				U32 num_node = length / 8;
				if (length != num_node * 8)
					FDU_LOG(ERR) << "length not match in MSG_TELL_LOADS";
				TellLoadsCommand * cmd = new TellLoadsCommand(/*d*/);
				U32 * nodes = (U32 *)msg;
				for (int i = 0; i < num_node; i += 2)
				{
					cmd->add_load(nodes[i], nodes[i+1]);
				}
				c = CommandPtr(cmd);
			}
			break;
		case MSG_SHOW_APP_RESULT:	// show result
			{
				AppId d = cast<AppId>(msg); msg += sizeof(AppId);
				ShowResultCommand * cmd = new ShowResultCommand(d);
				c = CommandPtr(cmd);
			}
			break;
		case MSG_QUERY_LOADS:	// show result
			{
				AppId d = cast<AppId>(msg); msg += sizeof(AppId);
				QueryLoadsCommand * cmd = new QueryLoadsCommand(d);
				c = CommandPtr(cmd);
			}
			break;
		default:
			FDU_LOG(ERR) << boost::format("unknown message key: %08x, skip this message") % msg_id;
			break;
	}
	return c;
}
