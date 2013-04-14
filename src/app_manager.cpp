#include "app_manager.hpp"
#include "match_stream.hpp"
#include "command_recv_service.hpp"
#include <numeric>
#include <boost/format.hpp>
#include "server.hpp"

using namespace std;

ResultPacket::ResultPacket(DomainInfoPtr pd)
	: src_id(pd->LoadBalanceId)
	, snk_id(pd->TerminalId)
	, msg_id(htonl(DCSP::MSG_RESULT_RPT))
	, report_id(0)
	, app_id(pd->ApplicationId)
	, timestamp(0)
{ 
}

void ResultPacket::put_line(const string& line)
{
	int len = strlen(result) + line.length();
	if (len > max_len) {
		FDU_LOG(ERR) << "ResultPacket: result too long: " << len;
	}
	strcat(result, line.c_str());
	++lines;
}

void ResultPacket::send()
{
	int len = strlen(result);
	msg_len = htons(20 + len);

	if (CommArg::comm_arg().disable_sending_report_timeout) {
		CommandRecvService::instance().send(boost::asio::buffer((const char *)this, 34 + len));
	}
	else {
		DCSPPacket copy_pkt;
		memcpy(&copy_pkt, this, 34 + len);
		DCSPPacketPtr pkt(DCSPPacket::make_dcsp_packet(copy_pkt));
		Server::instance().command_window()->put(pkt);
	}

	clear();
	report_id = htons(ntohs(report_id) + 1);
}

	AppDomain::AppDomain(DomainInfoPtr pd)
	: appid_(pd->ApplicationId)
	, loader_(0)
	, sender_(0)
	, sec_man_(pd->ReMergeThrehold) 
	, domaininfo_(pd)
	, sent_frames_(0)
	, result_pkt_(pd)
{
	pd->LibId = ntohs(pd->LibId);
	ASSERTS(pd->LibId < CommArg::comm_arg().load_args.size());	// currently use lib id as source id
	match_source_ = CommArg::comm_arg().load_args[pd->LibId].source;
	loader_ = new BitFeatureLoader(this);
	sender_ = new BitFeatureSender(this);
	unsigned domain_id = ntohs(appid_.DomainId);
	U64 domain_time = ntohll(appid_.TimeStamp);
	lib_file_ = (boost::format(CommArg::comm_arg().path_format) % CommArg::comm_arg().load_args[pd->LibId].lib % (pd->NBits << 5)).str();
	out_file_.open((boost::format(CommArg::comm_arg().out_file) % domain_id % domain_time).str());
	source_repeat_times_ = CommArg::comm_arg().load_args[pd->LibId].source_repeat_times;
	if (CommArg::comm_arg().update_load)
		CommArg::comm_arg().node_speed = ntohs(pd->SpeedPerPipe) * pd->NumTotalPipe;

	boost::thread t(boost::bind(&AppDomain::trigger_run, this));
	t.detach();
}

AppDomain::~AppDomain()
{
	if (loader_) delete loader_;
	if (sender_) delete sender_;
}

void AppDomain::output_result(std::ostream & out)
{
	foreach (unsigned testid, testid_set_)
		sec_man_.output_results(out, testid);
}

// todo: send to dcsp
void AppDomain::output_result(unsigned testid)
{
	static const int max_lines = 12;

	vector<string> results = sec_man_.xml_results(testid, domain_info_ptr()->SliceLen);
	foreach (const string& line, results) {
		out_file_ << line;
		result_pkt_.put_line(line);
		if (result_pkt_.lines >= max_lines) result_pkt_.send();
	}
	out_file_.flush();
	if (result_pkt_.lines > 0) result_pkt_.send();

	sec_man_.delete_results(testid);
}

unsigned AppDomain::unhandled_frames() const
{
	MatcherManager::Matchers ms(MatcherManager::instance().find_matchers_at_domain_and_state( appid(), Matcher::READY ));
	unsigned ret = 0;
	foreach (MatcherPtr m, ms)
	{
		ret += m->num_load();
	}
	return ret;
}

unsigned AppDomain::speed() const
{
	MatcherManager::Matchers ms(MatcherManager::instance().find_matchers_at_domain_and_state( appid(), Matcher::READY ));
	return ms.size() * CommArg::comm_arg().node_speed;
}

void AppDomain::trigger_run()
{
	StreamLine line;

	line.add_node( loader_ );
	line.add_node( sender_ );

	line.start_running_stream();
	line.wait_stream_end();

	FDU_LOG(INFO) << "BitFeature all sent: " << appid();
}

//---------------------------------------------------------------------------------- 

bool AppManager::create_app(DomainInfoPtr pd)
{
	AppId appid = pd->ApplicationId;
	auto it = domains_.find(appid);
	if (it != domains_.end())
	{
		FDU_LOG(ERR) << "Application already exists: with appid " << appid;
	}
	else
	{
		AppDomainPtr pa(new AppDomain(pd));
		domains_.insert(std::make_pair(
					appid, pa
					));
	}
}

