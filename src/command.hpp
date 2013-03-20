#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "matcher_manager.hpp"

namespace DCSP {

	const unsigned MSG_ADD_NODES        = 0x00010010;
	const unsigned MSG_DEL_NODES        = 0x00010011;
	const unsigned MSG_TELL_LOADS       = 0x00010024;
	const unsigned MSG_CREATE_DOMAIN    = 0x00010034;

}

/// interface class
class ICommand
{

public:

	virtual void execute() = 0;
	virtual void reply  ();

	static const std::string unsigned_to_string (unsigned u);

};

//---------------------------------------------------------------------------------- 
class AddNodesCommand : public ICommand
{

public:

	typedef std::set<unsigned> Nodes;

	AddNodesCommand(AppId id)
		: app_id_(id)
	{
	}

	void add_target_node(unsigned node_id)
	{
		nodes_.insert(node_id);
	}

	const Nodes & target_nodes() { return nodes_; }

	virtual void execute();
	virtual void reply  ();

private:

	AppId app_id_;
	Nodes nodes_;

};

//---------------------------------------------------------------------------------- 
class RemoveNodesCommand : public ICommand
{

public:

	typedef std::set<unsigned> Nodes;

	RemoveNodesCommand(AppId id)
		: app_id_(id)
	{
	}

	void add_target_node(unsigned node_id)
	{
		nodes_.insert(node_id);
	}

	const Nodes & target_nodes() { return nodes_; }

	virtual void execute();
	virtual void reply  ();

private:

	AppId app_id_;
	Nodes nodes_;

};

//---------------------------------------------------------------------------------- 
class DomainCreateCommand : public ICommand
{

public:

	DomainCreateCommand(DomainInfoPtr pd);

	virtual void execute();
	virtual void reply  ();

private:

	DomainInfoPtr domain_;

};

//---------------------------------------------------------------------------------- 
class QueryLoadsCommand : public ICommand
{

public:

	// TODO

	virtual void execute() {}
	virtual void reply  () {}

private:


};

//---------------------------------------------------------------------------------- 
class TerminalChangeCommand : public ICommand
{

public:

	// TODO

	virtual void execute() {}
	virtual void reply  () {}

private:


};

typedef boost::shared_ptr<ICommand> CommandPtr;

#endif
