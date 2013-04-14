#ifndef APP_MANAGER_HPP
#define APP_MANAGER_HPP

#include <fstream>
#include "matcher_manager.hpp"
#include "match_utils.h"
#include <boost/noncopyable.hpp>
#include "comm_window.hpp"

class BitFeatureLoader;
class BitFeatureSender;

struct ResultPacket {
	static const int max_len = 1380;

	U32 src_id;
	U32 snk_id;
	U32 msg_id;
	U16 msg_len;
	U16 report_id;
	AppId app_id;
	U64 timestamp;
	char result[max_len + 2];
	int lines;

	ResultPacket(DomainInfoPtr);
	void put_line(const string& line);
	void clear() { result[0] = lines = 0; }
	void send();

}__attribute__((packed));

/// use to store domain specific data
class AppDomain : public boost::noncopyable
{

public:

	~AppDomain();

	const AppId appid() const { return appid_; }
	BitFeatureWindow * window() { return &window_; }

	void output_result(std::ostream & out);
	void output_result(unsigned testid);

	void add_testid(unsigned testid) { testid_set_.insert(testid); }
	void add_section(const MatchSection & sec) { sec_man_.add_section(sec); }

	DomainInfoPtr domain_info_ptr() { return domaininfo_; }

	const std::string match_source() const { return match_source_; }
	unsigned source_repeat_times() const { return source_repeat_times_; }
	const std::string lib_file() const { return lib_file_; }

	BitFeatureLoader * bitfeature_loader() { return loader_; }
	BitFeatureSender * bitfeature_sender() { return sender_; }

	void increase_sent_frames(unsigned v) { sent_frames_ += v; }
	unsigned sent_frames() const { return sent_frames_; }
	unsigned unhandled_frames() const;
	unsigned speed() const;

private:

	AppDomain(DomainInfoPtr);

	friend class AppManager;

	void set_loader(BitFeatureLoader * p) { loader_ = p; }
	void set_sender(BitFeatureSender * p) { sender_ = p; }

	void trigger_run();

	AppId appid_;
	BitFeatureLoader * loader_;
	BitFeatureSender * sender_;
	BitFeatureWindow window_;
	std::string match_source_;
	SectionManagerSimp sec_man_;
	std::set<unsigned> testid_set_;
	DomainInfoPtr domaininfo_;
	std::string lib_file_;
	std::ofstream out_file_;
	unsigned source_repeat_times_;

	unsigned sent_frames_;
	
	ResultPacket result_pkt_;
};


typedef boost::shared_ptr<AppDomain> AppDomainPtr;

class AppManager : boost::noncopyable
{

public:

	typedef std::map<AppId, AppDomainPtr> AppDomainMap;

	bool create_app(DomainInfoPtr pd);

	static AppManager & instance() {
		static AppManager a;
		return a;
	}

	AppDomainPtr get_domain_by_appid(AppId appid) {
		auto it = domains_.find(appid);
		if (it != domains_.end()) {
			return it->second;
		}
		return AppDomainPtr();
	}

	AppDomainMap & domains() { return domains_; }

	void report_all(std::ostream & out) {
		foreach (auto & app, domains_) {
			app.second->output_result(out);
		}
	}

private:

	AppManager() {}

	AppDomainMap domains_;

};

#endif
