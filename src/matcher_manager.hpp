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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

typedef boost::asio::ip::address ip_address;

class Matcher;
class MatcherManager;
struct DomainInfo;
struct AppId;

typedef AppId DomainType;
typedef boost::shared_ptr<DomainInfo> DomainInfoPtr;
typedef boost::shared_ptr< Matcher > MatcherPtr;

typedef std::map<DomainType, DomainInfoPtr> DomainMap;

inline U64 ntohll(U64 x) { return ((U64)htonl((x & 0xFFFFFFFF)) << 32) | htonl(x >> 32); }

enum MATCHER_TYPE { INVALID_MATCHER_TYPE = -1, CPU, HRCA };

struct AppId
{
	U16 DomainId;
	U64 TimeStamp;

	std::string to_string() const {
		using namespace boost::posix_time;
		using namespace boost::gregorian;
		ptime pt(date(1970, Jan, 1), milliseconds(ntohll(TimeStamp)));
		return std::string("App-") + boost::lexical_cast<string>(htons(DomainId)) + "-" + to_simple_string(pt);
	}
} __attribute__((packed));

bool operator < (const AppId & lhs, const AppId & rhs);
bool operator == (const AppId & lhs, const AppId & rhs);
std::ostream & operator << (std::ostream & os, const AppId & appid);

struct DomainInfo
{
	U08 PacketType;
	U08 PacketIndex;
	U16 PacketLength;
	AppId ApplicationId;

	U32 PresentationId;
	U32 TerminalId;
	U32 MonitorId;
	U32 LoadBalanceId;

	U32 LeftNeighborId;
	U32 RightNeighborId;

	U32 HighLoadThreshold;
	U16 LowLoadThreshold;
	U16 BalanceThreshold;

	U16 DCSPTimeout;
	U16 CommTimeout;
	U08 OutputTimeout;
	U08 ReportCycle;

	U08 NBits;
	U08 MergeUnit;
	U08 SliceLen;
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

	static DomainInfoPtr find_domain_by_id(DomainType id)
	{
		DomainMap::iterator it = domains.find(id);
		if (it != domains.end())
			return it->second;
		return DomainInfoPtr();
	}

	static bool add_domain(DomainInfoPtr pd)
	{
		return domains.insert(std::make_pair(pd->ApplicationId, pd)).second;
	}

	static DomainInfoPtr make_domain(char * pc)
	{
		DomainInfoPtr pd( new DomainInfo );
		memcpy(pd.get(), pc, sizeof(DomainInfo));
		return pd;
	}

	static DomainMap domains;	// public

} __attribute__ ((packed)) ;

//---------------------------------------------------------------------------------- 
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
	Matcher ( const endpoint_type & e, DomainType d );
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

	DomainType domain_id() const { return domain_id_; }
	void set_domain_id(DomainType d) { domain_id_ = d; }

	bool first_flag() const { return first_flag_; }
	void demark_first_flag() { first_flag_ = false; }

	U08 result_count() const { return result_count_; }
	void increse_result_count() { ++result_count_; }
	void clear_result_count() { result_count_ = 0; }

	U08 bitfeature_count() const { return bitfeature_count_; }
	void increse_bitfeature_count() { ++bitfeature_count_; }
	void clear_bitfeature_count() { bitfeature_count_ = 0; }

	static MATCHER_TYPE node_type(const endpoint_type & e);

private:

	boost::mutex monitor_;
	MATCHER_STATE state_;
	MATCHER_TYPE type_;
	endpoint_type endpoint_;
	unsigned num_load_;
	unsigned nbits_;
	unsigned merge_unit_;
	unsigned match_threshold_;
	unsigned merge_threshold_;
	DomainType domain_id_;

	bool first_flag_;
	U08 result_count_;
	U08 bitfeature_count_;

};

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
	void set_matcher_load( const Matcher & m , unsigned load );

	void print_matchers     (std::ostream & out) const;

	static MatcherPtr make_matcher( const Matcher::endpoint_type & e, DomainType domain_id )
	{ return MatcherPtr( new Matcher(e, domain_id) ); }
	static MatcherPtr make_matcher( const std::string & ip, DomainType domain_id )
	{ return make_matcher( Matcher::endpoint_type(ip_address::from_string(ip), CommArg::comm_arg().fpga_port), domain_id ); }

	void connect_matcher( const Matcher & m );
//	void connect_matcher( const Matcher::endpoint_type & e );

	MatcherPtr find( const Matcher & m ) const;
	MatcherPtr find_one_matcher_at_state( Matcher::MATCHER_STATE s );
	MatcherPtr find_one_matcher_at_domain_and_state( const Matcher::MATCHER_STATE s, DomainType d );
	Matchers find_matchers_at_state( Matcher::MATCHER_STATE s );
	Matchers find_matchers_at_domain( DomainType d );
	Matchers find_matchers_at_domain_and_state( DomainType d, Matcher::MATCHER_STATE s );

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
	MatcherPtr choose_next_matcher_for_domain(DomainType d);
	MatcherPtr choose_next_matcher();

	void notify_wait_ready_matcher();

	void update_all_matcher_load();
	void update_all_matcher_load(unsigned elapsed_time);

	void start_timer();

private:

	MatcherManager();

	void add_matcher( MatcherPtr m, Matchers & ms );
	void print_matcher_util( std::ostream & out, const Matchers & ms ) const;

	Matchers matchers_;
	boost::mutex monitor_;
	boost::timer::cpu_timer timer_;
	boost::condition c_wait_;

	static MatcherManager * instance_;

};

#endif
