#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "matcher_manager.hpp"

namespace DCSP {

	const unsigned MSG_ADD_NODES        = 0x00010010;
	const unsigned MSG_DEL_NODES        = 0x00010011;
	const unsigned MSG_TELL_LOADS       = 0x00010024;
	const unsigned MSG_CREATE_DOMAIN    = 0x00010005;
	const unsigned MSG_REPLY            = 0x0001006F;
	const unsigned MSG_OMC_REQ          = 0x00010002;
	const unsigned MSG_RESULT_RPT       = 0x00030003;

	const unsigned MSG_QUERY_LOADS      = 0x00020042;
	const unsigned MSG_REPORT_LOADS     = 0x00020043;

}

struct DCSPPacket;
typedef boost::shared_ptr<DCSPPacket> DCSPPacketPtr;

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

	void do_hton() {
		src_id  = htonl(src_id);
		snk_id  = htonl(snk_id);
		msg_id  = htonl(msg_id);
		msg_len = htons(msg_len);
	}

	boost::asio::const_buffers_1 to_buffer()
	{
		auto ret = boost::asio::buffer((const U08 *)this, 14 + msg_len);
		do_hton();
		return ret;
	}

	DCSPPacketPtr make_dcsp_packet(const DCSPPacket & p) { return DCSPPacketPtr(new DCSPPacket(p)); }

} __attribute__((packed));

/// interface class
class ICommand
{

public:

	virtual void execute() = 0;
	virtual void reply  () = 0;
	static void do_reply(unsigned msg_key = DCSP::MSG_REPLY);

	static const std::string unsigned_to_string (unsigned u);

};

//---------------------------------------------------------------------------------- 
class AddNodesCommand : public ICommand
{

public:

	typedef std::set<unsigned> Nodes;

	AddNodesCommand(AppId id)
		: app_id_(id)
	{
	}

	void add_target_node(unsigned node_id)
	{
		nodes_.insert(node_id);
	}

	const Nodes & target_nodes() { return nodes_; }

	virtual void execute();
	virtual void reply  ();

private:

	AppId app_id_;
	Nodes nodes_;

};

//---------------------------------------------------------------------------------- 
class RemoveNodesCommand : public ICommand
{

public:

	typedef std::set<unsigned> Nodes;

	RemoveNodesCommand(/*AppId id*/)
		: app_id_{0,0}		// C++11风格初始化
	{
	}

	void add_target_node(unsigned node_id)
	{
		nodes_.insert(node_id);
	}

	const Nodes & target_nodes() { return nodes_; }

	virtual void execute();
	virtual void reply  ();

private:

	AppId app_id_;
	Nodes nodes_;

};

//---------------------------------------------------------------------------------- 
class TellLoadsCommand : public ICommand
{

public:

	typedef std::map<unsigned, unsigned> Loads;

	TellLoadsCommand(/*AppId id*/)
		: app_id_{0,0}		// C++11风格初始化
	{
	}

	void add_load(unsigned node_id, unsigned load)
	{
		loads_[node_id] = ntohl(load);
	}

	const Loads & loads() { return loads_; }

	virtual void execute();
	virtual void reply  ();

private:

	AppId app_id_;
	Loads loads_;

};

//---------------------------------------------------------------------------------- 
class DomainCreateCommand : public ICommand
{

public:

	DomainCreateCommand(DomainInfoPtr pd);

	virtual void execute();
	virtual void reply  ();

private:

	DomainInfoPtr domain_;

};

//---------------------------------------------------------------------------------- 
class QueryLoadsCommand : public ICommand
{

public:

	QueryLoadsCommand(AppId appid);

	virtual void execute();
	virtual void reply  ();

private:

	AppId appid_;

};

//---------------------------------------------------------------------------------- 
class ShowResultCommand : public ICommand
{

public:

	ShowResultCommand(AppId appid);

	virtual void execute();
	virtual void reply  ();

private:

	AppId appid_;

};

//---------------------------------------------------------------------------------- 
class TerminalChangeCommand : public ICommand
{

public:

	// TODO

	virtual void execute() {}
	virtual void reply  () {}

private:


};

typedef boost::shared_ptr<ICommand> CommandPtr;

#endif
