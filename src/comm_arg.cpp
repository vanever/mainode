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

//	device        = pt.get<string>("device");
//	extract_addr  = pt.get<string>("extract_addr");
//	video_beg     = pt.get<int>("video_beg", 300);
//	video_end     = pt.get<int>("video_end", 420);
//	num_clients   = pt.get<int>("num_clients");
//	do_surf       = pt.get<int>("do_surf", 0);
//	match_timeout = pt.get<int>("match_timeout", 300);
//	match_width   = pt.get<int>("match_width", MATCH_WIDTH);
//	consider_extract_mac = pt.get<int>("consider_extract_mac", 0);

//	random_loop   = pt.get<int>("random_loop", 0);
//	image_ratio   = pt.get<int>("image_ratio", 0);	// default no image
//	video_ratio   = pt.get<int>("video_ratio", 1);
//	image_pause   = pt.get<int>("image_pause", 500);
//	video_pause   = pt.get<int>("video_pause", 2000);

//	manually_broadcast_points = pt.get<int>("manually", 0);
//  sleep_time                = pt.get<int>("sleep_time", 1000);
//	dump_matched_lib          = pt.get<int>("dump_matched_lib", 0);
//	only_transform_lib        = pt.get<int>("only_transform_lib", 0);
//	only_send_lib             = pt.get<int>("only_send_lib", 0);
//	only_send_image           = pt.get<int>("only_send_image", 0);
//	do_send_lib               = pt.get<int>("do_send_lib", 0);

	out_file      = pt.get<string>("out_file", "match_result.xml");
	log_file      = pt.get<string>("log_file", "log");
	bind_ip       = pt.get<string>("bind_ip");

	server_port               = pt.get<unsigned short>("server_port", 0xFD01);
	send_port                 = pt.get<unsigned short>("send_port", 0xFD02);
	fpga_port                 = pt.get<unsigned short>("fpga_port", 0xFD00);

	max_conn_times_allowed    = pt.get<int>("max_conn_times_allowed", 5);
	conn_build_wait_time      = pt.get<int>("conn_build_wait_time", 200);

	// add 2013/02/22
	file_to_match      = pt.get<string>("file_to_match");
	nbits              = pt.get<int>("nbits", 32);
	framegroup_size    = pt.get<int>("framegroup_size", 512);		// need confirm
	balance_threshold  = pt.get<int>("balance_threshold", 1024);	// need confirm
	merge_unit         = pt.get<int>("merge_unit", 8);
	match_threshold    = pt.get<int>("match_threshold", 1);	// need confirm
	merge_threshold    = pt.get<int>("merge_threshold", 7);	// need confirm
	node_speed         = pt.get<double>("node_speed", 800);	// need confirm
	ASSERT(file_to_match.size(), "empty file_to_match");

	// add 2013/2/26
	lib_id = pt.get<unsigned>("lib_id", 0);
	max_num_node = pt.get<unsigned>("max_num_node");
	command_bind_port = pt.get<unsigned short>("command_bind_port");
	command_bind_ip = pt.get<std::string>("command_bind_ip");
	debug = pt.get<int>("debug");

	foreach (ptree::value_type &v, pt.get_child("libs"))
		libs.push_back(v.second.get_value<string>());
	foreach (ptree::value_type &v, pt.get_child("videos"))
		videos.push_back(v.second.get_value<string>());
	foreach (ptree::value_type &v, pt.get_child("images"))
		images.push_back(v.second.get_value<string>());

	ASSERT(bind_ip.size(), "bind ip empty");
	ASSERT(command_bind_ip.size(), "command bind ip empty");
//	ASSERT(device.size(), "empty device");
//	ASSERT(videos.size() || images.size(), "no image or video");
//	ASSERT(video_beg >= 0 && video_end > 0, "no begin end pos");

	if (only_send_image) do_surf = 0;

	FDU_LOG(INFO) << "libs num: " << libs.size();
	FDU_LOG(INFO) << "images num: " << images.size();
	FDU_LOG(INFO) << "videos num: " << videos.size();
	FDU_LOG(INFO) << "do surf: " << (do_surf ? "true" : "false");

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
