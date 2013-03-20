#include "command_recv_service.hpp"
#include "server.hpp"
#include "dcsp_fifo.h"

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

struct DCSPPacket
{
	U32 src_id;
	U32 snk_id;
	U32 msg_id;
	U16 msg_len;
	u_char msg[1500];
	void do_ntoh() {
		src_id  = ntohl(src_id);
		snk_id  = ntohl(snk_id);
		msg_id  = ntohl(msg_id);
		msg_len = ntohs(msg_len);
	}
} __attribute__((packed));


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

void CommandRecvService::send( const boost::asio::mutable_buffers_1 & buffer )
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

void CommandRecvService::run()
{
	//! open command executing
	boost::thread t(
			boost::bind(&CommandRecvService::command_execute_loop, this)
			);
	t.detach();

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
			if (msg_len != length - 16)
			{
				FDU_LOG(WARN) << "message length not match: " << msg_len << " wanted is " << length - 16;
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
				U16 num_node = ntohs(cast<U16>(msg)); msg += sizeof(U16);
				if (length != num_node * 4 + 12)
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
		case MSG_DEL_NODES:	// add node
			{
				AppId d = cast<AppId>(msg); msg += sizeof(AppId);
				U16 num_node = ntohs(cast<U16>(msg)); msg += sizeof(U16);
				if (length != num_node * 4 + 12)
					FDU_LOG(ERR) << "length not match in MSG_ADD_NODES";
				RemoveNodesCommand * cmd = new RemoveNodesCommand(d);
				U32 * nodes = (U32 *)msg;
				for (int i = 0; i < num_node; i++)
				{
					cmd->add_target_node(nodes[i]);
				}
				c = CommandPtr(cmd);
			}
			break;
		default:
			FDU_LOG(ERR) << boost::format("unknown message key: %08x, skip this message") % msg_id;
			break;
	}
	return c;
}
