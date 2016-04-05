/*
 * udp_i2c.c:
 *	interface between UDP orders and I2C control/
 *
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
#include <mcp3422.h>
#include "../RaspBot.h"


#ifdef USE_JOY
PI_THREAD(joystick)
{
  int ycal = analogRead(400);
  int xcal = analogRead(403);

  while (state != S_exit) {
    int y = analogRead(400) - ycal;
    int x = analogRead(403) - xcal;

    L = (y - x) / 2;
    R = (y + x) / 2;

    printf("\rL=%04i R=%04i     >",L,R);
  }
  return 0;
}
#endif
 
 
