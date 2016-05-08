/*broadcast for microtik*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <dirent.h>
#include <netinet/ip.h>
#include <getopt.h>

#define DESTPORT 5678    // the port  for mndp
//#define SRCPORT 5678

char * iface = NULL;


void print_usage()
{
    printf("\n\nUsage:\n\t-a\tset number of broadcast attempts per defined period\n"
	"\t-t\tset multiplier of 10 sec period\n"
	"\t-f\tset interface by which it is recognized by MikroTik\n"
	"\t-i\tset device identity\n"
	"\t-b\tset board details\n"
	"\t-p\tset platform\n"
	"\t-v\tset version\n\n");
}


char * addmsg(char *ptr, int id, int len, const char* msg)
{
    *ptr++ = (id >> 8) & 0xFF;
    *ptr++ = id & 0xFF;
    *ptr++ = (len >> 8) & 0xFF;
    *ptr++ = (len >> 0) & 0xFF;
    while (len > 0)
    {
	*ptr++ = *msg++;
	len--;
    }

    return ptr;
}

int scanIface(char * path)
{
	DIR *dp;
  	struct dirent *ep;
  	dp = opendir (path);
	int ret = 0;

  	if (dp != NULL)
  	{
    		while (ep = readdir (dp))
      			if((ep->d_type == DT_LNK) && (strcmp(ep->d_name, iface) == 0))
				ret = 1;

	    	(void) closedir (dp);
  	}
  	else
    		perror ("Couldn't open iface directory");
	if(ret)
  		return 0;
	else
		return 1;
}

int getmac(char *mac_addr)
{
	if (scanIface("/sys/class/net"))
		return 1;
	char* path = (char*) malloc(24 + strlen(iface));
	memset(path, '\0', 24 + strlen(iface));
	strcat(path, "/sys/class/net/");
	strcat(path, iface);
	strcat(path, "/address");
	FILE * f = fopen (path, "rb");
	int i = 0;
	char dummy;
	if (f) {
		for(i = 0; i < 6; i++) {
			fscanf(f, "%2x", &mac_addr[i]);
			fscanf(f, "%c", &dummy);
  		}
		fclose(f);
		return 0;
	}
	else
		perror("MAC file not found\n");
}

char * addmsgT(char *ptr, int id, const char* msg)
{
    return addmsg(ptr, id, strlen(msg), msg);
}

int main(int argc, char *argv[])
{
	int attempts = 10, mult = 3, opt;
	int charnum = 0;
	char*  identity = NULL;
	char* board = NULL;
	char* version = NULL; 
	char* platform = NULL;
	while ((opt = getopt(argc, argv, "a:t:f:i:b:v:p:")) != -1)
	{
	switch (opt)
	    {
		case 'a':	// attempts
			attempts = atoi(optarg);
			break;
		case 't':	// multiplier of broadcasting
			mult = atoi(optarg);
			break;
		case 'f':
			charnum = strlen(optarg) + 1;
			iface = (char *) malloc(charnum);
			memset(iface, '\0', charnum);
			strcpy((char *) iface, optarg);
			break;
		case 'i':
			charnum = strlen(optarg) + 1;
			identity = (char *) malloc(charnum);
			memset(identity, '\0', charnum);
			strcpy((char *) identity, optarg);
			break;
		case 'b':
			charnum = strlen(optarg) + 1;
			board = (char *) malloc(charnum);
			memset(board, '\0', charnum);
			strcpy((char *) board, optarg);
			break;
		case 'v':
			charnum = strlen(optarg) + 1;
			version = (char *) malloc(charnum);
			memset(version, '\0', charnum);
			strcpy((char *) version, optarg);
			break;
		case 'p':
			charnum = strlen(optarg) + 1;
			platform = (char *) malloc(charnum);
			memset(platform, '\0', charnum);
			strcpy((char *) platform, optarg);
			break;

		default:
			print_usage();
			exit(EXIT_FAILURE);
	    }
	}
	if(iface == NULL) {
		iface = (char *) malloc(6);
		memcpy(iface, "wlan0", 6);
	}
	if(identity == NULL) {
		identity = (char *) malloc(8);
		memcpy(identity, "RaspBot", 8);
	}
	if(board == NULL) {
		board = (char *) malloc(16);
		memcpy(board, "RaspBerry PI V2", 16);
	}
	if(version == NULL) {
		version = (char *) malloc(2);
		memcpy(version, "1", 2);
	}
	if(platform == NULL) {
		platform = (char *) malloc(9);
		memcpy(platform, "MikroTik", 9);
	}

	char buf[64];
	int bcast_fd = socket(AF_INET, SOCK_DGRAM, 0);
	int broadcastEnable=1;
	struct sockaddr_in addr, srcaddr;

	if(bcast_fd < 0)
	        return;


	// set broadcast flag to allow send broadcast
	if(setsockopt(bcast_fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable,
		sizeof(broadcastEnable)))
	{
	perror("setsockopt (SO_BROADCAST)");
	exit(1);
	}


	memset(&srcaddr, 0, sizeof(srcaddr));
	srcaddr.sin_family = AF_INET;
	srcaddr.sin_addr.s_addr = htonl(INADDR_ANY);
//	srcaddr.sin_port = htons(SRCPORT);

	if (bind(bcast_fd, (struct sockaddr *) &srcaddr, sizeof(srcaddr)) < 0) 
	{
		perror("bind");
		exit(1);
	}

	memset(&addr, '\0', sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = (in_port_t) htons(DESTPORT);
	addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	// message
	buf[0] = 0; buf[1] = 0;  // header 
	buf[2] = 0; buf[3] = 0; //seqno

	char *ptr = &buf[4];
	char mac[6];
	memset(mac, '\0', 6);
	getmac(mac);		// get MAC address of defined global iface
	// for debug only
	//printf("MAC addr %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	ptr = addmsg(ptr, 1, 6,(char *) mac);
	ptr = addmsgT(ptr, 5, (char *) identity);	// Identity
	ptr = addmsgT(ptr, 7, (char *) version);	// Version
	ptr = addmsgT(ptr, 8, (char *) platform);	// Platform
	ptr = addmsgT(ptr, 12, (char *) board); 	// Board

	int i = 0;
	while(1) {
		for(i = 0; i < attempts; i++) {

			if(sendto(bcast_fd, buf, ptr - buf, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
       				perror("sendto");
			usleep(1000000);	// wait 1s
		}
		usleep(mult * 10000000);		// wait 10s multiple
	}

	return 0;
} 
