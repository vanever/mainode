#include "matcher_manager.hpp"
#include "connection_build_service.hpp"
#include "lib_load_service.hpp"
#include "server.hpp"
#include <boost/lexical_cast.hpp>

using namespace std;

DomainMap DomainInfo::domains;

namespace {
	bool load_compare(MatcherPtr lhs, MatcherPtr rhs, unsigned short thres)
	{
		unsigned lload = lhs->num_load();
		unsigned rload = rhs->num_load();
		if (lload < thres && rload < thres)
			return !(lload < rload);
		else
			return lload < rload;
	}
}

//--------------------------------------------------------------------------------------- 

// Matcher::Matcher ( const endpoint_type & e, unsigned nbits, unsigned merge_unit )
//     : state_( UNCONNECTED )
//     , endpoint_( e )
//     , num_load_( 0 )
//     , domain_id( 0 )
// {
//     set_nbits(nbits);
//     set_merge_unit(merge_unit);
// }

Matcher::Matcher ( const endpoint_type & e )
	: state_( UNCONNECTED )
	, endpoint_( e )
	, num_load_( 0 )
	, domain_id_( 0 )
{
}

Matcher::Matcher ( const endpoint_type & e, unsigned short d )
	: state_( UNCONNECTED )
	, endpoint_( e )
	, num_load_( 0 )
	, domain_id_( 0 )
{
	if (DomainInfoPtr pd = DomainInfo::find_domain_by_id(d))
	{
		set_domain_id(d);
		set_nbits(pd->NBits);
		set_merge_unit(pd->MergeUnit);
//		set_match_threshold(d->MatchThreshold);
//		set_merge_threshold(d->MergeThreshold);
	}
	else
		FDU_LOG(ERR) << "Unknown domain id " << d;
}

// bool operator < (MatcherPtr lhs, MatcherPtr rhs)
// {
//     unsigned lload = lhs->num_load();
//     unsigned rload = rhs->num_load();
//     unsigned thres = CommArg::comm_arg().balance_threshold;
//     if (lload < thres && rload < thres)
//         return !(lload < rload);
//     else
//         return lload < rload;
// }

void Matcher::set_merge_unit(unsigned m)
{
	lock lk(monitor_);
	ASSERTS( m == 8 || m == 32 || m == 16 );
	merge_unit_ = m;

	static unsigned thres_table[] = { 6, 12, 24, 24 };
	unsigned idx = (m >> 3) - 1;
	merge_threshold_ = thres_table[idx];
}

void Matcher::set_nbits( unsigned n )
{
	lock lk(monitor_);
	ASSERTS( n == 32 || n == 64 || n == 128 );
	nbits_ = n;

	static unsigned thres_table[] = { 3, 6, 12, 12 };
	unsigned idx = (n >> 5) - 1;
	match_threshold_ = thres_table[idx];
}

void Matcher::set_match_threshold( unsigned n )
{
	ASSERTS(n < nbits());
	match_threshold_ = n;
}

void Matcher::set_merge_threshold( unsigned n )
{
	ASSERTS(n <= merge_unit());
	merge_threshold_ = n;
}

void Matcher::set_num_load(unsigned num)
{
	lock lk(monitor_);
	num_load_ = num;
}

//--------------------------------------------------------------------------------------- 

MatcherManager * MatcherManager::instance_ = 0;

void MatcherManager::add_matcher( MatcherPtr m )
{
	add_matcher(m, matchers_);
	ConnectionBuilderService::instance().notify_wait_matcher();		// !!! notify point
}

void MatcherManager::print_matcher_util( ostream & out, const Matchers & ms ) const
{
	int idx = 0;
	foreach (MatcherPtr m, ms)
	{
		out << "  " << m->to_endpoint() << "\t\t# " << ++idx;
	}
}

void MatcherManager::add_matcher( MatcherPtr m, Matchers & ms )
{
	lock lk(monitor_);
	if (!find(*m))
	{
		ms.push_back(m);
	}
}

void MatcherManager::remove_matcher( const Matcher & m )
{
	lock lk(monitor_);
	Matchers::iterator it = matchers_.begin(), it_end = matchers_.end();
	for (; it != it_end; ++it)
	{
		if ((*it)->address() == m.address())
			break;
	}
	if (it != it_end)
		matchers_.erase(it);
}

void MatcherManager::set_matcher_state( const Matcher & m , Matcher::MATCHER_STATE s )
{
	if (MatcherPtr n = find(m))
	{
		n->set_state( s );
	}
	else
	{
		FDU_LOG(ERR) << __func__ << ": matcher " << m.to_endpoint() << " not found";
	}
}

void MatcherManager::connect_matcher( const Matcher & m )
{
	lock lk(monitor_);
	if (MatcherPtr n = find(m))
	{
		if (n->state() != Matcher::UNCONNECTED)
		{
			FDU_LOG(WARN) << "matcher already connected: " << m.to_endpoint();
		}
		else
		{
			n->set_state( Matcher::WAIT_LIB_LOAD );
			FDU_LOG(INFO) << m.to_endpoint() << " connection built. " << " Do notify lib load";
			LibLoadService::instance().notify_wait_matcher();
		}
	}
	else
	{
		FDU_LOG(ERR) << __func__ << ": matcher " << m.to_endpoint() << " not found";
	}
}

void MatcherManager::notify_wait_ready_matcher()
{
//	FDU_LOG(INFO) << "MatcherManager notify choose_next_matcher before lock";
//	lock lk(monitor_);
//	FDU_LOG(INFO) << "MatcherManager notify choose_next_matcher after lock";
	c_wait_.notify_one();
}

