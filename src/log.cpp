#include <iostream>
#include <fstream>

#include <boost/log/utility/init/to_file.hpp>
#include <boost/log/utility/init/to_console.hpp>

#include "log.h"
#include "fdutils.h"	// EnumStringMap

namespace LOG {

using namespace std;
using namespace boost;

static const char* sev_levels[] = { "DEBUG", "VERBOSE", "INFO ", "WARN ", "ERROR" };
static EnumStringMap<severity_level> levelmap(sev_levels);
std::ostream& operator << (std::ostream& s, severity_level level) { return levelmap.writeEnum(s, level); }

Log& Log::get() {
	static Log instance;
	return instance;
}

Log::Log() : core_(logging::core::get()) {
	sink_con_ = logging::init_log_to_console(
		std::clog,
		keywords::format = fmt::format("%1%: %2%")
			%  fmt::attr<severity_level>("Severity")
			% fmt::message(),
		keywords::filter = flt::attr<severity_level>("Severity") >= INFO 
	);
}

void Log::init_log_file(const std::string& log_file, int rotation_size) {
	core_->add_global_attribute("TimeStamp", make_shared<attrs::local_clock>());
	core_->remove_sink(sink_file_);				// remove old sink
	sink_file_ = logging::init_log_to_file(		// add new sink
		keywords::file_name = log_file,
//		keywords::open_mode = std::ios_base::app,
		keywords::auto_flush = true,
		keywords::format = fmt::format("[%1%] %2%: %3%")
			% fmt::date_time<boost::posix_time::ptime>("TimeStamp")
			% fmt::attr<severity_level>("Severity", "%-7s")
			% fmt::message()
	);
	if (rotation_size > 0)
		sink_file_->locked_backend()->set_rotation_size(rotation_size);
}

}
