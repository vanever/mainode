#ifndef MATCH_ANALYSER
#define MATCH_ANALYSER

#include "match_utils.h"
#include "packet.hpp"
#include <fstream>
#include <map>
#include "matcher_manager.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

using boost::asio::ip::udp;

class MatchAnalyser
{

public:

	static MatchAnalyser & instance() {
		static MatchAnalyser m;
		return m;
	}

	void handle_packet( Packet & pkt, const udp::endpoint & from_addr );
	void output_result( std::ostream & out );

private:

	MatchAnalyser () {}

	SectionManager sec_man_;

};

#endif
