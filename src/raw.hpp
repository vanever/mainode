//
// raw.hpp
// ~~~~~~~~~~~~~~~
//
// Raw protocol for ethernet
//
// Copyright (c) 2012 X.G. Zhou, Fudan University
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef RAW_HPP
#define RAW_HPP

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <netpacket/packet.h>
#include <net/ethernet.h>

namespace eth {

class endpoint;

struct raw {			// ethernet protocol
	int family()   const { return AF_PACKET; }			// 0x0011
	int type()     const { return SOCK_RAW; }
	int protocol() const { return htons(ETH_P_ALL); }	// 0x0300

	typedef eth::endpoint endpoint;
};

class address {			// ethernet address
public:
	static const int addr_len = 6;
	typedef unsigned char bytes_type[addr_len];

	address(const std::string& str = "00 00 00 00 00 00");	// construct from string
	address(const unsigned char* bytes, unsigned int len);	// construct from bytes

	unsigned long long to_longlong() const;
	std::string to_string() const;
	const bytes_type& to_bytes() const { return bytes_; }

	static address broadcast() { return address("ff ff ff ff ff ff"); }
	bool is_multicast() const { return bytes_[0] == 1; }
	
private:
	bytes_type bytes_;
};

class endpoint {		// ethernet endpoint
public:
	endpoint(int ifindex = 0);

	typedef raw protocol_type;
	protocol_type protocol() const { return raw(); }

	typedef boost::asio::detail::socket_addr_type data_type;
	data_type* data() { return (data_type*)&data_; }
	const data_type* data() const { return (data_type*)&data_; }

	std::size_t size() const { return size_; }
	void resize(std::size_t s) { size_ = s; }
	std::size_t capacity() const { return sizeof(sockaddr_ll); }
	
	int ifindex() const { return data_.sll_ifindex; }
	void set_ifindex(int index) { data_.sll_ifindex = index; }
	
	unsigned int header_type() const { return data_.sll_hatype; }
	unsigned int packet_type() const { return data_.sll_pkttype; }
	
	eth::address address() const { return eth::address(data_.sll_addr, data_.sll_halen); }
	void set_address(const eth::address& addr);

private:
	std::size_t size_;
	sockaddr_ll data_;
};

struct socket : boost::asio::basic_raw_socket<raw> {	// ethernet socket
	explicit socket(boost::asio::io_service& io_service)
    	:  boost::asio::basic_raw_socket<raw>(io_service), ifindex(0)
	{}
	socket(boost::asio::io_service& io_service, const raw& protocol)
    	:  boost::asio::basic_raw_socket<raw>(io_service, protocol), ifindex(0)
	{}
	socket(boost::asio::io_service& io_service, const endpoint& ep)
    	:  boost::asio::basic_raw_socket<raw>(io_service, ep), ifindex(0)
	{}
	socket(boost::asio::io_service& io_service, const raw& protocol,
		   const boost::asio::raw_socket_service<raw>::native_handle_type& native_socket)
    	:  boost::asio::basic_raw_socket<raw>(io_service, protocol, native_socket), ifindex(0)
	{}

	socket(boost::asio::io_service& io_service, const char* dev)
   		:  boost::asio::basic_raw_socket<raw>(io_service, raw()), ifindex(0)
	{ bind_device(dev); }
		
  	int get_ifindex(const char* dev);
	int get_flags(const char* dev);
	address get_hwaddr(const char* dev);

	void bind_device(const char* dev) {
		ifindex = get_ifindex(dev);
		if (ifindex > 0)
			bind(endpoint(ifindex));
	}
	
	int ifindex;
};

// IoControlCommand for configuring network devices, see netdevice(7)
// Currently only a few get request supported
class device_config {
	int req_;
	ifreq ifr_;
public:
	device_config(int request, const char* dev);
	
	int name() const { return req_; }
	void* data() { return (void*)&ifr_; }
	
	bool run(socket& sock);
	
	int get_ifindex() const { return ifr_.ifr_ifindex; }	// SIOCGIFINDEX
	int get_flags()   const { return ifr_.ifr_flags; }		// SIOCGIFFLAGS
	address get_hwaddr() const								// SIOCGIFHWADDR
	{ return address((const unsigned char*)ifr_.ifr_hwaddr.sa_data, address::addr_len); }
};

	inline bool operator == (const eth::address& x, const eth::address& y) { return x.to_longlong() == y.to_longlong(); }
	inline bool operator != (const eth::address& x, const eth::address& y) { return x.to_longlong() != y.to_longlong(); }
	inline bool operator <  (const eth::address& x, const eth::address& y) { return x.to_longlong() <  y.to_longlong(); }

	inline std::ostream& operator << (std::ostream& os, const eth::address& addr)
	{ return os << addr.to_string(); }

	std::ostream& operator << (std::ostream& os, const eth::endpoint& addr);

}	// namespace eth

#endif // RAW_HPP
