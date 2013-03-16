#ifndef PACKET_HANDLE_CENTER_HPP
#define PACKET_HANDLE_CENTER_HPP

#include "packet.hpp"

class PacketHandleCenter
{

public:

	PacketHandleCenter();
	void run();

	static PacketHandleCenter & instance()
	{
		static PacketHandleCenter p;
		return p;
	}

private:

	void packet_handle_loop();
    void handle_packet( PacketPtr p, const udp::endpoint & end_point );

	bool quit_;

};

#endif
