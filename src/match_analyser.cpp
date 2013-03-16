#include "match_analyser.hpp"
#include <boost/make_shared.hpp>
#include "bits.hpp"
#include "server.hpp"
#include "match_stream.hpp"

using namespace std;

#define xml_attribute( out, name, value ) \
	do { \
		(out) << (name) << "=" << "\"" << (value) << "\" "; \
	} while (0)

#define PRINT_RANGES

static const unsigned int END_KEY = 0xFFFFFFFF;

void MatchAnalyser::handle_packet( Packet & pkt, const udp::endpoint & from_addr )
{
	static unsigned count = 0;
	FDU_LOG(INFO) << "MatchAnalyser: handle	" << ++count << " packet";

	static u_char wanted_idx = 0;
	if (pkt.index() != wanted_idx) {
		FDU_LOG(DEBUG) << "MatchAnalyser wanted: " << wanted_idx << " but received: " << pkt.index() << " skip this packet.";
		return;
	}

	++wanted_idx;

	static unsigned end_packet_num = 0;
	unsigned cnt = 0;
	unsigned * p = (unsigned *)pkt.data_ptr();
	bool end_flag = false;
	const unsigned max_data_len = 1440;
	unsigned data_len = pkt.data_len();
	while (cnt < data_len / sizeof(unsigned))
	{
		unsigned lb = ntohl(p[cnt++]);
		unsigned tb = ntohl(p[cnt++]);
		if (lb || tb)
			sec_man_.add_section( MatchSection(tb, lb, CommArg::comm_arg().merge_unit) );
	}
}

void MatchAnalyser::output_result(std::ostream & out)
{
	// TODO
}
