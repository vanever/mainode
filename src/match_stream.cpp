#include "match_stream.hpp"
#include "packet.hpp"
#include "utils.h"
#include "match_utils.h"
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <cstdlib>
#include <ctime>

#include "server.hpp"
#include "bits.hpp"

#include "comm_box.hpp"
#include "comm_window.hpp"

#include "app_manager.hpp"

using namespace std;
using boost::asio::ip::udp;

//----------------------------------------------------------------------------------

#define check_and_put(count, max_data_len, window, pkt, TYPE, INDEX, LEN, pdata) \
	do {											\
		if (count == max_data_len / sizeof(int))	\
		{											\
			window->put();                       	\
			pkt = window->acquire();             	\
			pkt->set_data_len(LEN);     			\
			pkt->set_index(INDEX++);                \
			pkt->set_type(TYPE);             		\
			pdata = (int *)pkt->data_ptr();         \
			count = 0;                              \
		}											\
	} while (0)

NewLibLoader::NewLibLoader( const VideoLibVec & lib )
	: lib_(lib)
{
}

void NewLibLoader::do_run_task()
{
	FDU_LOG(INFO) << "-- NewLibLoader start";
	load_lib( lib_ );
}

void NewLibLoader::do_end_task()
{
	Server::instance().surf_lib_window()->notify_all_put();
	FDU_LOG(INFO) \
		<< "-------------------------------------------------\n"
		<< "-- NewLibLoader quit\n"
		<< "-------------------------------------------------";
}

void NewLibLoader::load_lib( const VideoLibVec & lib )
{
	int word;
	int count = 0;
	u_char index = 0;

	const int max_data_len = 1440;
	const int max_num_word = max_data_len / sizeof(word);	// 360 words
	const int align_words = 256 / 8 / sizeof(word);	// 256 alignment, 8 bytes

	SurfLibWindow * const window = Server::instance().surf_lib_window();

	window->reset();	// important !!!
	Packet * pkt = window->acquire();
	pkt->set_info(LIB_START, index++, max_data_len);
	int * pdata = (int *)pkt->data_ptr();   

	int rec_num = 0;
	foreach (const VideoLibRec & rec, lib)
	{
		int left_words = max_num_word - count;
		ASSERTS( count % align_words == 0 && left_words >= align_words );

		//! fill SYNCWORD & video index & num_frame
		memset(pdata + count, 0xFF, 6 * sizeof(word));
		count += 6;
		pdata[count++] = SYNCWORD;
		pdata[count++] = htons(rec.video_num) + ( htons(rec.point_vec.size()) << 16 );
		check_and_put(count, max_data_len, window, pkt, LIB_SENDING, index, max_data_len, pdata);

		//! fill bit_feature
		foreach (const BitFeature & b, rec.point_vec)
		{
			ASSERTS(max_num_word - count >= b.nwords() && b.nwords() <= align_words);	// sure
			for (int i = 0; i < b.nwords(); i++)
				pdata[count++] = b.words[i];
			check_and_put(count, max_data_len, window, pkt, LIB_SENDING, index, max_data_len, pdata);
		}

		if (count % align_words)
		{	// need alignment after one video
			int fill_num = align_words - count % align_words;
			memset(pdata + count, 0xFF, fill_num * sizeof(word));
			count += fill_num;
		}
		check_and_put(count, max_data_len, window, pkt, LIB_SENDING, index, max_data_len, pdata);
	}

	ASSERTS(max_num_word - count >= align_words && (count % align_words == 0));
	memset(pdata + count, 0xFF, 6 * sizeof(word));
	count += 6;
	pdata[count++] = SYNCWORD_END;                    // add a sync end word
	pdata[count++] = 0xFFFFFFFF;
	if (count == max_num_word)
	{
		check_and_put(count, max_data_len, window, pkt, LIB_SENDING, index, max_data_len, pdata);
		memset(pdata + count, 0xFF, 32);       		// add additional empty line
		count += 32 / sizeof(int);
		pkt->set_type(LIB_END);
		pkt->set_data_len(count * sizeof(word));
		window->put();
	}
	else
	{
		ASSERTS(max_num_word - count >= align_words);
		memset(pdata + count, 0xFF, 32);       		// add additional empty line
		count += 32 / sizeof(int);
		pkt->set_type(LIB_END);
		pkt->set_data_len(count * sizeof(word));
		window->put();
	}
}

