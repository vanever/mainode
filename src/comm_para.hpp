#ifndef COMM_PARA_HPP
#define COMM_PARA_HPP

typedef unsigned char      u_char;
typedef unsigned char      U08;
typedef unsigned short     U16;
typedef unsigned int       U32;
typedef unsigned long long U64;

namespace {
	const u_char      CONN_BUILD          = 0x70;
	const u_char      BITFEATURE_TYPE     = 0x04;

	const u_char      WANNA_QUIT          = 0x40;
	const u_char      QUITED              = 0x50;

	const u_char      LIB_START           = 0x12;
	const u_char      LIB_SENDING         = 0x02;
	const u_char      LIB_END             = 0x22;

	const u_char      IMG_START           = 0x16;
	const u_char      IMG_SENDING         = 0x06;
	const u_char      IMG_END             = 0x26;
	const u_char      IMG_ALL_END         = 0x66;

	const u_char      POINT_START         = 0x14;
	const u_char      POINT_SENDING       = 0x04;
	const u_char      POINT_END           = 0x24;
	const u_char      POINT_ALL_END       = 0x64;

//  const u_char      EXTRACT_START       = 0x0C;
	const u_char      EXTRACT_RECVING     = 0x0C;
	const u_char      EXTRACT_END         = 0x2C;

//  const u_char      EXTRACT_START       = 0x0C;
	const u_char      MATCH_RECVING       = 0x08;
	const u_char      MATCH_END           = 0x28;

	const u_char      CTRL_PAUSE          = 0xFF;
	const u_char      CTRL_RECOVER        = 0x00;

	const int         SYNCWORD            = 0xFADCCAFD;
	const int         SYNCWORD_END        = 0xAFCDACDF;

	const int         SEND_LIB_PKT_SIZE   = 1472;

	const int         MATCH_WIDTH         = 128;

//	const int         CLIENT_NUM          = 1;
}

#endif
