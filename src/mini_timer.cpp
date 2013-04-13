#include "mini_timer.hpp"
#include "log.h"
#include <boost/bind.hpp>
#include "utils.h"

using namespace std;

typedef MiniTimer::FuncType FuncType;
typedef MiniTimer::TaskMap  TaskMap;
typedef MiniTimer::Tasks    Tasks;
typedef MiniTimer::HandlerInfo HandlerInfo;

namespace {

	bool meet_condition(unsigned tick, unsigned interval, HandlerInfo h)
	{
		return (tick - h.add_time) % interval == 0;
	}

	unsigned add_func_to_list(FuncType f, Tasks & tasks, unsigned tick)
	{
		HandlerInfo h(tick, f);
		tasks.push_back(h);
		return h.id;
	}

	bool remove_func_from_list(unsigned id, Tasks & tasks)
	{
		auto it = tasks.begin(), it_end = tasks.end();
		for (; it != it_end; ++it)
		{
			if (it->id == id)
				break;
		}
		if (it != tasks.end())
		{
			tasks.erase(it);
			return true;
		}
		else
		{
			return false;
		}
	}

}

MiniTimer::MiniTimer (unsigned unit)
	: unit_(unit)
	, tick_(0)
	, io_()
	, timer_(io_)
{
}

unsigned MiniTimer::add_task_to(unsigned interval, FuncType f, TaskMap & taskmap)
{
	lock lk(monitor_);
	auto it = taskmap.find(interval);
	if (it != taskmap.end())
	{
		return add_func_to_list(f, it->second, tick_);
	}
	else
	{
		taskmap.insert(make_pair(interval, Tasks()));
		HandlerInfo h(tick_, f);
		taskmap[interval].push_back(h);
		return h.id;
	}
}

void MiniTimer::remove_task_from(unsigned id, TaskMap & taskmap)
{
	lock lk(monitor_);
	bool found = false;
	foreach (TaskMap::value_type & tasks_pair, taskmap)
	{
		if (remove_func_from_list(id, tasks_pair.second))
		{
			found = true;
			break;
		}
	}
	if (!found)
		FDU_LOG(WARN) << "no task for id: " << id;
}

unsigned MiniTimer::add_onetime_task(unsigned interval, FuncType f)
{
	return add_task_to(interval, f, onetime_tasks_);
}

unsigned MiniTimer::add_period_task(unsigned interval, FuncType f)
{
	return add_task_to(interval, f, period_tasks_);
}

void MiniTimer::remove_period_task(unsigned id)
{
	remove_task_from(id, period_tasks_);
}

void MiniTimer::run()
{
	boost::thread t(boost::bind(&MiniTimer::do_run, this));
	t.detach();
}

void MiniTimer::do_run()
{
	timer_.expires_from_now(boost::posix_time::milliseconds(unit_));
	timer_.async_wait(boost::bind(&MiniTimer::timer_handler, this, _1));
	io_.run();
}

void MiniTimer::timer_handler(const boost::system::error_code & ec)
{
	lock lk(monitor_);
	++tick_;
	//FDU_LOG(DEBUG) << "tick count: " << tick_;
	if (!ec)
	{
		foreach (TaskMap::value_type & tasks_pair, period_tasks_)
		{
			std::size_t interval = tasks_pair.first;
			foreach (HandlerInfo h, tasks_pair.second)
			{
				if ((tick_ - h.add_time) % interval == 0)
					(h.f)();
			}
		}
		foreach (TaskMap::value_type & tasks_pair, onetime_tasks_)
		{
			std::size_t interval = tasks_pair.first;
			foreach (HandlerInfo h, tasks_pair.second)
			{
				if ((tick_ - h.add_time) % interval == 0)
					(h.f)();
			}
			tasks_pair.second.remove_if(boost::bind(meet_condition, tick_, interval, _1));
		}
		timer_.expires_from_now(boost::posix_time::milliseconds(unit_));
		timer_.async_wait(boost::bind(&MiniTimer::timer_handler, this, _1));
	}
	else if (ec == boost::asio::error::operation_aborted)
	{
		FDU_LOG(INFO) << "mini timer aborted";
	}
	else
	{
		FDU_LOG(ERR) << __func__ << ": " << ec.message();
	}
}

MiniTimer & MiniTimer::instance(unsigned unit)
{
	static MiniTimer * inst = 0;
	if (inst == 0)
	{
		static boost::mutex monitor;
		boost::mutex::scoped_lock lk(monitor);
		if (inst == 0)
		{
			inst = new MiniTimer(unit);
		}
	}
	return *inst;
}

void MiniTimer::stop()
{
	timer_.cancel();
	io_.stop();
}