NewLibSender::NewLibSender( const endpoint & dest )
	: dest_(dest)
{
}

void NewLibSender::do_end_task()
{
	FDU_LOG(INFO) \
		<< "--------------------------------------\n"
		<< "-- NewLibSender quit\n"
		<< "--------------------------------------\n";
}

void NewLibSender::do_run_task()
{
	FDU_LOG(INFO) << "-- NewLibSender start";
	send_packets();
}

void NewLibSender::send_packets()
{
	SurfLibWindow * const window = Server::instance().surf_lib_window();
	Packet * pkt;
	static size_t cnt = 0;
	while (pkt = window->get())
	{ 
		FDU_LOG(DEBUG) << "lib sender -> send packet: " << cnt << "th";
		Server::instance().send( pkt->to_udp_buffer(), dest_ );
		if (!(++cnt % 10000))
		{
			FDU_LOG(INFO) << "send packets: " << cnt;
		}
	}
}

//----------------------------------------------------------------------------------
// connection builder

ConnectionBuilder::ConnectionBuilder ( MatcherPtr m )
	: dest_( m->to_endpoint() )
	, num_timeout_( 0 )
	, built_( false )
	, pkt_( CFA_UDP_MIN_DATA_LEN, CONN_BUILD, 0 )	// type: 0x71 index: 1
//	, pkt_( CFA_UDP_MIN_DATA_LEN, CONN_BUILD + 1, 1 )	// type: 0x71 index: 1
{
	unsigned short * p = (unsigned short *)pkt_.data_ptr();
	*p = htons( CommArg::comm_arg().server_port );
	u_char * pc = (u_char *)(p + 1);
	*pc++ = m->nbits() >> 5;
	*pc++ = m->merge_unit() >> 3;
	*pc++ = m->match_threshold();
	*pc++ = m->merge_threshold();
	conn_task_.insert( std::make_pair(dest_, this) );
}

ConnectionBuilder::~ConnectionBuilder()
{
	std::map<endpoint, ConnectionBuilder *>::iterator it = conn_task_.find(dest_);
	if (it != conn_task_.end())
	{
		conn_task_.erase(it);
	}
}

std::map<endpoint, ConnectionBuilder *> ConnectionBuilder::conn_task_;

void ConnectionBuilder::do_run_task()
{
	FDU_LOG(INFO) << "start connect " << dest_;
	do_build_connection(dest_);
}

void ConnectionBuilder::do_end_task()
{
	FDU_LOG(INFO)
		<< "\n---------------------------------------------------------------------------"
		<< "\n-- ConnectionBuilder quit, connect " << dest_ << " " << (built_ ? "success" : "failed")
		<< "\n---------------------------------------------------------------------------";
}

void ConnectionBuilder::do_build_connection( const endpoint & dest )
{
	lock lk( monitor_ );
	Server::instance().send( pkt_.to_udp_buffer(), dest );
	while ( !built_ )
	{
		if (!c_wait_.timed_wait( lk, boost::posix_time::milliseconds(CommArg::comm_arg().conn_build_wait_time) ))
		{	// time out
			if (++num_timeout_ == CommArg::comm_arg().max_conn_times_allowed)
			{
				FDU_LOG(WARN)
					<< "\n-----------------------------------------------------"
					<< "\n-- connect " << dest << " failed for " << num_timeout_ << " times"
					<< "\n-- skip this client"
					<< "\n-----------------------------------------------------";
				break;	// fail build connection
			}
			else
			{
				FDU_LOG(WARN) << "connect " << dest << " timeout " << num_timeout_ << " times";
				FDU_LOG(WARN) << "re-connect again";
			}

			// send again
			Server::instance().send( pkt_.to_udp_buffer(), dest );
		}
	}

	if (built_)
		MatcherManager::instance().connect_matcher( dest );
	else
		MatcherManager::instance().set_matcher_state( dest, Matcher::FAIL_CONNECT );
}

