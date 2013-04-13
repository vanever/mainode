#ifndef MINI_TIMER_HPP
#define MINI_TIMER_HPP

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <map>
#include <list>

class MiniTimer : public boost::noncopyable
{

public:

	typedef boost::function<void (void)> FuncType;
	typedef boost::mutex::scoped_lock lock;

	struct HandlerInfo {
		HandlerInfo(std::size_t time, FuncType h) : add_time(time) , f(h), id(make_id()) {}
		std::size_t add_time;
		FuncType f;
		unsigned id;
		static unsigned make_id() {
			static unsigned id = 0;
			return id++;
		}
	};

	typedef std::list<HandlerInfo> Tasks;
	typedef std::map< unsigned, Tasks > TaskMap;

	static MiniTimer & instance(unsigned unit = 20);	// default 20 ms

	unsigned add_period_task    (unsigned interval, FuncType f);
	unsigned add_onetime_task   (unsigned interval, FuncType f);
	void     remove_period_task (unsigned id);

	unsigned unit() const { return unit_; }

	/// not blocked, start run loop in another thread
	void run();

	void stop();

private:

	MiniTimer(unsigned);

	void remove_task_from(unsigned, TaskMap & taskmap);
	unsigned add_task_to(unsigned, FuncType f, TaskMap & taskmap);

	/// blocked
	void do_run();

	/// timer handler
	void timer_handler(const boost::system::error_code &);

	unsigned unit_;
	std::size_t tick_;

	boost::mutex monitor_;
	boost::asio::io_service io_;
	boost::asio::deadline_timer timer_;

	TaskMap period_tasks_;
	TaskMap onetime_tasks_;

};

#endif
