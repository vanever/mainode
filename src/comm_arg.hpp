#ifndef COMM_ARG_HPP
#define COMM_ARG_HPP

#include <string>
#include <vector>
#include <boost/noncopyable.hpp>

struct CommArg : boost::noncopyable
{
	bool parse_info_file(const char * file);
	bool parse_host_file(const char * file);

	std::string    device;
	std::string    out_file;
	std::string    log_file;
	std::string    extract_addr;
	std::string    bind_ip;
	std::vector<std::string> images;
	std::vector<std::string> videos;
	std::vector<std::string> sources;
	std::vector<std::string> libs;
	std::vector<std::string> image_libs;
	std::vector<std::string> client_addrs;
	std::vector<std::string> speed_files;

	std::string file_to_match;
	int nbits;
	int framegroup_size;
	int balance_threshold;
	double node_speed;
	int merge_unit;
	int match_threshold;
	int merge_threshold;

	unsigned lib_id;
	unsigned max_num_node;
	std::string command_bind_ip;
	unsigned short command_bind_port;

	int debug;

	unsigned short server_port;
	unsigned short send_port;
	unsigned short fpga_port;

	int max_conn_times_allowed;
	int conn_build_wait_time;

	int num_clients;

	int video_beg;
	int video_end;

	int do_surf;

	int match_timeout;
	int match_width;

	int consider_extract_mac;

	//---------------------------------------
	// random-loop mode related
	int random_loop;
	int image_ratio;
	int video_ratio;
	int image_pause;
	int video_pause;

	//----------------------------------------
	// debug
	int manually_broadcast_points;
  	int sleep_time;
	int dump_matched_lib;
	int only_transform_lib;
	int only_send_lib;
	int only_send_image;
	int do_send_lib;

	//---------------------------------------------------------------------------------- 
	// module id
	unsigned mainode_id;
	unsigned pdss_id;

	//---------------------------------------------------------------------------------- 
	// source data control
	unsigned packet_pause;
	unsigned group_pause;
	int use_packet_pause;
	unsigned max_send_speed; // frames/second
	unsigned load_update_interval;

	//---------------------------------------------------------------------------------------
	// static

	static CommArg & comm_arg()
	{
		static CommArg c;
		return c;
	}

private:

	CommArg()
		: video_beg(-1), video_end(-1)
	{}

};

#endif
