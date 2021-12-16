#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "mfs.h"	
#include "udp.h"
#include "lfs.h"



void printserverError(int line)
{
    printf("Server Error at line %d \n",((line)));
}

int init(int port, char* image_path)
{
    int res = fsInit(image_path);
    if(res==-1)
    {
        printserverError(__LINE__);
        return res;
    }

  int sd=-1;
  if((sd =   UDP_Open(port))< 0){
    perror("server_init: port open fail");
    return -1;
  }


  struct sockaddr_in s;
  UDP_Packet buf_pk,  rx_pk;

  while (1) {
    //printf("Server Started \n");
    if( UDP_Read(sd, &s, (char *)&buf_pk, sizeof(UDP_Packet)) < 1)
      continue;


    if(buf_pk.request == REQ_LOOKUP){
      rx_pk.inum = fsLookup(buf_pk.inum, buf_pk.name);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_STAT){
      rx_pk.inum = fsStat(buf_pk.inum, &(rx_pk.stat));
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_WRITE){
      rx_pk.inum = fsWrite(buf_pk.inum, buf_pk.buffer, buf_pk.block);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_READ){
      rx_pk.inum = fsRead(buf_pk.inum, rx_pk.buffer, buf_pk.block);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_CREAT){
      rx_pk.inum = fsCreate(buf_pk.inum, buf_pk.type, buf_pk.name);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_UNLINK){
      rx_pk.inum = fsUnlink(buf_pk.inum, buf_pk.name);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_SHUTDOWN) {
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));
      fsShutDown();
    }
    else if(buf_pk.request == REQ_RESPONSE) {
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));
    }
    else {
      perror("server_init: unknown request");
      return -1;
    }


  }

  return 0;
}

int main(int argc, char *argv[])
{
	if(argc != 3) {
		perror("Usage: server <portnum> <image>\n");
		exit(1);
	}

	init(atoi(argv[1]),argv[2] );

	return 0;
}
