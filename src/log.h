#ifndef FDU_LOGGING_H_
#define FDU_LOGGING_H_

#include <string>
#include <iosfwd>
#include <exception>

#include <boost/noncopyable.hpp>
#include <boost/log/common.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/filters.hpp>
#include <boost/log/formatters.hpp>
#include <boost/log/attributes.hpp>

struct AssertionFail : std::runtime_error {
	AssertionFail()  : std::runtime_error("Assertion failed, please check the log file for more information.") {}
};

namespace LOG {
	namespace logging  = boost::log;
	namespace sinks    = boost::log::sinks;
	namespace src      = boost::log::sources;
	namespace flt      = boost::log::filters;
	namespace keywords = boost::log::keywords;
	namespace fmt      = boost::log::formatters;
	namespace attrs    = boost::log::attributes;

	enum severity_level { DEBUG, VERBOSE, INFO, WARN, ERR };
	std::ostream& operator << (std::ostream& s, severity_level level);

	typedef src::severity_logger<severity_level> sev_logger;
	typedef boost::shared_ptr<logging::core>     sp_log_core;
	typedef boost::shared_ptr<sinks::synchronous_sink<sinks::text_file_backend> >    sp_file_sink;
	typedef boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend> > sp_stream_sink;

	class Log : boost::noncopyable {
	public:
		void init_log_file(const std::string& log_file, int rotation_size);

		sev_logger&    logger()    { return slg_;       }
		sp_stream_sink con_sink()  { return sink_con_;  }
		sp_file_sink   file_sink() { return sink_file_; }

		static Log&    get();		// get the singleton instance

	private:
		Log();

		sp_log_core    core_;
		sev_logger     slg_;
		sp_stream_sink sink_con_;
		sp_file_sink   sink_file_;
	};

	inline sev_logger& get_logger() { return Log::get().logger(); }

	inline void init_log_file(const std::string& log_file, int rotation_size = 0)
	{ Log::get().init_log_file(log_file, rotation_size); }

	template<typename SP, typename FMT> inline void set_format(SP sink, FMT const& fmt)
	{ sink->locked_backend()->set_formatter(fmt); }

	template<typename F> inline void set_console_format(F const& fmt)
	{ set_format(Log::get().con_sink(), fmt); }
	template<typename F> inline void set_file_format(F const& fmt)
	{ set_format(Log::get().file_sink(), fmt); }

	template<typename SP, typename FLT> inline void set_filter(SP sink, FLT const& flt)
	{ sink->set_filter(flt); }

	template<typename F> inline void set_console_filter(F const& flt)
	{ set_filter(Log::get().con_sink(), flt); }
	template<typename F> inline void set_file_filter(F const& flt)
	{ set_filter(Log::get().file_sink(), flt); }

}	// namespace LOG

#define FDU_LOG(level)  BOOST_LOG_SEV(LOG::get_logger(), LOG::level)
#define FDU_LOGV(level) BOOST_LOG_SEV(LOG::get_logger(), level)

#define ASSERT(asst, msg) \
	if(!(asst)) { \
		FDU_LOG(ERR) << __FILE__ << '(' << __LINE__ << "): " << msg; \
		throw AssertionFail(); \
	}

#define ASSERTS(asst) ASSERT(asst, "Assertion "#asst" failed.")

#ifdef _DEBUG
#	define FDU_DEBUG(msg) FDU_LOG(DEBUG) << msg
#	define ASSERTD  ASSERT
#	define ASSERTSD ASSERTS
#else
#	define FDU_DEBUG(msg)
#	define ASSERTD(a, m)
#	define ASSERTSD(a)
#endif

#endif	// FDU_LOGGING_H_
