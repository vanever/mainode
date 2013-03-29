/// @file comm_window.hpp
/// 
/// @author X.G. Zhou, Chaofan Yu
/// @version 1.0
/// @date 2012-11-26

#ifndef COMM_WINDOW
#define COMM_WINDOW

#include "comm_box.hpp"

/// packet window
///
/// box related with packets are called windows
/// their difference compared with comm_boxes are that
/// they need CONFIRM
/// @tparam N circular queue size
template < unsigned int N >
class PacketWindow : public Basebox
{

public:

	void reset()
	{
		lock lk(monitor);
		all_put_ = false;
		consider_timeout = false;
		head = tail = curr = 0;
	}

	virtual std::size_t capacity() const { return N; }

	virtual std::size_t num_elements() const
	{
		std::size_t t = tail;
		std::size_t h = head;
		if ( t < h )
			t += N + 1;
		return t - h;
	}

	/// specific get for:
	/// get in window actually doesn't clear it
	/// @return 
	virtual bool get_condition_not_satisfied() const
	{
		return curr == tail;
	}

	Packet * acquire()
	{
		return &circular_buf[tail];
	}
	
	void put()
	{
		lock lk(monitor);
		// FDU_LOG(DEBUG) << "head: " << head << " curr: " << curr << " tail: " << tail;
		while ( put_condition_not_satisfied() )
			c_put.wait(lk);

		// timeout control
		if (!consider_timeout)
		{	// enable timeout
			consider_timeout = true;
		}

		// put it && notify get-thread to get
		tail = next(tail);
		c_get.notify_one();
	}

	
	Packet * get()
	{
		lock lk(monitor);
		while ( get_condition_not_satisfied() )
		{
			if ( end_condition_not_satisfied() == false )
			{	// reach end
				return 0;
			}
			if (!c_get.timed_wait(lk, boost::posix_time::milliseconds(TIMEOUT)) && consider_timeout)
			{	// time out
				FDU_LOG(INFO) << name << ": time out, send again";
				for (int i = head; i != curr; i = next(i))
				{
					FDU_LOG(DEBUG) << ( boost::format("type: %02x index: %03d not responsed") % (int)circular_buf[i].type() % (int)circular_buf[i].index() );
				}
				curr = head;				// resend all packets
			}
		}
		int wanted = curr;
		curr = next(curr);
		return &circular_buf[wanted];
	}

	bool confirm(unsigned char index)
	{	
		lock lk(monitor);
		for (int nh = head; nh != curr; nh = next(nh))
			if (index == circular_buf[nh].index())
			{
				head = next(nh);				// 允许批量确认
				c_put.notify_one();
				if ( box_empty() )
				{
					if (consider_timeout)
					{	// disable timeout
						consider_timeout = false;
					}
					if ( all_put() )
					{	// enable get thread to quit
						c_get.notify_one();
					}
				}
				return true;
			}
		return false;
	}

protected:
	
	explicit PacketWindow(const std::string & name_, int timeout = 20)
		: name(name_)
		, circular_buf( N + 1 )
		, TIMEOUT( timeout )
	{
		reset();
		FDU_LOG(DEBUG) << name << " constructed";
	}

	static std::size_t next(std::size_t p) { return (p + 1) % (N + 1); }

	std::size_t tail;	// 队列尾，指向一个空包，由acquire()返回
	std::size_t head;	// 队列头，由confirm调整
	std::size_t curr;	// 指向当前发送的包，由get()返回
	std::string name;
	std::vector<Packet> circular_buf;
	bool consider_timeout;
	const int TIMEOUT;

};

typedef PacketWindow<1> StepWindow;

typedef PacketWindow<16> surf_lib_window_base__;

class SurfLibWindow : public surf_lib_window_base__
{

public:

	explicit SurfLibWindow(int timeout = 20)
		: surf_lib_window_base__("SurfLibWindow", timeout)
	{
	}

};

typedef PacketWindow<16> bitfeature_window_base__;

class BitFeatureWindow : public bitfeature_window_base__
{

public:

	explicit BitFeatureWindow(int timeout = 20)
		: bitfeature_window_base__("BitFeatureWindow", timeout)
	{
	}

};

typedef PacketWindow<16> command_window_base__;

class CommandWindow : public command_window_base__
{

public:

	explicit CommandWindow(int timeout = 100)
		: command_window_base__("CommandWindow", timeout)
	{
	}

};

#endif
