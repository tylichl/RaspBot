/*
 * Copyright (c) 2015 Frantisek Burian <BuFran@seznam.cz>
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
//#include <mcp3422.h>
#include "../RaspBot.h"

/*temporary*/
volatile enum estate {
    S_write = 0,
    S_read = 1,
    S_iddle = 2,
    S_exit = 0xFF,
} state = S_iddle;

volatile int L = 0;
volatile int R = 0;
volatile char actaddr;
struct wiringPiNodeStruct *node ;




PI_THREAD(udp)
{
   struct sockaddr_in srv, cli;


   int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   memset(&srv, 0, sizeof(srv));
   srv.sin_family = AF_INET;
   srv.sin_addr.s_addr = htonl(INADDR_ANY);	// long from host byte order to network byte order
   srv.sin_port = htons(32000);	// unsigned short from host byte order to network byte order
   bind(fd, (struct sockaddr*)&srv, sizeof(srv));	// bind a name to a socket, bind(sockfd, addr, addrlen);

    while (state != S_exit) {
	fd_set rfds;		// declare struct fd_set
	FD_ZERO(&rfds);		// clears the set
	FD_SET(fd,&rfds);	// add file descriptor to set rfds

	struct timeval tv;
	tv.tv_sec = 1;
        tv.tv_usec = 0;

	if (select(fd+1, &rfds, NULL,NULL, & tv) < 0)
	    continue;

	if (FD_ISSET(fd, &rfds)) 		// test if file descriptor is part of the set
	{
	    char buf[1024];
	    socklen_t alen = sizeof(cli);
	    int by = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&cli, &alen);
	
	    if (by == 6)
	    {
		
		actaddr = buf[0] & 0xFE;	// actual address + W/R flag
		state = buf[0] & 0x01;

		L = (short)((buf[3] << 8) | buf[2]);
		R = (short)((buf[5] << 8) | buf[4]);
		
	    printf("\rL=%04i R=%04i     >",L,R);
	    }
	}
    }

    close(fd);
    return 0;
}

int peripheralSetup (int i2cAddress)
{
  int fd ;
  if ((fd = wiringPiI2CSetup (i2cAddress)) < 0)
    return fd ;

  node = wiringPiNewNode (400, 4) ;		// pinBase = 400

  node->fd         = fd;
  node->data0      = 0 ;		// sample rate = 0 
  node->data1      = 0 ;		// gain = 0
  //node->digitalWrite = myDigitalWrite;
  //node->digitalRead = myDigitalRead;	// write to AVR
  return 0 ;
}

int main(void)
{
	char buffer[6];
	int addr;
	wiringPiSetup();
	peripheralSetup(0x70);
	printf("I2C %d\n\n", node->fd);
	buffer[0] = 0x00;
	buffer[1] = 0x0F;
	buffer[2] = 0x01;	// L
	buffer[3] = 0x0F;
	buffer[4] = 0x01;
	write(node->fd, buffer, 5);
/*
	printf("%d\n", wiringPiI2CWrite(node->fd, 0x00));
	printf("%d\n", wiringPiI2CWrite(node->fd, 0x00));
	printf("%d\n", wiringPiI2CWrite(node->fd, 0x00));
	printf("%d\n", wiringPiI2CWrite(node->fd, 0x00));
	printf("%d\n", wiringPiI2CWrite(node->fd, 0x00));
	printf("%d\n", wiringPiI2CWrite(node->fd, 0x00));*/
	//while(1)
	   // wiringPiI2CWrite(node->fd, 0x00);
	//printf("Write address (hexadecimal) of the I2C device: \t0x");
	//scanf("%x", &addr);
	//mcp3422Setup(400, addr, MCP3422_BITS_12, MCP3422_GAIN_1);
  //piThreadCreate(udp);
return 0;
}
