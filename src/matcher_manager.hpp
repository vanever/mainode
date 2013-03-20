#ifndef MATCHER_MANAGER_HPP
#define MATCHER_MANAGER_HPP

#include "log.h"
#include "utils.h"
#include "comm_arg.hpp"
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>
#include <boost/foreach.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/timer/timer.hpp>
#include <boost/thread/condition.hpp>

typedef boost::asio::ip::address ip_address;

class Matcher;
class MatcherManager;
struct DomainInfo;

typedef boost::shared_ptr<DomainInfo> DomainInfoPtr;
typedef boost::shared_ptr< Matcher > MatcherPtr;

typedef std::map<unsigned short, DomainInfoPtr> DomainMap;

class Matcher
{

public:

	typedef boost::asio::ip::udp::endpoint endpoint_type;
	typedef boost::mutex::scoped_lock lock;

	enum MATCHER_STATE {
		INVALID_MATCHER_STATE = -2,		//<! invalid state
		FAIL_CONNECT,					//<! fail connect
		UNCONNECTED,					//<! unconnected default state
		WAIT_LIB_LOAD,					//<! connected but lib not loaded
		LIB_LOADING,					//<! lib loading
		READY,							//<! ready waiting data
		TO_BE_COLLECTED,				//<! to be collected, won't be target any more
		COLLECTED,						//<! collected, won't be target any more
  		RECEIVING,						//<! receiving data
  		BUSY,							//<! do match state
  		SENDING							//<! sending match result
	};

//	Matcher ( const endpoint_type & e, unsigned nbits, unsigned merge_unit );
	Matcher ( const endpoint_type & e, unsigned short d );
	Matcher ( const endpoint_type & e );

	const endpoint_type to_endpoint() const { return endpoint_; }
	const ip_address    address() const { return endpoint_.address(); }
	unsigned            port   () const { return endpoint_.port(); }

	MATCHER_STATE state() const { return state_; }
	void set_state(MATCHER_STATE s) { lock lk(monitor_); state_ = s; }

	bool ready_receive() const { return (state_ == READY); }

	unsigned merge_unit() const { return merge_unit_; }
	void set_merge_unit(unsigned m);
	unsigned nbits() const { return nbits_; }
	void set_nbits( unsigned n );
	unsigned match_threshold() const { return match_threshold_; }
	void set_match_threshold( unsigned n );
	unsigned merge_threshold() const { return merge_threshold_; }
	void set_merge_threshold( unsigned n );

	unsigned num_load() const { return num_load_; }
	void increase_load(unsigned n) { num_load_ += n; } 
	void set_num_load(unsigned num);

	unsigned short domain_id() const { return domain_id_; }
	void set_domain_id(unsigned short d) { domain_id_ = d; }

private:

	boost::mutex monitor_;
	MATCHER_STATE state_;
	endpoint_type endpoint_;
	unsigned num_load_;
	unsigned nbits_;
	unsigned merge_unit_;
	unsigned match_threshold_;
	unsigned merge_threshold_;
	unsigned short domain_id_;

};

struct AppId
{
	U16 DomainId;
	U64 TimeStamp;
} __attribute__((packed));

struct DomainInfo
{
	U08 PacketType;
	U08 PacketIndex;
	U16 PacketLength;
	AppId ApplicationId;

	U32 PresentationId;
	U32 TerminalId;
	U32 MonitorId;

	U32 HighLoadThreshold;
	U16 LowLoadThreshold;
	U16 BalanceThreshold;

	U16 DCSPTimeout;
	U16 CommTimeout;
	U08 OutputTimeout;
	U08 ReportCycle;

	U08 NBits;
	U08 MergeUnit;
	U08 SectionLen;
	U08 MatchThreshold;
	U08 MergeThreshold;
	U08 ReMergeThrehold;

	U16 LibId;
	U16 StructInfo;
	U16 NodeVersion;

	U16 NodeFrequency;
	U32 SpeedPerPipe;
	U16 BasicPower;
	U08 PowerPerPipe;
	U08 NumTotalPipe;

	static DomainInfoPtr find_domain_by_id(unsigned short id)
	{
		DomainMap::iterator it = domains.find(id);
		if (it != domains.end())
			return it->second;
		return DomainInfoPtr();
	}

	static bool add_domain(DomainInfoPtr pd)
	{
		return domains.insert(std::make_pair(pd->ApplicationId.DomainId, pd)).second;
	}

	static DomainInfoPtr make_domain(char * pc)
	{
		DomainInfoPtr pd( new DomainInfo );
		memcpy(pd.get(), pc, sizeof(DomainInfo));
		return pd;
	}

	static DomainMap domains;	// public

} __attribute__ ((packed)) ;

class MatcherManager
{

public:

	typedef std::vector< MatcherPtr > Matchers;
	typedef boost::mutex::scoped_lock lock;

	/// singleton in multi-thread programing
	/// @return 
	static MatcherManager & instance()
	{
		static boost::mutex monitor;
		if (instance_ == 0)
		{
			boost::mutex::scoped_lock lk(monitor);
			if (instance_ == 0)
			{
				instance_ = new MatcherManager;
			}
		}
		return *instance_;
	}

	bool exists( const Matcher & m ) { return find(m); }
	void add_matcher( MatcherPtr m );
	void remove_matcher( const Matcher & m );

	Matcher::MATCHER_STATE get_matcher_state( const Matcher & m ) const;
	void set_matcher_state( const Matcher & m , Matcher::MATCHER_STATE s );

	void print_matchers     (std::ostream & out) const;

	static MatcherPtr make_matcher( const Matcher::endpoint_type & e, unsigned short domain_id )
	{ return MatcherPtr( new Matcher(e, domain_id) ); }
	static MatcherPtr make_matcher( const std::string & ip, unsigned short domain_id )
	{ return make_matcher( Matcher::endpoint_type(ip_address::from_string(ip), CommArg::comm_arg().fpga_port), domain_id ); }

	void connect_matcher( const Matcher & m );
//	void connect_matcher( const Matcher::endpoint_type & e );

	MatcherPtr find( const Matcher & m ) const;
	MatcherPtr find_one_matcher_at_state( Matcher::MATCHER_STATE s );
	MatcherPtr find_one_matcher_at_domain_and_state( const Matcher::MATCHER_STATE s, unsigned short d );
	Matchers find_matchers_at_state( Matcher::MATCHER_STATE s );
	Matchers find_matchers_at_domain( unsigned short d );
	Matchers find_matchers_at_domain_and_state( unsigned short d, Matcher::MATCHER_STATE s );

	static MatcherPtr find_in( const Matcher & m, const Matchers & ms )
	{
		foreach ( MatcherPtr n, ms )
		{
			if (n->address() == m.address())
			{
				return n;
			}
		}
		return MatcherPtr();
	}

	static const std::string id_to_ip(unsigned id);

	//----------------------------------------------------------------------------------

	/// need load balance
	/// @return 
	MatcherPtr choose_next_matcher_for_domain(unsigned short d);
	MatcherPtr choose_next_matcher();

	void notify_wait_ready_matcher();

	void update_all_matcher_load();

	void start_timer();

private:

	MatcherManager()
	{
		timer_.stop();
	}

	void add_matcher( MatcherPtr m, Matchers & ms );
	void print_matcher_util( std::ostream & out, const Matchers & ms ) const;

	Matchers matchers_;
	boost::mutex monitor_;
	boost::timer::cpu_timer timer_;
	boost::condition c_wait_;

	static MatcherManager * instance_;

};

#endif
