#include "packet_handle_center.hpp"
#include "server.hpp"
#include "connection_build_service.hpp"
#include "match_stream.hpp"

using namespace std;

#ifdef NO_PACKET_MSG
#define PACKET_MSG(LOG_TYPE, length, endpoint, type, index, MSG_TYPE)
#else
#define PACKET_MSG(LOG_TYPE, length, endpoint, type, index, MSG_TYPE) \
	do { \
		FDU_LOG(LOG_TYPE) << "--------- " << MSG_TYPE << " ---------"; \
		FDU_LOG(LOG_TYPE) << "--- length:" << length << "\n" << endpoint; \
		FDU_LOG(LOG_TYPE) << "--- type:" << boost::format("0x%02x") % (int)type; \
		FDU_LOG(LOG_TYPE) << "--- index:" << (int)index; \
	} while (0)
#endif

PacketHandleCenter::PacketHandleCenter()
	: quit_(false)
{
}

void PacketHandleCenter::run()
{
	packet_handle_loop();
	FDU_LOG(INFO)
		<< "\n--------------------------------"
		<< "\n-- PacketHandleCenter quit"
		<< "\n--------------------------------";
}

void PacketHandleCenter::packet_handle_loop()
{
	PacketQueue * queue = Server::instance().packet_queue();
	while ( PacketPtr pkt = queue->get() )
	{
		handle_packet( pkt, pkt->from_point() );
	}
}

