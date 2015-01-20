#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include "msgapi.h"
#include "hosts.h"
#include "hw_addrs.h"


int sig_alrm_flag = -1;
static void sig_alrm()
{
	sig_alrm_flag = 0;
}

//returns -1 if vm not found else returns index in vm[]
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
	struct sockaddr_un cli_addr,odr_addr;
	int fd,rc;
	// buffer to hold the temporary file name
	char nameBuff[32];
	// buffer to hold data to be written/read to/from temporary file
	int filedes = -1;
extern const char* odr_port;
extern const int default_port;
extern const char* default_cli_msg;
extern const char* vm[10];
extern const char* vm_port[10];
	char vm_name[5]={'\0','\0','\0','\0','\0'};
	int vm_no = 0;
	int flag=0;
	int vm_cli_index=-1;
	char * srv_msg_time= NULL,*srv_addr=NULL;
	int * srv_port=NULL;
	struct sigaction saction;
	struct itimerval timer ;
	time_t ltime; /* calendar time */
	if ( (fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		perror("socket error");
		exit(-1);
	}
	memset(&cli_addr, 0, sizeof(cli_addr));
	memset(&odr_addr, 0, sizeof(odr_addr));
	cli_addr.sun_family = AF_UNIX;
	odr_addr.sun_family = AF_UNIX;
	// memset the buffers to 0
	memset(nameBuff,0,sizeof(nameBuff));
	// Copy the relevant information in the buffers
	strncpy(nameBuff,"/tmp/gyro-XXXXXX",16);
	errno = 0;
	// Create the temporary file, this function will replace the 'X's
	filedes = mkstemp(nameBuff);
	// Call unlink so that whenever the file is closed or the program exits
	// the temporary file is deleted
	unlink(nameBuff);
	if(filedes<1)
	{
		printf("\n Creation of temp file failed with error [%s]\n",strerror(errno));
		exit(1);
	}
	else
	{
		//printf("\n Temporary file [%s] created\n", nameBuff);
	}
	strncpy(cli_addr.sun_path, nameBuff, sizeof(cli_addr.sun_path)-1);
	bind(fd,(struct sockaddr*)&cli_addr,sizeof(cli_addr));
	memset ( &saction, 0, sizeof ( saction ) ) ;

	saction.sa_handler = &sig_alrm ;
	sigaction ( SIGALRM, &saction, NULL );

	timer.it_value.tv_sec = 50;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0 ;

	//identifying client's ip address
	vm_cli_index = get_vm_ip();
	if(vm_cli_index == -1)
	{
		printf("could not find ip\n");
		exit(1);
	}
	strncpy(odr_addr.sun_path, odr_port, sizeof(odr_addr.sun_path)-1);
	//printf("odr_port in time_client is %s\n",odr_port);
	if (connect(fd, (struct sockaddr*)&odr_addr, sizeof(odr_addr)) == -1) 
	{
		perror("connect error");
		exit(1);
	}
	while(1)
	{
		printf("Enter the vm name to connect in format <vm1>:");
		scanf("%s",vm_name);
		sscanf(vm_name+2,"%d",&vm_no);
		//printf("%s\n",vm[vm_no-1]);
		//printf("Do you want to force the root discovery[0 - No, 1 - Yes]:");
		//scanf("%d",&flag);

		printf("client at node vm%d sending request to server at vm%d\n",vm_cli_index+1,vm_no);
		msg_send(fd,vm[vm_no-1],default_port,default_cli_msg,flag);
		setitimer ( ITIMER_REAL, &timer, NULL ) ;
		msg_recv(fd,&srv_msg_time,&srv_addr,&srv_port);
		if(sig_alrm_flag != 0)
		{
			timer.it_value.tv_sec = 0;
			timer.it_value.tv_usec = 0;
			setitimer ( ITIMER_REAL, &timer, NULL );
			printf("client at node  vm%d received from vm%d\t%s\n",vm_cli_index+1,vm_no,srv_msg_time); 
			//printf("Msg received:%s\t%s\t%d\n",srv_msg_time,srv_addr,*srv_port);
		}
		else
		{	
			timer.it_value.tv_sec = 0;
			timer.it_value.tv_usec = 0;
			setitimer ( ITIMER_REAL, &timer, NULL );
			sig_alrm_flag=0;
			flag = 1;//force discovery
			msg_send(fd,vm[vm_no-1],default_port,default_cli_msg,flag);
			timer.it_value.tv_sec = 50;
			setitimer ( ITIMER_REAL, &timer, NULL ) ;
			printf("client at node vm%d timeout on response from vm%d\n",vm_cli_index+1,vm_no);
			msg_recv(fd,&srv_msg_time,&srv_addr,&srv_port);
			if(sig_alrm_flag != 0)
			{
				printf("client at node  vm%d received from vm%d\t%s\n",vm_cli_index+1,vm_no,srv_msg_time); 
				//printf("Msg received:%s\t%s\t%d\n",srv_msg_time,srv_addr,*srv_port);
			}
			else
			{
				printf("client at node vm%d timeout on response from vm%d in retransmit also\n",vm_cli_index+1,vm_no);
			}
			timer.it_value.tv_sec = 0;
			timer.it_value.tv_usec = 0;
			setitimer ( ITIMER_REAL, &timer, NULL );
		}
	}
	return 0;
}
