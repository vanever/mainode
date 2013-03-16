#ifndef DOMAIN__h
#define DOMAIN__h

//#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <string.h>
//#include <sys/types.h>
//#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SERVER_SOCK_FILE "/tmp/DCSP_FIFO_A_SOCK"
#define CLIENT_SOCK_FILE "/tmp/DCSP_FIFO_B_SOCK"

extern "C" {

extern int sock_fd;
extern int serv_addr_len;

extern struct sockaddr_un serv_addr;
extern struct sockaddr_un clnt_addr;

void printbuf_hex(char* buf, int len);

int dcsp_fifo_init(int type);
void dcsp_fifo_close(void);
int dcsp_fifo_send(char *snd_buf, int len);
int dcsp_fifo_read(char *recv_buf, int len);

};

#endif

