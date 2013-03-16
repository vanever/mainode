#ifndef PACKET_HPP
#define PACKET_HPP

#include <boost/asio/buffer.hpp>
#include "raw.hpp"
#include "log.h"

enum {

	ETH_MAX_LEN  = 1514,
	ETH_MIN_LEN  = 60,

	RAW_HEAD_SIZE = 14,
	UDP_HEAD_SIZE = 42,
	CFA_UDP_HEAD_SIZE = UDP_HEAD_SIZE + 4,

	RAW_MAX_LOAD_LEN     = ETH_MAX_LEN - RAW_HEAD_SIZE,		// 1500
	RAW_MIN_LOAD_LEN     = ETH_MIN_LEN - RAW_HEAD_SIZE,		// 46
	UDP_MAX_LOAD_LEN     = ETH_MAX_LEN - UDP_HEAD_SIZE,		// 1472
	UDP_MIN_LOAD_LEN     = ETH_MIN_LEN - UDP_HEAD_SIZE,		// 18
	CFA_UDP_MAX_LOAD_LEN = ETH_MAX_LEN - CFA_UDP_HEAD_SIZE,	// 1468
	CFA_UDP_MIN_LOAD_LEN = ETH_MIN_LEN - CFA_UDP_HEAD_SIZE,	// 14

	RAW_MIN_DATA_LEN     = RAW_MIN_LOAD_LEN - 2,		// 44
	RAW_MAX_DATA_LEN     = RAW_MAX_LOAD_LEN - 2,		// 1498
	UDP_MIN_DATA_LEN     = UDP_MIN_LOAD_LEN - 2,		// 16
	UDP_MAX_DATA_LEN     = UDP_MAX_LOAD_LEN - 2,		// 1470
	CFA_UDP_MIN_DATA_LEN = CFA_UDP_MIN_LOAD_LEN - 2,	// 12
	CFA_UDP_MAX_DATA_LEN = CFA_UDP_MAX_LOAD_LEN - 2,	// 1466

};

template< unsigned HEAD_SIZE, unsigned int N >
class BasicPacket
{

public:

	boost::asio::mutable_buffers_1 to_raw_buffer() { return boost::asio::buffer(bytes_, total_len()); }
	boost::asio::mutable_buffers_1 to_udp_buffer() { return boost::asio::buffer(bytes_ + UDP_HEAD_SIZE, total_len() - UDP_HEAD_SIZE ); }

	u_char          type() const   { return bytes_[HEAD_SIZE + 0]; }
	u_char         index() const   { return bytes_[HEAD_SIZE + 1]; }
	u_char          ctrl() const   { return bytes_[HEAD_SIZE + 2]; }
	void        set_type(u_char c) { bytes_[HEAD_SIZE + 0] = c; }
	void       set_index(u_char c) { bytes_[HEAD_SIZE + 1] = c; }
	void        set_ctrl(u_char c) { bytes_[HEAD_SIZE + 2] = c; }

	void       	set_info(u_char type, u_char idx, int len) { set_type(type); set_index(idx); set_data_len(len); }
	void       	set_info(u_char type, u_char idx) { set_type(type); set_index(idx); }

	void       set_src_mac(const eth::address& addr) { memcpy(bytes_ + 6, addr.to_bytes(), 6); }
	void       set_snk_mac(const eth::address& addr) { memcpy(bytes_    , addr.to_bytes(), 6); }
	eth::address src_addr() const { return eth::address(bytes_ + 6, 6); }
	eth::address snk_addr() const { return eth::address(bytes_    , 6); }

	int      data_len() const { return data_len_; }
	int      load_len() const { return data_len_ + 2; }
	int     total_len() const { return load_len() + HEAD_SIZE; }
	void set_total_len(unsigned l) {
	//	FDU_LOG(INFO) << "set total len: " << l;
		set_load_len(l - HEAD_SIZE);
	}
	void set_load_len(unsigned l)  { set_data_len(l - 2); }
	void set_data_len(unsigned l)
	{
		ASSERTS(l <= N - 2);
		data_len_ = l;
		update_len_bytes();
	}

    u_char * data_ptr()  { return bytes_ + HEAD_SIZE + 2; }
    u_char * load_ptr()  { return bytes_ + HEAD_SIZE;     }	// from type