void MatcherManager::update_all_matcher_load()
{
	lock lk(monitor_);
	const double secs = 1000000000.0;
	timer_.stop();
	double elapsed = timer_.elapsed().wall / secs;
	unsigned reduced = elapsed * CommArg::comm_arg().node_speed;
	Matchers ms(find_matchers_at_state(Matcher::READY));
	foreach (MatcherPtr m, ms)
	{
		m->set_num_load(
				m->num_load() > reduced ? (m->num_load() - reduced) : 0
				);
	}
}

void MatcherManager::start_timer()
{
	lock lk(monitor_);
	timer_.start();
}

/// need fix
/// @param domain
/// @return 
MatcherPtr MatcherManager::choose_next_matcher_for_domain(unsigned short domain)
{
	lock lk(monitor_);
	while ( !find_one_matcher_at_domain_and_state(Matcher::READY, domain) )
	{
		FDU_LOG(INFO) << "BitFeatureLoader wait ready matcher";
		c_wait_.wait(lk);
	}

	unsigned short thres = CommArg::comm_arg().balance_threshold;	// default
	if (DomainInfoPtr pd = DomainInfo::find_domain_by_id(domain))
	{
		thres = pd->BalanceThreshold;	// domain thres
	}
	else
		FDU_LOG(WARN) << __func__ << " not valid domain id: " << domain;

	Matchers ms(find_matchers_at_domain_and_state(domain, Matcher::READY));
	std::sort(ms.begin(), ms.end(), boost::bind(load_compare, _1, _2, thres));
	FDU_LOG(DEBUG) << "BitFeatureLoader choose " << (ms[0])->to_endpoint();
	return ms[0];
}

MatcherPtr MatcherManager::choose_next_matcher()
{
	lock lk(monitor_);
	while ( !find_one_matcher_at_state(Matcher::READY) )
	{
		FDU_LOG(INFO) << "BitFeatureLoader wait ready matcher";
		c_wait_.wait(lk);
	}
	unsigned short thres = CommArg::comm_arg().balance_threshold;	// default
	Matchers ms(find_matchers_at_state(Matcher::READY));
	std::sort(ms.begin(), ms.end(), boost::bind(load_compare, _1, _2, thres));
	FDU_LOG(DEBUG) << "BitFeatureLoader choose " << (ms[0])->to_endpoint();
	return ms[0];
}

MatcherManager::Matchers MatcherManager::find_matchers_at_state( Matcher::MATCHER_STATE s )
{
	Matchers ms;
	foreach ( MatcherPtr m, matchers_ )
	{
		if (m->state() == s)
		{
			ms.push_back(m);
		}
	}
	return ms;
}

MatcherManager::Matchers MatcherManager::find_matchers_at_domain_and_state( unsigned short d, Matcher::MATCHER_STATE s )
{
	Matchers ms;
	foreach ( MatcherPtr m, matchers_ )
	{
		if (m->domain_id() == d && m->state() == s)
		{
			ms.push_back(m);
		}
	}
	return ms;
}

MatcherManager::Matchers MatcherManager::find_matchers_at_domain( unsigned short d )
{
	Matchers ms;
	foreach ( MatcherPtr m, matchers_ )
	{
		if (m->domain_id() == d)
		{
			ms.push_back(m);
		}
	}
	return ms;
}

void MatcherManager::print_matchers     (std::ostream & out) const
{
	FDU_LOG(INFO) << "all matchers list";
	print_matcher_util( out, matchers_ );
}

MatcherPtr MatcherManager::find_one_matcher_at_state( const Matcher::MATCHER_STATE s )
{
	foreach ( MatcherPtr m, matchers_ )
	{
		if (m->state() == s)
		{
			return m;
		}
	}
	return MatcherPtr();
}

MatcherPtr MatcherManager::find_one_matcher_at_domain_and_state( const Matcher::MATCHER_STATE s, unsigned short d )
{
	foreach ( MatcherPtr m, matchers_ )
	{
		if (m->state() == s && m->domain_id() == d)
		{
			return m;
		}
	}
	return MatcherPtr();
}

Matcher::MATCHER_STATE MatcherManager::get_matcher_state( const Matcher & m ) const
{
	if (MatcherPtr n = find(m))
	{
		return n->state();
	}
	else
	{
		FDU_LOG(ERR) << __func__ << ": matcher " << m.to_endpoint() << " not found";
		return Matcher::INVALID_MATCHER_STATE;
	}
}

MatcherPtr MatcherManager::find( const Matcher & m ) const
{
	return find_in( m, matchers_ );
}

const std::string MatcherManager::id_to_ip(unsigned id)
{
	struct NodeId
	{
		unsigned row_id    : 5;
		unsigned col_id    : 5;
		unsigned module_id : 5;
		unsigned unit_id   : 7;
		unsigned device_id : 10;
	} __attribute__((packed));

	ASSERTSD(sizeof(NodeId) == sizeof(unsigned));

	NodeId nodeid;
	memcpy(&nodeid, &id, sizeof(NodeId));

	string head = "172.22.";
	U08 X = nodeid.row_id * 5 + nodeid.col_id + 1;
	U08 Y = (nodeid.module_id + 1) * 10 + nodeid.unit_id;
	return head +
		boost::lexical_cast<string>(X) + '.' +
		boost::lexical_cast<string>(Y);

}