void ConnectionBuilder::set_built(bool b)
{
	lock lk(monitor_);
	built_ = b;
	if (b)
	{
		c_wait_.notify_one();
	}
}

//--------------------------------------------------------------------------------------- 
//

BitFeatureLoader::BitFeatureLoader (AppDomainPtr app)
	: image_cnt_(0)
	, app_(app)
{
}

void BitFeatureLoader::do_run_task()
{
	FDU_LOG(INFO) << "-- BitFeatureLoader start";
	DomainInfoPtr pd = app_->domain_info_ptr();
	loadLibNew(pd->NBits << 5, vec_, app_->match_source());
	load_loop(vec_);
}

void BitFeatureLoader::do_end_task()
{
	app_->window()->notify_all_put();
	FDU_LOG(INFO) << "\n---------------------------------------\n"
	              << "-- BitFeatureLoader quit\n"
	              << "---------------------------------------";
}

void BitFeatureLoader::load_loop(const VideoLibVec & videos)
{
	BitFeatureWindow * const window 
		= app_->window();

	AppId appid = app_->appid();

	foreach (const VideoLibRec & rec, videos)
	{
//		int framegroup_size = CommArg::comm_arg().framegroup_size;
		int framegroup_size = app_->domain_info_ptr()->SliceLen * 60;
		unsigned num_of_frames = rec.num_of_frames;
		unsigned short vid = rec.video_num;
		unsigned short pos = 0;
		unsigned short num_slice = 0;
		while (num_of_frames > framegroup_size)
		{
			MatcherPtr node = MatcherManager::instance().choose_next_matcher_for_domain(appid);
			MatcherManager::instance().start_timer();
			load_section(vid, pos, &rec.point_vec[pos], framegroup_size, node, num_slice);
			unsigned testid = (vid << 10) + num_slice;
			app_->add_testid(testid);
			num_slice++;
			window->wait_until_box_empty();		// must
			node->increase_load( framegroup_size );
			MatcherManager::instance().update_all_matcher_load();	// will stop timer

			num_of_frames -= framegroup_size;
			pos           += framegroup_size;
		}
//		if (num_of_frames)
//		{
//			MatcherPtr node = MatcherManager::instance().choose_next_matcher_for_domain(appid);
//			MatcherManager::instance().start_timer();
//			load_section(vid, pos, &rec.point_vec[pos], num_of_frames, node, num_slice++);
//			window->wait_until_box_empty();		// must
//			node->increase_load( num_of_frames );
//			MatcherManager::instance().update_all_matcher_load();	// will stop timer
//		}
	}
}

