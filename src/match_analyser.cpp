#include "match_analyser.hpp"
#include <boost/make_shared.hpp>
#include "bits.hpp"
#include "server.hpp"
#include "match_stream.hpp"
#include "app_manager.hpp"

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

	MatcherPtr pm = MatcherManager::instance().find(from_addr);
	if (!pm)
	{
		FDU_LOG(WARN) << "skip result from unknown matcher " << from_addr;
		return;
	}

	u_char wanted_idx = pm->result_count();
	if (pkt.index() != wanted_idx) {
		FDU_LOG(DEBUG) << "MatchAnalyser wanted: " << (int)wanted_idx << " but received: " << pkt.index() << " skip this packet.";
		return;
	}

	pm->increse_result_count();

	AppDomainPtr papp = AppManager::instance().get_domain_by_appid(pm->domain_id());
	if (!papp)
	{
		FDU_LOG(WARN) << "cannot find app " << pm->domain_id();
		return;
	}

	static unsigned end_packet_num = 0;
	unsigned cnt = 0;
	unsigned * p = (unsigned *)pkt.data_ptr();
	bool end_flag = false;
	const unsigned max_data_len = 1440;
	unsigned data_len = pkt.data_len();
	while (cnt < data_len / sizeof(unsigned))
	{
		unsigned lb = ntohl(p[cnt++]);
		unsigned tb = (p[cnt++]);
//		unsigned lb = ntohl(p[cnt++]);
//		unsigned tb = ntohl(p[cnt++]);
		if (lb)
			papp->add_section( MatchSection(tb, lb, papp->domain_info_ptr()->MergeUnit) );
		else if (tb)
		{	// output condition
			papp->output_result(std::cout, tb >> 12);
		}
		else
		{
			FDU_LOG(INFO) << "skip all 0 64bit";
		}
	}
}

void MatchAnalyser::output_result(std::ostream & out)
{
}
