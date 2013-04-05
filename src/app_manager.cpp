#include "app_manager.hpp"
#include "match_stream.hpp"
#include <numeric>

using namespace std;

AppDomain::AppDomain(DomainInfoPtr pd)
	: appid_(pd->ApplicationId)
	, loader_(0)
	, sender_(0)
	, sec_man_(pd->ReMergeThrehold) 
	, domaininfo_(pd)
	, sent_frames_(0)
{
	ASSERTS(pd->LibId < CommArg::comm_arg().sources.size());	// currently use lib id as source id
	match_source_ = CommArg::comm_arg().sources[pd->LibId];
	loader_ = new BitFeatureLoader(this);
	sender_ = new BitFeatureSender(this);

	boost::thread t(boost::bind(&AppDomain::trigger_run, this));
	t.detach();
}

AppDomain::~AppDomain()
{
	if (loader_) delete loader_;
	if (sender_) delete sender_;
}

void AppDomain::output_result(const std::string & path)
{
	ofstream out(path.c_str());
	ASSERT(out, "cannot open " << path);
	output_result(out);
}

void AppDomain::output_result(std::ostream & out, unsigned testid)
{
	sec_man_.output_results(out, testid);
}

void AppDomain::output_result(std::ostream & out)
{
	foreach (unsigned testid, testid_set_)
		sec_man_.output_results(out, testid);
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