void BitFeatureLoader::load_section (unsigned short vid, unsigned short frompos, const BitFeature * bit_vec, unsigned size, MatcherPtr matcher, unsigned slice_idx)
{
	ASSERT( matcher->ready_receive(), "matcher: " << matcher << " not ready" );

	const unsigned max_data_len   = 1440;
	const unsigned merge_unit     = matcher->merge_unit();
//	const unsigned sent_size      = size & ~(merge_unit - 1);	// make size 8 devided
	const unsigned sent_size      = size;
	ASSERTS(merge_unit == 8 || merge_unit == 16 || merge_unit == 32);
	if (sent_size == 0)
		return;

	BitFeatureWindow * const window 
		= app_->window();

	app_->bitfeature_sender()->set_dest_addr( matcher->to_endpoint() );

	if (sent_size != size)
		FDU_LOG(DEBUG) << "cut bitfeature vector size from " << size << " to " << sent_size;

	static u_char idx = 0;
	unsigned cnt = 0;
	unsigned slicepos = 0;
	Packet * pkt = window->acquire();
	pkt->set_info(
			matcher->first_flag() ? (BITFEATURE_TYPE | 0x10) : BITFEATURE_TYPE,
			idx++,
			max_data_len
			);
	matcher->demark_first_flag();
	unsigned * p = (unsigned *)pkt->data_ptr();

	for (unsigned i = 0; i < sent_size; i += merge_unit)
	{
		unsigned num_left = sent_size - i;
		if (num_left < merge_unit)
		{	// handle last merge unit
			switch (merge_unit) {
			case 8:
				if (num_left > 4)
					i = sent_size - merge_unit;	// align
				break;
			case 16:
				if (num_left > 8)
					i = sent_size - merge_unit;	// align
				break;
			case 32:
				if (num_left > 10)
					i = sent_size - merge_unit;	// align
				break;
			default:
				break;
			}
		}

		if (i + merge_unit > sent_size)
			break;	// check for skip condition

		ASSERTS(max_data_len / sizeof(unsigned) - cnt >= 32);
		memset(p + cnt, 0xFF, 20);
		cnt += 20 / sizeof(unsigned);
		p[cnt++] = SYNCWORD;
		p[cnt++] = (vid << 22) + (slice_idx << 12) + i;
		p[cnt]   = 0; cast<u_char>(p + cnt) = merge_unit; cnt++;	// add merge unit word

		if (cnt == max_data_len / sizeof(unsigned))
		{											
			window->put();                       	
			pkt = window->acquire();             	
			pkt->set_info(BITFEATURE_TYPE, idx++, max_data_len);
			p = (unsigned *)pkt->data_ptr();         
			cnt = 0;                              
		}											

		for (unsigned j = 0; j < merge_unit; j++)
		{
			unsigned vec_idx = i + j;
			const BitFeature & bit = bit_vec[vec_idx];
			for (unsigned k = 0; k < bit.nwords(); k++)
				p[cnt++] = bit.words[k];
			if (cnt == max_data_len / sizeof(unsigned))	
			{
				window->put();                       	
				pkt = window->acquire();             	
				pkt->set_info(BITFEATURE_TYPE, idx++, max_data_len);
				p = (unsigned *)pkt->data_ptr();         
				cnt = 0;                              
			}											
		}

	}

	if (cnt > 0)
	{	// last packet
		pkt->set_data_len( cnt * sizeof(unsigned) );
		window->put();
	}

	if (!CommArg::comm_arg().use_packet_pause)
	{	// use group pause
		usleep( 1000 * CommArg::comm_arg().group_pause );
	}
}

BitFeatureSender::BitFeatureSender(AppDomainPtr app)
	: app_(app)
{
}

void BitFeatureSender::do_run_task()
{
	FDU_LOG(INFO) << "BitFeatureSender start";
	send_packets_loop(  );
}

void BitFeatureSender::do_end_task()
{
	Server::instance().mark_bitfeatures_all_sent();
	FDU_LOG(INFO) << "\n------------------------------------\n"
	              << "-- BitFeatureSender quit\n"
                  << "------------------------------------";
}

void BitFeatureSender::send_packets_loop()
{
	static std::size_t cnt = 0;
	bool pause = (bool)CommArg::comm_arg().use_packet_pause;
	BitFeatureWindow * window = app_->window();
	while ( Packet * pkt = window->get() )
	{
		FDU_LOG(DEBUG) << "BitFeatureSender -> send packet: " << ++cnt;
		Server::instance().send( pkt->to_udp_buffer(), dest_addr() );
		if (pause)
		{	// control get packet speed
			usleep( 1000 * paused_time_ );
		}
	}
}

const endpoint BitFeatureSender::dest_addr() const 
{
	return dest_addr_;
}

void BitFeatureSender::set_dest_addr( const endpoint & addr )
{
	ASSERT( Server::instance().bitfeature_window()->box_empty(), "setting address at wrong time: box not empty" );
	dest_addr_ = addr;
}

void BitFeatureSender::update_pause_time(unsigned v)
{
	paused_time_ = v;
	FDU_LOG(INFO) << app_ << ": change bitfeature sending interval to " << v;
}
