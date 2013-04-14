#include "utils.h"
#include "comm_arg.hpp"
#include "comm_para.hpp"
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <fstream>
#include <iostream>
#include "log.h"

#include <boost/foreach.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>

using namespace std;

bool CommArg::parse_info_file( const char * file )
{
	using namespace boost::property_tree;
	ptree pt;
	read_info(file, pt);

	string log_file      = pt.get<string>("log_file", "mainode_%N.log");
	unsigned log_rotation_size = pt.get<unsigned>("log_rotation_size", 0);
	LOG::init_log_file(log_file, log_rotation_size);

	out_file      = pt.get<string>("out_file", "match_result_%d.xml");

	bind_ip       = pt.get<string>("bind_ip");
	server_port               = pt.get<unsigned short>("server_port", 0xFD01);
	send_port                 = pt.get<unsigned short>("send_port", 0xFD02);
	fpga_port                 = pt.get<unsigned short>("fpga_port", 0xFD00);

	max_conn_times_allowed    = pt.get<int>("max_conn_times_allowed", 5);
	conn_build_wait_time      = pt.get<int>("conn_build_wait_time", 200);

	// add 2013/02/22
	nbits              = pt.get<int>("nbits", 32);
	framegroup_size    = pt.get<int>("framegroup_size", 512);		// need confirm
	balance_threshold  = pt.get<int>("balance_threshold", 1024);	// need confirm
	merge_unit         = pt.get<int>("merge_unit", 8);
	match_threshold    = pt.get<int>("match_threshold", 1);	// need confirm
	merge_threshold    = pt.get<int>("merge_threshold", 7);	// need confirm
	node_speed         = pt.get<double>("node_speed", 800);	// need confirm

	// add 2013/2/26
	lib_id = pt.get<unsigned>("lib_id", 0);
	max_num_node = pt.get<unsigned>("max_num_node");
	command_bind_port = pt.get<unsigned short>("command_bind_port");
	command_bind_ip = pt.get<std::string>("command_bind_ip");
	debug = pt.get<int>("debug");

	//---------------------------------------------------------------------------------- 
	group_pause = pt.get<unsigned>("group_pause", 10000);
	packet_pause = pt.get<unsigned>("packet_pause", 3000);
	use_packet_pause = pt.get<unsigned>("use_packet_pause", 1);
	update_load = pt.get<unsigned>("update_load", 1);
	max_send_speed = pt.get<unsigned>("max_send_speed", 100000);	// need confirm
	load_update_interval = pt.get<unsigned>("load_update_interval", 2);	// 20ms as a unit, 2 means 2 * 20ms

	disable_sending_report_timeout = pt.get<int>("disable_sending_report_timeout", 0);	// 20ms as a unit, 2 means 2 * 20ms

	ASSERT(bind_ip.size(), "bind ip empty");
	ASSERT(command_bind_ip.size(), "command bind ip empty");

	path_format = pt.get<std::string>("path_format", "%s_%d.bin");

	foreach (ptree::value_type &v, pt.get_child("load_args")) {
		LoadArg arg;
		ASSERTS(stoi(v.second.data()) == load_args.size());
		arg.lib = v.second.get<string>("lib");
		arg.source = v.second.get<string>("source");
		arg.source_repeat_times = v.second.get<unsigned>("source_repeat_times");
		arg.speed_file = v.second.get<string>("speed_file");
		load_args.push_back(arg);
	}
	ASSERT(load_args.size(), "load_args empty");

	return true;
}

bool CommArg::parse_host_file( const char * file )
{
	ifstream in( file );
	string line;
	FDU_LOG(INFO) << "clients address list: \n";
	int idx = 0;
	while ( getline(in, line) )
	{
		boost::trim( line );
		if (line.empty() || line[0] == '#')
			continue;
		this->client_addrs.push_back( line );
		FDU_LOG(INFO) << "\t NO." << ++idx << "\t: "<< line;
	}
	num_clients = client_addrs.size();
	if (num_clients == 0)
		FDU_LOG(WARN) << "no matchers in host file";
	else
		FDU_LOG(INFO) << "num clients: " << client_addrs.size();
	return true;
}
