/*
 * Copyright (c) 2016 Ladislav Tylich <tylichl@gmail.com>
 ***********************************************************************
 * This file is part of RaspBot:
 *	https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 *    RaspBot is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    RaspBot is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with RaspBot.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************
 */


#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <mcp3422.h>
#include "../RaspBot.h"

#define LOCK_KEY 0
#define DESTPORT 32000
#define SRCPORT 23
#define MCP3422 0x68
#define FILEDESC_LIMIT 500


extern int myAnalogRead (struct wiringPiNodeStruct *node, int chan);		// mcp3422/24
int errno;

/*temporary*/
volatile enum estate {
	S_iddle = 0x00,
    	S_exit = 0xFF,
} state = S_iddle;
int fdnum = 0;
int L = 0;
int R = 0;

struct sockaddr_in client;



int  parseI2CAddr(char i2cAddress, struct wiringPiNodeStruct* node)
{
	if(i2cAddress > 0x7F) {		// address range overflow
		printf("I2C address is out of range");
		exit(EXIT_FAILURE);
	}
	int fd;
	if((fd = wiringPiI2CSetup (i2cAddress)) < 0) {
		perror("I2C address is unavailable");
		return EXIT_FAILURE;
	}
	fdnum++;
	if(fdnum > FILEDESC_LIMIT) {
		printf("Too many file descriptors in use");
		exit(EXIT_FAILURE);
	}
	node->fd = fd;
	node->data0      = 0 ;		// sample rate = 0 
	node->data1      = 0 ;		// gain = 0
	if(i2cAddress == MCP3422)		// only mcp3422/24
		node->analogRead = myAnalogRead ;


	return EXIT_SUCCESS;
}

int writeEH(int fd, const void *buf, size_t count)	// write with error handling
{
	if(write(fd, buf, count) < 0) {
		perror("Error during write to I2C") ;
		exit(EXIT_FAILURE);
	}
	return 0;
}

int checkI2Cdev(char *addrpool, struct wiringPiNodeStruct* node)
{
	int i = 0; 	// for loop control variable
	int  num = 0;	// number of  attached I2C devices
	printf("Velikosti je/melo by byt %d/%d\n", strlen(addrpool), (0x7F * sizeof(char)));
	if(strlen(addrpool) < (0x7F * sizeof(char))) {
		printf("I2C address pool is too small. Desired is 127 address elements.\n");
		exit(EXIT_FAILURE);
	}
	for(i = 1; i <= 0x7F; i++) {
		if(node->fd) {
			close(node->fd);
			node->fd = 0;
		}
		if(!parseI2CAddr(i, node))
			addrpool[num++] = i;
	}
	return (num > 0) ? (num-1) : 0;
}



int main(void)
{


	wiringPiSetup();

	char i2cpool[127];	// for checkI2Cdev function
	memset(i2cpool, 1, 127*sizeof(char));
	struct wiringPiNodeStruct* node; // = (struct wiringPiNodeStruct*) malloc(sizeof(struct wiringPiNodeStruct));
	node = wiringPiNewNode (400, 4) ;		// pinBase = 400
	state = S_iddle;
	struct sockaddr_in srv, cli;


	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	fdnum++;
	memset(&srv, 0, sizeof(srv));
	srv.sin_family = AF_INET;
	srv.sin_addr.s_addr = htonl(INADDR_ANY);	// long from host byte order to network byte order
	srv.sin_port = htons(SRCPORT);	// unsigned short from host byte order to network byte order
	bind(fd, (struct sockaddr*)&srv, sizeof(srv));	// bind a name to a socket, bind(sockfd, addr, addrlen);
	// client settigs
	client.sin_family = AF_INET;
	client.sin_port = htons(DESTPORT);
	int increment = 0;
	while (state != S_exit) {
		fd_set rfds;		// declare struct fd_set
 		FD_ZERO(&rfds);		// clears the set
		FD_SET(fd,&rfds);	// add file descriptor to set rfds
		if(fdnum > FILEDESC_LIMIT) {
			printf("Too many file descriptors in use!");
			exit(EXIT_FAILURE);
		}
		struct timeval tv;
		tv.tv_sec = 1;
        	tv.tv_usec = 0;
		if (select(fd+1, &rfds, NULL,NULL, & tv) < 0)
	    		continue;
		if (FD_ISSET(fd, &rfds)) { 		// test if file descriptor is part of the set
			char buf[1024];
			char pombuf[1024];
			memset(&buf, 0, 1024);
			socklen_t alen = sizeof(cli);
			int by = 0;
			by = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&cli, &alen);
			if(by < 2) {
				printf("Too short message (unknown protocol)");
				exit(EXIT_FAILURE);
			}

			int i = 0;					// control variable of for loop
			client.sin_addr.s_addr = cli.sin_addr.s_addr;
			switch(buf[0]) {		// parse function byte (protocol)
				case 0:
					printf("Compatibility with previous version\n");
					if(parseI2CAddr(buf[1], node)) {	// parse address with error check
						return EXIT_FAILURE;
					}
					pombuf[0] = 0;			// get bytes to proper order
					memcpy(&pombuf[1], &buf[2], 4);
					writeEH(node->fd, pombuf, 5);
					break;
				case 1:
					printf("Check available devices on I2C bus");
					checkI2Cdev(i2cpool, node);
					break;
				case 2:
					printf("Write to 1\n");
					parseI2CAddr(buf[1], node);	// parse address with error check
					writeEH(node->fd, &buf[2], by-2);
					break;
				case 3:
					printf("Write to multiple\n");
					printf("Received message: %x, %x, %x, %x, %x, %x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
					if((buf[1] < 1) || (buf[1] >= 0x7F)) {
						printf("There is incorrect number of recipients");
						exit(EXIT_FAILURE);
					}
					parseI2CAddr(buf[2], node);
					if(by-2-buf[1] <= 0) {
						printf("There is no message to send");
						exit(EXIT_FAILURE);
					}
					for(i = 0; i < buf[1]; i++) {
						if((node->fd = wiringPiI2CSetup (buf[2+i])) < 0) {
							perror("I2C address is unavailable");
							exit(EXIT_FAILURE);
						}

						writeEH(node->fd, &buf[2+i], (by-2-buf[1]));
					}
					break;
				case 4:
					printf("Read from 1\n");
					break;
				case 5:
					printf("Read from multiple\n");
					break;
				case 6:
					printf("Continuous reading\n");
					break;
				case 7:
					printf("Stop active operation and detroy queue\n");
					break;
				case 8:
					printf("Kill process\n");
					state = S_exit;
					break;
			}
			if(node->fd > 0) {
				close(node->fd);
				node->fd = 0;
			}
		}

	}
	printf("End of program");
	return 0;
}
