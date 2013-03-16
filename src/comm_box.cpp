#include "comm_box.hpp"
#include "server.hpp"

//--------------------------------------------------------------------------------------- 
// base box

bool Basebox::all_put() const { return all_put_; }

void Basebox::notify_all_put()
{
	lock lk(monitor);
	all_put_ = true;
	c_get.notify_one();
}

void Basebox::wait_until_box_empty()
{
	lock lk(monitor);
	while ( !box_empty() )
		c_put.wait(lk);
}

void Basebox::wait_put_condition()
{
	lock lk(monitor);
	while ( put_condition_not_satisfied() )
		c_put.wait(lk);
}

void Basebox::notify_wait_put()
{
	lock lk(monitor);
	c_put.notify_one();
}

void Basebox::wait_get_condition()
{
	lock lk(monitor);
	while ( get_condition_not_satisfied() )
		c_get.wait(lk);
}

void Basebox::notify_wait_get()
{
	lock lk(monitor);
	c_get.notify_one();
}
