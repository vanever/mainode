//
// raw.cpp
// ~~~~~~~~~~~~~~~
//
// Raw protocol for ethernet
//
// Copyright (c) 2012 X.G. Zhou, Fudan University
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/format.hpp>
#include <memory.h>
#include <string.h>
#include "raw.hpp"
#include "log.h"

using namespace std;

namespace {		// local functions
inline int ctoh(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	ASSERT(0, "from_string: format error");
}
inline int htoc(unsigned int h) {
	ASSERTSD(h < 16);
	if (h < 10)
		return h + '0';
	else
		return h - 10 + 'A';
}

}	// namespace

namespace eth {

address::address(const string& str) {
	ASSERT(str.length() == addr_len * 3 - 1, "from_string: length error");
	for (int i = 0; i < addr_len; ++i)
		bytes_[i] = (ctoh(str[3 * i]) << 4) + ctoh(str[3 * i + 1]);
}

address::address(const unsigned char* bytes, unsigned int len) {
	ASSERT(len == addr_len, "from_bytes: length error");
	memcpy(bytes_, bytes, addr_len);
}

string address::to_string() const {
	char str[addr_len * 3];
	int p = -1;
	for (int i = 0; i < addr_len; ++i) {
		str[++p] = htoc(bytes_[i] / 16);
		str[++p] = htoc(bytes_[i] % 16);
		str[++p] = ':';
	}
	str[p] = 0;
	return string(str);
}

unsigned long long address::to_longlong() const {
	unsigned long long val = 0;
	for (int i = 0; i < addr_len; ++i)
		val = (val << 8	) + bytes_[i];
	return val;
}

endpoint::endpoint(int ifindex) : size_(sizeof(sockaddr_ll)) {
	memset(&data_, 0, size_);
	data_.sll_family = AF_PACKET;
	data_.sll_protocol = htons(ETH_P_ALL);
	data_.sll_ifindex = ifindex;
}

void endpoint::set_address(const eth::address& addr) {
	data_.sll_halen = eth::address::addr_len;
	size_ = sizeof(sockaddr_ll) - 8 + eth::address::addr_len;	// 18
	memcpy(data_.sll_addr, &addr.to_bytes(), eth::address::addr_len);
}

device_config::device_config(int request, const char* dev) : req_(request) {
	memset(&ifr_, 0, sizeof(ifr_));
	strncpy(ifr_.ifr_name, dev, sizeof(ifr_.ifr_name) - 1);
	ifr_.ifr_name[sizeof(ifr_.ifr_name)-1] = 0;
}

bool device_config::run(socket& sock) {
	boost::system::error_code ec;
	sock.io_control(*this, ec);
	if (ec) {
		FDU_LOG(WARN) << "get_ifindex ERROR: " << ec.message();
		return false;
	}
	return true;
}

int socket::get_ifindex(const char* dev) {
	device_config cmd(SIOCGIFINDEX, dev);
    return cmd.run(*this) ? cmd.get_ifindex() : -1;
}

int socket::get_flags(const char* dev) {
	device_config cmd(SIOCGIFFLAGS, dev);
    return cmd.run(*this) ? cmd.get_flags() : -1;
}
address socket::get_hwaddr(const char* dev) {
	device_config cmd(SIOCGIFHWADDR, dev);
	cmd.run(*this);
    return cmd.get_hwaddr();
}

std::ostream& operator << (std::ostream& os, const eth::endpoint& ep) {
	static const char* pkttypes[] = { "H", "B", "M", "X", "O", "OB", "OM", "OX" };
	sockaddr_ll& data = *(sockaddr_ll*)ep.data();
	os << boost::format("protocol: %04x  ") % htons(data.sll_protocol);
	os << boost::format("ifindex: %d  ")    % data.sll_ifindex;
	os << boost::format("hatype: %02x  ")   % data.sll_hatype;
	os << boost::format("pkttype: %-2s  ")  % pkttypes[data.sll_pkttype];
	os << "addr: " << ep.address();
	return os;
}

}	// namespace eth
