#ifndef FTP_H
#define FTP_H


#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>


#define RRQ		0
#define WRQ		1
#define DATA	2
#define ACK		3
#define ERROR	4
#define CREATE	5
#define READ	6
#define END		7

#define MAX_SIZE	512
#define SERVER_PORT	5000

/*structure for request packet*/
typedef struct _request_pack
{
	short int opcode;
	char fname[20];
	char mode[20];

}request_packet_t;

/*structure for data packet*/
typedef struct _data_pack
{
	short int opcode;
	short int block_num;
	char data[MAX_SIZE];

}data_packet_t;

/*structure for ack packet*/
typedef struct _ack_pack
{
	short int opcode;
	short int block_num;

}ack_packet_t;

/*structure for the error packet*/
typedef struct _error_pack
{
	short int opcode;
	short int error_block_num;
	char error_msg[50];

}error_packet_t;


/*union for the packet*/
typedef union _packet
{
	//request packet
	request_packet_t r_packet;

	//data packet
	data_packet_t d_packet;

	//ack packet
	ack_packet_t a_packet;

	//error packet
	error_packet_t e_packet;

}packet_t;

#endif
