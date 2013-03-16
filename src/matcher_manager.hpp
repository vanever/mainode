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
class SortMatchers;
class MatcherManager;

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

	enum COMM_STATE {
		INVALID_COMM_STATE = -1,		//<! invalid 
		RESPONSED,						//<! last packet responsed 
		UNRESPONSED						//<! last packet not responsed
	};

	Matcher ( const endpoint_type & e )
		: state_( UNCONNECTED )
		, comm_state_( RESPONSED )
		, endpoint_( e )
		, merge_threshold_(8)
		, match_threshold_(1)
	{
	}

	const endpoint_type to_endpoint() const { return endpoint_; }
	const ip_address    address() const { return endpoint_.address(); }
	unsigned            port   () const { return endpoint_.port(); }

	MATCHER_STATE state() const { return state_; }
	void set_state(MATCHER_STATE s) { lock lk(monitor_); state_ = s; }

	COMM_STATE comm_state() const { return comm_state_; }
	void set_comm_state(COMM_STATE s) { lock lk(monitor_); comm_state_ = s; }

	bool ready_receive() const { return (state_ == READY) && comm_state_ == RESPONSED; }

	unsigned merge_unit() const { return merge_unit_; }
	void set_merge_unit(unsigned m);
	unsigned nbits() const { return nbits_; }
	void set_nbits( unsigned n );
	unsigned match_threshold() const { return match_threshold_; }
	unsigned merge_threshold() const { return merge_threshold_; }

	unsigned num_load() const { return num_load_; }
	void increase_load(unsigned n) { num_load_ += n; } 
	void set_num_load(unsigned num);


private:

	boost::mutex monitor_;
	MATCHER_STATE state_;
	COMM_STATE comm_state_;		// currently not used
	endpoint_type endpoint_;
	unsigned num_load_;
	unsigned merge_unit_;
	unsigned nbits_;
	unsigned match_threshold_;
	unsigned merge_threshold_;

};

typedef boost::shared_ptr< Matcher > MatcherPtr;

bool operator < (MatcherPtr lhs, MatcherPtr rhs);

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

	void set_matcher_comm_state( MatcherPtr m , Matcher::COMM_STATE s ) { set_matcher_comm_state( *m, s ); }
	void set_matcher_comm_state( const Matcher::endpoint_type & e, Matcher::COMM_STATE s ) { set_matcher_comm_state( Matcher(e), s ); }
	void set_matcher_comm_state( const Matcher & m , Matcher::COMM_STATE s );

	Matcher::MATCHER_STATE get_matcher_state( const Matcher & m ) const;

	Matcher::COMM_STATE get_comm_state( const Matcher & m ) const;

	void set_matcher_state( const Matcher & m , Matcher::MATCHER_STATE s );

	void print_matchers     (std::ostream & out) const;

	static MatcherPtr make_matcher( const Matcher::endpoint_type & e )
	{
		return MatcherPtr( new Matcher(e) );
	}

	void connect_matcher( const Matcher & m );

	MatcherPtr find( const Matcher & m ) const;

	MatcherPtr find_one_matcher_at_state( const Matcher::MATCHER_STATE s );

	Matchers find_matchers_at_state( const Matcher::MATCHER_STATE s );

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

	//----------------------------------------------------------------------------------

	/// need load balance
	/// @return 
	MatcherPtr choose_next_matcher();

	void notify_wait_ready_matcher();
	bool no_available_matcher();

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
//	Matchers conn_matchers_;
//	SortMatchers ready_mathers_;
	boost::mutex monitor_;
	boost::timer::cpu_timer timer_;
	boost::condition c_wait_;

	static MatcherManager * instance_;

};

#endif
