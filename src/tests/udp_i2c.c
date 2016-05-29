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

extern int myAnalogRead (struct wiringPiNodeStruct *node, int chan);		// mcp3422/24


/*temporary*/
volatile enum estate {
	S_i2c_write = 0x08,
	S_udp_write = 0x10,
	S_iddle = 0x17,
    	S_exit = 0xFF,
} state = S_iddle;

volatile int L = 0;
volatile int R = 0;

int addrOK = 0;
char i2cAddr = 0x00;		// General call default
char km2buf[5];
int received = 0;


struct wiringPiNodeStruct *node ;
struct sockaddr_in client;




PI_THREAD(udp)
{
	struct sockaddr_in srv, cli;
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	memset(&srv, 0, sizeof(srv));
	srv.sin_family = AF_INET;
	srv.sin_addr.s_addr = htonl(INADDR_ANY);	// long from host byte order to network byte order
	srv.sin_port = htons(SRCPORT);	// unsigned short from host byte order to network byte order
	bind(fd, (struct sockaddr*)&srv, sizeof(srv));	// bind a name to a socket, bind(sockfd, addr, addrlen);
	// client settigs
	client.sin_family = AF_INET;
	client.sin_port = htons(DESTPORT);
	while (state != S_exit) {
		fd_set rfds;		// declare struct fd_set
 		FD_ZERO(&rfds);		// clears the set
		FD_SET(fd,&rfds);	// add file descriptor to set rfds
		int addok;

		struct timeval tv;
		tv.tv_sec = 1;
        	tv.tv_usec = 0;
		if (select(fd+1, &rfds, NULL,NULL, & tv) < 0)
	    		continue;
		if (FD_ISSET(fd, &rfds)) { 		// test if file descriptor is part of the set

			char buf[1024];
			socklen_t alen = sizeof(cli);
			int by = 0;
			piLock(LOCK_KEY);
			by = recvfrom(fd, km2buf, sizeof(km2buf), 0, (struct sockaddr*)&cli, &alen);
			client.sin_addr.s_addr = cli.sin_addr.s_addr;
			piUnlock(LOCK_KEY);
	    		if (addrOK) {
				switch(by)
				{
				case 1:
                                        //piLock(LOCK_KEY);
                                        addok = peripheralSetup(km2buf[0]);
                                        //piUnlock(LOCK_KEY);
                                        i2cAddr = km2buf[0];
                                        if(addok < 0) {
                                                addrOK = 0;
                                                perror("Can't send through i2c");
                                        }
                                        else {
                                                addrOK = 1;
					}
					break;
				case 4:
					received = 4;
					state = S_i2c_write;
					break;
				case 5:
					L = (short)((km2buf[2] << 8) | km2buf[1]);
					R = (short)((km2buf[4] << 8) | km2buf[3]);
					printf("\rL=%04i R=%04i >\n",L,R);
					received = 5;
					state = S_i2c_write;
					break;
				}
				//state = S_i2c_write;
				/*if(i2cAddr == MCP3422) {
                                        piLock(LOCK_KEY);
					km2buf[0] = 0;
					km2buf[1] = (char)(L & 0x00FF);
					km2buf[2] = (char)((L >> 8) & 0x00FF);
					km2buf[3] = (char)(R & 0x00FF);
					km2buf[4] = (char)((R >> 8) & 0x00FF);
					piUnlock(LOCK_KEY);
					int attempt = 10;
					while(attempt--)
						if(sendto(fd, km2buf,  5, 0, (struct sockaddr *)&client, sizeof(struct sockaddr_in)) < 0)
       							perror("sendto");
						else
							printf("korektne odeslano");
				}*/
			}
			else {
				state = S_iddle;
				if(by == 1) {		// address was sent
					int addok;
					piLock(LOCK_KEY);
					addok = peripheralSetup(km2buf[0]);
					piUnlock(LOCK_KEY);
					i2cAddr = km2buf[0];
					if(addok < 0) {
						addrOK = 0;
						perror("Can't send through i2c");
					}
					else {
						addrOK = 1;
					}
				}
			}
		}

	if(i2cAddr == MCP3422)		// only mcp3422/24
	{
			state = S_iddle;
			char buf[4];
			buf[0] = (char)(L & 0x00FF);
			buf[1] = (char)((L & 0xFF00)  >> 8);
			buf[2] = (char)(R & 0x00FF);
			buf[3] = (char)((R & 0xFF00)  >> 8);
			int bufsize = 4;
	                if(sendto(fd, buf, bufsize, 0, (struct sockaddr *)&client, sizeof(struct sockaddr_in)) < 0)
                        	perror("sendto");
			}
	}

	close(fd);
	return 0;
}


int peripheralSetup (int i2cAddress)			//, struct wiringPiNodeStruct *node)
{
	if(node->fd > 0) {
		close(node->fd);
		node->fd = 0;
	}
	int fd ;
	if ((fd = wiringPiI2CSetup (i2cAddress)) < 0)
		return fd ;

	node->fd         = fd;
	node->data0      = 0 ;		// sample rate = 0 
	node->data1      = 0 ;		// gain = 0
	if(i2cAddress == MCP3422)		// only mcp3422/24
		node->analogRead = myAnalogRead ;
	return 0 ;
}

int stop(void)
{
	char buf[5];
	buf[0] = 0x00; 
	buf[1] = 0x00; buf[2] = 0x00;
	buf[3] = 0x00; buf[4] = 0x00;
	write(node->fd, buf, 5);
	return 0;
}

PI_THREAD(i2c)
{
	while(state != S_exit) {
		if( state == S_i2c_write) {
			piLock(LOCK_KEY);
			write(node->fd, km2buf, received) ;
			piUnlock(LOCK_KEY);
			state = S_iddle;
		}
		if(i2cAddr == MCP3422) {
			int y = analogRead(400);
    			int x = analogRead(403);
			L = (y - x) / 2;
    			R = (y + x) / 2;
			printf("\rL=%04i R=%04i     >",L,R);
			state = S_udp_write;
		}
	}
}

int main(void)
{
	wiringPiSetup();
	memset(&km2buf, 0, 5);
	node = wiringPiNewNode (400, 4) ;		// pinBase = 400
	state = S_iddle;
	piThreadCreate(udp);
	piThreadCreate(i2c);
	do{
		usleep(5000000);	// 5s
	}
	while (state != S_exit);
	return 0;
}
