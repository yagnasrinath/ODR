#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "msgapi.h"
#include "hosts.h"
#include "hw_addrs.h"

int get_vm_ip()
{
	extern const char* vm[10];
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	char *cli_vm_addr;
	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) 
	{
		if ( (sa = hwa->ip_addr) != NULL)
		{
			cli_vm_addr = Sock_ntop_host(sa, sizeof(*sa));
			int j = 0;
			for(j = 0;j<10;j++)
			{
				if(strcmp(cli_vm_addr,vm[j])==0)
				{
					free_hwa_info(hwahead);
					return j;
				}
			}
		}
	}
	free_hwa_info(hwahead);
	return -1;
}

int main(int argc, char *argv[]) 
{
	time_t ltime; /* calendar time */
	extern const char* odr_port;
	int *cli_port;
	extern const int default_port;
	extern const char* default_cli_msg;
	struct sockaddr_un srv_addr,odr_addr;
	char * cli_msg_time = NULL;
	char * cli_addr = NULL;
	int fd,rc;
	int flag = 0;
	int vm_srv_index = -1;
	extern const	char* vm[10];
	extern const	char* vm_port[10];
	if ( (fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) 
	{
		perror("socket error");
		exit(-1);
	}
	memset(&srv_addr, 0, sizeof(srv_addr));
	memset(&odr_addr, 0, sizeof(odr_addr));
	srv_addr.sun_family = AF_UNIX;
	odr_addr.sun_family = AF_UNIX;
	/*strncpy(odr_addr.sun_path, odr_port, sizeof(odr_addr.sun_path)-1);
	if (connect(fd, (struct sockaddr*)&odr_addr, sizeof(odr_addr)) == -1) 
	{
		perror("connect error");
		exit(1);
	}*/
	vm_srv_index = get_vm_ip();
	if(vm_srv_index == -1)
	{
		printf("could not find ip\n");
		exit(1);
	}
	strncpy(srv_addr.sun_path, vm_port[vm_srv_index], sizeof(srv_addr.sun_path)-1);
	unlink(srv_addr.sun_path);
	if(bind(fd,(struct sockaddr*)&srv_addr,sizeof(srv_addr))<0)
	{
		perror("bind error");
		exit(1);
	}
	strncpy(odr_addr.sun_path, odr_port, sizeof(odr_addr.sun_path)-1);
	if (connect(fd, (struct sockaddr*)&odr_addr, sizeof(odr_addr)) == -1) 
	{
		perror("connect error");
		exit(1);
	}
	int i=0;
	while(1)
	{
		msg_recv(fd,&cli_msg_time,&cli_addr,&cli_port);
		//printf("contents read from server file are %d\t%s\t%s\t%d\n",fd,cli_msg_time,cli_addr,*cli_port);
		for(i=0;i<10;i++)
		{
			if(strncmp(cli_addr,vm[i],strlen(vm[i]))==0)
			{
				break;
			}
		}	
		printf("server at node vm%d responding to request from vm%d\n",vm_srv_index+1,i+1);
		msg_send(fd,cli_addr,*cli_port,asctime(localtime(&ltime)),flag);
	}
	return 0;
}
