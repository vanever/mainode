#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "matcher_manager.hpp"

namespace DCSP {

	const unsigned MAINODE_ID = 0x00800000;
	const unsigned PDSS_ID    = 0x00800001;

	const unsigned MSG_START_MAIN_NODE  = 0x00000010;	// need confirm
	const unsigned MSG_CONFIG_MAIN_NODE = 0x00000011;	// need confirm

	const unsigned MSG_ADD_NODES        = 0x00010010;
	const unsigned MSG_DEL_NODES        = 0x00010011;
	const unsigned MSG_ADD_LOADS        = 0x00010012;
	const unsigned MSG_DEL_LOADS        = 0x00010013;
}

class Command
{

public:

	enum CMDTYPE {
		CONFIG_MAIN_NODE,	//<! config main node
		START_MAIN_NODE,	//<! start main node
		ADD_NODES,			//<! add node
		DEL_NODES,			//<! collect node
	};

	Command (CMDTYPE t)
		: type_(t)
	{}

	MatcherManager::Matchers & target_nodes()
	{
		return nodes_;
	}

	CMDTYPE type() const { return type_; }

	static const std::string unsigned_to_string(unsigned ip);

	void add_target_node(const boost::asio::ip::udp::endpoint & e)
	{
		FDU_LOG(DEBUG) << "add node: " << e;
		ASSERT((int)type_ >= (int)ADD_NODES, "type error");
		MatcherPtr m = MatcherManager::make_matcher( e );
		m->set_nbits( CommArg::comm_arg().nbits );
		m->set_merge_unit( CommArg::comm_arg().merge_unit );
		nodes_.push_back( m );
	}

	void set_config_bytes(const std::vector<u_char> & bytes)
	{
		bytes_ = bytes;
	}

	void execute();
	void reply();

private:

	CMDTYPE type_;
	MatcherManager::Matchers nodes_;
	std::vector<u_char> bytes_;

};

typedef boost::shared_ptr<Command> CommandPtr;

#endif
