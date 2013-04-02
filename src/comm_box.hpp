/// @file comm_box.hpp
/// communication boxes for multi-thread programming
/// @author Chaofan Yu
/// @version 1.0
/// @date 2012-11-26

#ifndef COMM_BOX_HPP
#define COMM_BOX_HPP

#include "comm_para.hpp"
#include <vector>
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/thread_time.hpp>
#include <memory.h>
#include <functional>
#include "packet.hpp"
#include "utils.h"

#include "command.hpp"

//---------------------------------------------------------------------------------------
// declaration
class Server;

//--------------------------------------------------------------------------------------- 
// utils functors

template< class T >
struct AssignOp : public std::binary_function< T, T, void >
{
	void operator () (T & lhs, const T & rhs)
	{
		lhs = rhs;
	}
};

template< class T >
struct SwapOp : public std::binary_function< T, T, void >
{
	void operator () (T & lhs, T & rhs)
	{
		lhs.swap(rhs);
	}
};

//--------------------------------------------------------------------------------------- 

class Basebox
{

public:

	typedef boost::mutex::scoped_lock lock;

	bool box_empty() const { return num_elements() == 0; }
	bool box_full()  const { return num_elements() == capacity(); }

	virtual std::size_t num_elements() const = 0;
	virtual std::size_t capacity()     const = 0;

	void wait_until_box_empty();

	void wait_put_condition();
	void wait_get_condition();

	void notify_wait_put();
	void notify_wait_get();

	void notify_all_put();
	bool all_put() const ;
	virtual bool put_condition_not_satisfied() const { return box_full(); }
	virtual bool get_condition_not_satisfied() const { return box_empty(); }
	virtual bool end_condition_not_satisfied() const { return !(all_put() && box_empty()); }
	bool end_condition_satisfied() const { return !end_condition_not_satisfied(); }


protected:

	Basebox()
		: all_put_(false)
	{}

	bool all_put_;
	boost::mutex monitor;
	boost::condition c_put, c_get;

};

template< class T, class MoveOp = AssignOp<T> >
class CommBox : public Basebox
{

public:

	CommBox()
		: data_valid(false)
	{}

	bool get(T & t)
	{
		lock lk(monitor);
		while (get_condition_not_satisfied())
		{
			if ( end_condition_satisfied() )
			{	// read end
				return false;
			}
			c_get.wait(lk);
		}
		MoveOp()(t, data);
		data_valid = false;
		c_put.notify_one();
		return true;
	}

	void put(const T & t)
	{
		lock lk(monitor);
		while (put_condition_not_satisfied())
			c_put.wait(lk);
		data = t;
		data_valid = true;
		c_get.notify_one();
	}

	virtual std::size_t num_elements() const { return data_valid ? 1 : 0; }
	virtual std::size_t capacity()     const { return 1; }

protected:

	T data;
	bool data_valid;

};

template< unsigned int N, class T, class MoveOp = AssignOp<T> > // T: support swap
class RandomGetBox : public Basebox
{

public:

	RandomGetBox()
		: curr(0)
		, tail(0)
		, buff(N + 1) // addtional one to make sure tail is valid
	{}

	/// acquire the data container
	/// @return 
	T * acquire()
	{
		return &buff[tail];
	}

	typedef std::vector<std::size_t> indexes_type;

	/// implement algorithm to choose targets
	/// @return choosed targets
	virtual indexes_type choose_targets() = 0;

	virtual std::size_t num_elements() const { return curr; }
	virtual std::size_t capacity()     const { return N; }

	/// put selected items into snk
	/// @param targets
	/// @param snk
	bool get(std::vector< T > & snk)
	{
		lock lk(monitor);
		while ( get_condition_not_satisfied() )
		{
			if ( end_condition_not_satisfied() == false )
				return false;
			c_get.wait(lk);
		}

		const std::vector<std::size_t> & targets = choose_targets();
		if (targets.size() == 0)
		{
			FDU_LOG(WARN) << "choose nothing OTL ";
			return true;
		}

		// put targets
		snk.clear();
		foreach (std::size_t i, targets)
		{
			ASSERTS( i < curr );
			snk.push_back(buff[i]);
		}

		// restore data structure
		int pos = 0;
		for (std::size_t i = 0; i < curr; i++)
		{
			if ( std::find(targets.begin(), targets.end(), i) == targets.end() )
			{	// buff[i] is not choosen
				MoveOp()( buff[pos++], buff[i] );
			}
		}
		curr -= targets.size();
		c_put.notify_one();
		return true;
	}

	void put()
	{
		lock lk(monitor);
		while ( put_condition_not_satisfied() )
		{
			c_put.wait(lk);
		}
		MoveOp()( buff[curr], buff[tail] );
		curr++; 			// curr: never > N, when curr == N - 1, curr++ make box_full
		tail = curr;		// tail: never > N, tail >= curr for curr maybe reduced by get()
		c_get.notify_one();
	}

protected:

	int curr;
	int tail;
	std::vector< T > buff;

};

#include <queue>
#include <limits>

const std::size_t INFINITE_CAPACITY = std::size_t(-1);
template< class T, std::size_t CAPACITY = INFINITE_CAPACITY >
class CommQueue : public Basebox
{

public:

	virtual std::size_t num_elements() const
	{ return queue_.size(); }
	virtual std::size_t capacity()     const
	{ return CAPACITY; }

	void put(T p)
	{
		lock lk(monitor);
		while (put_condition_not_satisfied())
			c_put.wait(lk);
		queue_.push(p);			// no limit on the size
		c_get.notify_one();
	}

	T get()
	{
		lock lk(monitor);
		while (get_condition_not_satisfied())
		{
			if (end_condition_satisfied())
			{
				return T();
			}
			c_get.wait(lk);
		}
		T p = queue_.front();
		queue_.pop();
		return p;
	}

private:

	typedef typename std::queue< T > Queue;
	Queue queue_;

};

typedef CommQueue< CommandPtr > CommandQueue;
typedef CommQueue< PacketPtr > PacketQueue;

#endif
