#include "server.hpp"
#include <boost/timer/timer.hpp>

using namespace std;

#define EXCEPTION_HANDLE

int main(int argc, char *argv[])
{

#ifdef EXCEPTION_HANDLE
	try {
#endif

		string  cfg_file("match.cfg");
		string host_file("host_file");

		if (argc != 3)
		{
			FDU_LOG(WARN) << "usage: " << argv[0] << " cfg_file host_file";
			FDU_LOG(WARN) << "using default: match.cfg host_file";
		}
		else
		{
			cfg_file  = string(argv[1]);
			host_file = string(argv[2]);
		}

		boost::timer::cpu_timer c;
		LOG::init_log_file("log");
		LOG::set_console_filter(LOG::flt::attr<LOG::severity_level>("Severity") >= LOG::INFO);

		if (!CommArg::comm_arg().parse_info_file( cfg_file.c_str() ))
		{
			cout << "error parsing config file" << endl;
			return -1;
		}
		if (!CommArg::comm_arg().parse_host_file( host_file.c_str() ))
		{
			cout << "error parsing host file" << endl;
			return -1;
		}

		Server::construct_server( &CommArg::comm_arg() );
		Server::instance().start_main();
//		Server::instance().wait_all_end();
		cout << "time all: " << c.format() << endl;

#ifdef EXCEPTION_HANDLE
	}
	catch (exception& e) {
		cout << "error occurred: " << e.what() << endl;
		return -1;
	}
	catch (...) {
		cout << "unknown error occurred" << endl;
		return -1;
	}
#endif

	return 0;
}