	virtual void update_len_bytes()
	{ bytes_[12] = load_len() >> 8; bytes_[13] = load_len() & 0xFF; }

protected:

	void check_load_len( unsigned n )
	{
		ASSERTS( n >= ETH_MIN_LEN - HEAD_SIZE && n <= ETH_MAX_LEN - HEAD_SIZE );
	}

	BasicPacket(int data_len = N - 2)
	{ check_load_len( N ); set_data_len(data_len); }

	BasicPacket(int len, u_char type, u_char idx, u_char ctrl)
	{ check_load_len( N ); set_data_len(len); set_type(type); set_index(idx); set_ctrl(ctrl); }

	BasicPacket(int len, u_char type, u_char idx, const eth::address& src, const eth::address& snk, u_char ctrl)
	{
		check_load_len( N ); 
		set_data_len(len); set_type(type); set_index(idx); set_ctrl(ctrl);
		set_snk_mac(snk); set_src_mac(src);
	}

	int data_len_;
	u_char bytes_[HEAD_SIZE + N];

};

typedef BasicPacket<RAW_HEAD_SIZE, RAW_MIN_LOAD_LEN> raw_response_packet_base__;

class RawResponPacket : public raw_response_packet_base__
{

public:

	RawResponPacket()
		: raw_response_packet_base__(RAW_MIN_DATA_LEN) {}

	RawResponPacket(u_char type, u_char idx, u_char ctrl = 0x00)
		: raw_response_packet_base__(RAW_MIN_DATA_LEN, type, idx, ctrl) {}

	RawResponPacket(u_char type, u_char idx, const eth::address& src, const eth::address& snk, u_char ctrl = 0x00)
		: raw_response_packet_base__(RAW_MIN_DATA_LEN, type, idx, src, snk, ctrl) {}

};

typedef BasicPacket<RAW_HEAD_SIZE, RAW_MAX_LOAD_LEN> raw_packet_base__;

class RawPacket : public raw_packet_base__
{
public:
	RawPacket (int len = RAW_MAX_DATA_LEN)
		: raw_packet_base__(len) {}

	RawPacket (int len, u_char type, u_char idx)
		: raw_packet_base__(len, type, idx, 0x00) {}

	RawPacket (int len, u_char type, u_char idx, const eth::address& src, const eth::address& snk)
		: raw_packet_base__(len, type, idx, src, snk, 0x00) {}
};

typedef BasicPacket< CFA_UDP_HEAD_SIZE, CFA_UDP_MIN_LOAD_LEN > cfa_udp_response_packet_base__;
typedef BasicPacket< CFA_UDP_HEAD_SIZE, CFA_UDP_MAX_LOAD_LEN > cfa_udp_packet_base__;

class CfaUdpResponPacket : public cfa_udp_response_packet_base__
{

public:

	CfaUdpResponPacket()
		: cfa_udp_response_packet_base__(CFA_UDP_MIN_DATA_LEN) {}

	CfaUdpResponPacket(u_char type, u_char idx, u_char ctrl = 0x00)
		: cfa_udp_response_packet_base__(CFA_UDP_MIN_DATA_LEN, type, idx, ctrl) {}

	virtual void update_len_bytes()
	{
		// do nothing
	}
};

class CfaUdpPacket;
typedef boost::shared_ptr<CfaUdpPacket> PacketPtr;
typedef CfaUdpResponPacket ResponPacket;
typedef CfaUdpPacket       Packet;

#include <boost/asio.hpp>

using boost::asio::ip::udp;

class CfaUdpPacket : public cfa_udp_packet_base__
{

public:

	CfaUdpPacket (int len = CFA_UDP_MAX_DATA_LEN)
		: cfa_udp_packet_base__(len) {}

	CfaUdpPacket (int len, u_char type, u_char idx)
		: cfa_udp_packet_base__(len, type, idx, 0x00) {}

	virtual void update_len_bytes()
	{
		// do nothing
	}

	void set_from_point(const udp::endpoint & e)
	{
		src_endpoint_ = e;
	}

	const udp::endpoint from_point() { return src_endpoint_; }

	static PacketPtr make_shared_packet(const Packet & pkt)
	{ return PacketPtr(new Packet(pkt)); }

private:

	udp::endpoint src_endpoint_;

};

#endif
