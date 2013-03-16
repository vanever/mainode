#include "command.hpp"
#include "server.hpp"

#include "dcsp_fifo.h"

using namespace DCSP;

namespace {
	const std::string u_char_to_decimal_string(u_char c)
	{
		std::string s;
		u_char tmp = c / 100;
		if (tmp != 0) s += '0' + tmp;
		c -= tmp * 100;
		tmp = c / 10;
		if (tmp != 0) s += '0' + tmp;
		c -= tmp * 10;
		s += '0' + c;
		return s;
	}
}

const std::string Command::unsigned_to_string(unsigned ip)
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

void Command::execute()
{
	if (type() == ADD_NODES)
	{
		FDU_LOG(INFO) << "Execute Command: add nodes";
		foreach (MatcherPtr m, target_nodes())
		{
			FDU_LOG(INFO) << "add node: " << m->to_endpoint();
			MatcherManager::instance().add_matcher(m);
			// TODO
			// judge whether matchers are added successfully
			// and responsed to PDSS
		}
	}
	if (type() == DEL_NODES)
	{
		FDU_LOG(INFO) << "Execute Command: collect nodes";
		foreach (MatcherPtr m, target_nodes())
		{
			FDU_LOG(INFO) << "collect node: " << m->to_endpoint();
			MatcherManager::instance().set_matcher_state( *m, Matcher::TO_BE_COLLECTED );
			// TODO
			// responsed to PDSS
		}
	}
	if (type() == CONFIG_MAIN_NODE)
	{
		FDU_LOG(INFO) << "Execute Command: config main node";
		u_char * bytes = &bytes_[0];
		u_char c = *bytes++;
		int nbits;
		if (c == 1) nbits = 32;		else
		if (c == 2) nbits = 64;		else
		if (c == 4) nbits = 128;	else
			FDU_LOG(ERR) << "nbits error: now is " << c << " using default 32 nbits", nbits = 32;

		c = *bytes++;
		int merge_unit;
		if (c == 1) merge_unit = 8;  	else
		if (c == 2) merge_unit = 16; 	else
		if (c == 4) merge_unit = 32; 	else
			FDU_LOG(ERR) << "merge_unit error: now is " << c << " using default 32 merge_unit", merge_unit = 32;

		CommArg::comm_arg().nbits = nbits;
		CommArg::comm_arg().merge_unit = merge_unit;
		CommArg::comm_arg().lib_id = cast<unsigned short>(bytes); bytes += sizeof(unsigned short);
		CommArg::comm_arg().max_num_node = cast<unsigned short>(bytes); bytes += sizeof(unsigned short);
		FDU_LOG(INFO) << "config info"
			<< "\n-- nbits: " << nbits
			<< "\n-- merge_unit: " << merge_unit
			<< "\n-- lib_id: " << CommArg::comm_arg().lib_id
			<< "\n-- max_num_node: " << CommArg::comm_arg().max_num_node;

	}
	if (type() == START_MAIN_NODE)
	{
		Server::instance().start();
	}
}

void Command::reply()
{
	const unsigned SIZE = 1500;
	char buff[SIZE];
	unsigned length = 2 + 4 + 4 + 2 + 2 + 2;

	u_char * p = (u_char *)buff;
//	p += 2;	// CUDP head
	cast<unsigned>(p) = DCSP::MAINODE_ID; p += sizeof(unsigned);
	cast<unsigned>(p) = DCSP::PDSS_ID   ; p += sizeof(unsigned);

	switch (type())
	{
	case START_MAIN_NODE:
		// TODO: confirm reply type
		break;
	case CONFIG_MAIN_NODE:
		// TODO: confirm reply type
		break;
	case ADD_NODES:
		// TODO: confirm reply type
		break;
	case DEL_NODES:
		// TODO: confirm reply type
		break;
	default:
		FDU_LOG(DEBUG) << "error type when reply";
	}
	cast<unsigned>(p)       = type() + 1;
	cast<unsigned short>(p) = (length);
	dcsp_fifo_send(buff, length);
}