void PacketHandleCenter::handle_packet( PacketPtr p, const udp::endpoint & end_point )
{
	static unsigned long num_responses_image = 0;
	static unsigned long num_responses_point = 0;
	static unsigned long num_responses_lib = 0;
	static unsigned long num_unknown   = 0;

	Packet & pkt = *p;
	u_char type  = pkt.type();
	u_char index = pkt.index();
	size_t length = pkt.total_len();

	// PACKET_MSG(DEBUG, length, end_point, type, index, "all received packets");

	//--------------------------------------------------------------------------------------- 
	// receiving match results from 2~59# FPGA
	if (type == MATCH_RECVING || type == MATCH_END)
	{
//		PACKET_MSG(INFO, length, end_point, type, index, "match result");
		PACKET_MSG(DEBUG, length, end_point, type, index, "match result");
		if (MatcherManager::instance().get_matcher_state( end_point ) ==  Matcher::BUSY )
			MatcherManager::instance().set_matcher_state( end_point, Matcher::SENDING );
		Server::instance().send( ResponPacket(type + 1, index + 1).to_udp_buffer(), end_point );
		MatchAnalyser::instance().handle_packet(pkt, end_point);
	}

	//---------------------------------------------------------------------------------- 
	// bit feature
	else if (type == BITFEATURE_TYPE + 1 || type == (BITFEATURE_TYPE | 0x10) + 1)
	{
		PACKET_MSG(DEBUG, length, end_point, type, index, "bitfeature response");
		Server::instance().bitfeature_window()->confirm( index - 1 );
		unsigned stock_data = *(unsigned*)p->data_ptr();
		FDU_LOG(INFO) << "stock_data = " << ntohl(stock_data);
	}

//	//---------------------------------------------------------------------------------- 
//	// matcher add info
//	else if (type == CONN_BUILD)
//	{
//		PACKET_MSG(DEBUG, length, end_point, type, index, "add node msg");
//
//		u_char * pc = p->data_ptr();
//		unsigned nbits = pc[0];
//		unsigned merge_unit = pc[1];
//
//		bool nbits_ok = nbits == 1 || nbits == 2 || nbits == 4;
//		bool merge_unit_ok = merge_unit == 1 || merge_unit == 2 || merge_unit == 4;
//
//		if (!nbits_ok)
//		{
//			FDU_LOG(ERR) << "illegal nbits: " << nbits << "using defaults: " << 32;
//			nbits = 1;
//		}
//		nbits *= 32;
//
//		if (!merge_unit_ok)
//		{
//			FDU_LOG(ERR) << "illegal merge_unit: " << nbits << "using merge_unit: " << 32;
//			merge_unit = 4;
//		}
//		merge_unit *= 8;
//
//		MatcherPtr m;
//		if ( (m = MatcherManager::instance().find(Matcher(end_point))) )
//		{
//			FDU_LOG(WARN) << "repeated matcher in added node";
//			ResponPacket pkt(type + 1, index + 1);
//			u_char * p = pkt.data_ptr();
//			p[0] = m->match_threshold();
//			p[1] = m->merge_threshold();
//			Server::instance().send( pkt.to_udp_buffer(), end_point );	// node need a response
//			return;
//		}
//		else
//		{
//			m = MatcherManager::instance().make_matcher( end_point );
//		}
//
//		m->set_nbits(nbits);
//		m->set_merge_unit(merge_unit);
//
//		MatcherManager::instance().add_matcher( m );
//	}

//	//--------------------------------------------------------------------------------------- 
//	// connection build
//	else if (type == CONN_BUILD + 2)
//	{
//		PACKET_MSG(DEBUG, length, end_point, type, index, "connection build");
//		if (ConnectionBuilder * c = ConnectionBuilder::get_build_task(end_point))
//		{
//			c->set_built(true);
//		}
//	}
//
	//--------------------------------------------------------------------------------------- 
	// connection build
	else if (type == CONN_BUILD + 1)
	{
		PACKET_MSG(DEBUG, length, end_point, type, index, "connection build");
		if (ConnectionBuilder * c = ConnectionBuilder::get_build_task(end_point))
		{
			c->set_built(true);
		}
	}

	//---------------------------------------------------------------------------------- 
	// node wanna quit
	else if (type == WANNA_QUIT)
	{
		PACKET_MSG(DEBUG, length, end_point, type, index, "wanna quit");
		if ( MatcherPtr m = MatcherManager::instance().find(Matcher(end_point)) )
		{
			m->set_state( Matcher::TO_BE_COLLECTED );
		}
		else
		{
			FDU_LOG(ERR) << "WANNA_QUIT: cannot find node: " << end_point;
		}
		Server::instance().send( ResponPacket(type + 1, index + 1).to_udp_buffer(), end_point );
	}

	//---------------------------------------------------------------------------------- 
	// node quited
	else if (type == QUITED)
	{
		PACKET_MSG(DEBUG, length, end_point, type, index, "quited");
		if ( MatcherPtr m = MatcherManager::instance().find(Matcher(end_point)) )
		{
			if (m->state() == Matcher::TO_BE_COLLECTED)
			{
				m->set_state( Matcher::COLLECTED );
				MatcherManager::instance().remove_matcher( *m );
			}
			else if (m->state() != Matcher::COLLECTED)
				FDU_LOG(ERR) << "node state error in QUITED process";
		}
		else
		{
			FDU_LOG(WARN) << "WANNA_QUIT: cannot find node: " << end_point;
		}
		Server::instance().send( ResponPacket(type + 1, index + 1).to_udp_buffer(), end_point );
	}

	//--------------------------------------------------------------------------------------- 
	// lib loading response
	else if (type == LIB_SENDING + 1 || type == LIB_START + 1 || type == LIB_END + 1)
	{
		SurfLibWindow * window = Server::instance().surf_lib_window();
		if ( type == LIB_END + 1 )
		{
			PACKET_MSG(DEBUG, length, end_point, type, index, "last lib response");
			window->confirm(index - 1);
			if (window->end_condition_satisfied())
			{	// check end
				num_responses_lib = 0;
				Matcher::MATCHER_STATE s = MatcherManager::instance().get_matcher_state(end_point);
				//				ASSERT( s == Matcher::LIB_LOADING, end_point << " wrong state is " << (int)s << " wanted is " << (int)Matcher::LIB_LOADING );
				if (s == Matcher::LIB_LOADING)
				{
					MatcherManager::instance().set_matcher_state( end_point, Matcher::READY );
					MatcherManager::instance().notify_wait_ready_matcher();
					FDU_LOG(INFO) << end_point << " lib loaded. " << " Do notify choose_next_matcher";
				}
			}
		}
		else
		{
			PACKET_MSG(DEBUG, length, end_point, type, index, "lib response");
			if (window->confirm(index - 1))
			{
				if ( !(++num_responses_lib % 10000) )
					FDU_LOG(INFO) << "matcher response: " << num_responses_lib;
			}
		}
	}
	//--------------------------------------------------------------------------------------- 
	// unknown type
	else
	{
		FDU_LOG(DEBUG) << "unknown packs: " << ++num_unknown << " with type: " << boost::format("%02x") % (int)type;
	}
}
