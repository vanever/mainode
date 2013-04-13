#include "command.hpp"
#include "server.hpp"
#include "command_recv_service.hpp"
#include "match_stream.hpp"

//#include "dcsp_fifo.h"

#include "app_manager.hpp"

using namespace DCSP;

namespace {
	const std::string u_char_to_decimal_string(u_char c)
	{
		std::string s;
		u_char tmp = c / 100;
		bool notmove = false;
		if (tmp != 0 || notmove) s += '0' + tmp, notmove = true;
		c -= tmp * 100;
		tmp = c / 10;
		if (tmp != 0 || notmove) s += '0' + tmp;
		c -= tmp * 10;
		s += '0' + c;
		return s;
	}

	const std::string node_id_to_ip(unsigned node_id)
	{
		ASSERT(0, "not implemented");
		return "not implemented";
	}
}

const std::string ICommand::unsigned_to_string(unsigned ip)
{
	u_char c4 = ip & 0xFF;
	ip >>= 8;
	u_char c3 = ip & 0xFF;
	ip >>= 8;
	u_char c2 = ip & 0xFF;
	ip >>= 8;
	u_char c1 = ip & 0xFF;
	return
		u_char_to_decimal_string(c1) + '.' +
		u_char_to_decimal_string(c2) + '.' +
		u_char_to_decimal_string(c3) + '.' +
		u_char_to_decimal_string(c4) ;
}

void ICommand::do_reply(unsigned msg_key)
{
//	U08 buff[4 + 4 + 6];
	struct ResponInfo {
		U32 src_id;
		U32 snk_id;
		U32 msg_key;
		U16 msg_len;
	} __attribute__((packed));
	ResponInfo r;
	r.src_id  = htonl(CommArg::comm_arg().mainode_id);
	r.snk_id  = htonl(CommArg::comm_arg().pdss_id);
	r.msg_key = htonl(msg_key);
	r.msg_len = 0;
	CommandRecvService::instance().send(
			boost::asio::buffer((const char *)&r, sizeof(ResponInfo))
			);
	FDU_LOG(INFO) << boost::format(" reply key 0x%08x") % msg_key;
}

//---------------------------------------------------------------------------------- 
void AddNodesCommand::execute()
{
	if (DomainInfoPtr pd = DomainInfo::find_domain_by_id(app_id_))
	{
		foreach (unsigned nodeid, target_nodes())
		{
			std::string nodeip = MatcherManager::id_to_ip(ntohl(nodeid));
			//std::string nodeip = ICommand::unsigned_to_string(ntohl(nodeid));
			FDU_LOG(INFO) << "Add Node: " << boost::format("0x%08x") % nodeid << " IP: " << nodeip;
			MatcherManager::instance().add_matcher(
					MatcherManager::make_matcher(nodeip, app_id_)
					);
		}
	}
}

void AddNodesCommand::reply()
{
	ICommand::do_reply();
}

//---------------------------------------------------------------------------------- 
void RemoveNodesCommand::execute()
{
///	if (DomainInfoPtr pd = DomainInfo::find_domain_by_id(app_id_))
//	{
		foreach (unsigned nodeid, target_nodes())
		{
			std::string nodeip = MatcherManager::id_to_ip(ntohl(nodeid));
			//std::string nodeip = ICommand::unsigned_to_string(ntohl(nodeid));
			FDU_LOG(INFO) << "Remove Node: " << boost::format("0x%08x") % nodeid << " IP: " << nodeip;
			MatcherPtr m = MatcherManager::make_matcher(nodeip, app_id_);
			MatcherManager::instance().set_matcher_state( *m, Matcher::TO_BE_COLLECTED );
		}
//	}
}

void RemoveNodesCommand::reply()
{
	ICommand::do_reply();
}

//---------------------------------------------------------------------------------- 
void TellLoadsCommand::execute()
{
///	if (DomainInfoPtr pd = DomainInfo::find_domain_by_id(app_id_))
//	{
		foreach (auto load_pair, loads())
		{
			std::string nodeip = MatcherManager::id_to_ip(ntohl(load_pair.first));
			//std::string nodeip = ICommand::unsigned_to_string(ntohl(nodeid));
			FDU_LOG(INFO) << "Tell Loads: IP = " << nodeip << "  Load = " << load_pair.second;
			MatcherPtr m = MatcherManager::make_matcher(nodeip, app_id_);
			MatcherManager::instance().set_matcher_load( *m, load_pair.second );
		}
//	}
}

void TellLoadsCommand::reply()
{
	ICommand::do_reply();
}


//---------------------------------------------------------------------------------- 
DomainCreateCommand::DomainCreateCommand(DomainInfoPtr pd)
	: domain_(pd)
{
}

void DomainCreateCommand::execute()
{
	FDU_LOG(INFO) << "create app: " << domain_->ApplicationId.to_string();
	DomainInfo::add_domain(domain_);
	AppManager::instance().create_app(domain_);
}

void DomainCreateCommand::reply()
{
	// DCSP要求先发送一个OMC请求消息，以触发DCSP Agent更新ID/IP对照表
	ICommand::do_reply(MSG_OMC_REQ);
	ICommand::do_reply();
}

//---------------------------------------------------------------------------------- 
ShowResultCommand::ShowResultCommand(AppId appid)
	: appid_(appid)
{
}

void ShowResultCommand::execute()
{
	if (AppDomainPtr papp = AppManager::instance().get_domain_by_appid(appid_))
	{
		papp->output_result(std::cout);
	}
}

void ShowResultCommand::reply()
{
	ICommand::do_reply();
}

//---------------------------------------------------------------------------------- 
QueryLoadsCommand::QueryLoadsCommand(AppId appid)
	: appid_(appid)
{
}

void QueryLoadsCommand::execute()
{
}

void QueryLoadsCommand::reply()
{
	FDU_LOG(INFO) << "generate loads info to PDSS";
	if (AppDomainPtr papp = AppManager::instance().get_domain_by_appid(appid_))
	{
		DCSPPacket pkt;
		pkt.src_id  = CommArg::comm_arg().mainode_id;
		pkt.snk_id  = CommArg::comm_arg().pdss_id;
		pkt.msg_id  = MSG_REPORT_LOADS;
		pkt.msg_len = 26;
		unsigned num_sent      = papp->sent_frames();
		unsigned num_unhandled = papp->unhandled_frames();
		unsigned pause_time    = papp->bitfeature_sender()->pause_time();
		unsigned source_gen_speed;
		if (pause_time == 0)
		{
			FDU_LOG(INFO) << "pause_time is zero, use max speed";
			source_gen_speed = CommArg::comm_arg().max_send_speed;
		}
		else
		{
			source_gen_speed = (papp->domain_info_ptr()->SliceLen * 60) * 1000 / pause_time;
		}
		u_char * p = pkt.msg;
		cast<AppId>(p) = papp->appid(); p += sizeof(AppId);
		cast<U32>(p) = htonl(source_gen_speed); p += sizeof(U32);
		cast<U32>(p) = htonl(num_sent); p += sizeof(U32);
		cast<U32>(p) = htonl(num_unhandled); p += sizeof(U32);
		cast<U32>(p) = htonl(papp->speed()); p += sizeof(U32);
		CommandRecvService::instance().send(pkt.to_buffer());
	}
	else
		FDU_LOG(ERR) << "unknown appid " << appid_;
}

