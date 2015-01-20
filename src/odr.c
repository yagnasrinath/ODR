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
#include "odr_functions.h"
#include <sys/time.h>

extern struct interface_socket* interface_socket_list[10];
extern int send_msg_flag;
extern int udomain_flag;
//extern int msg_reached_dest_flag;
extern int pf_packet_flag;
extern struct sockaddr_un cli_addr;
extern char* msg;
extern int staleness_value;
//extern char* rrep_buff;
//extern char* msg_buff;
//extern char* rreq_buff=(char*)malloc(sizeof((char*)rreq_buff)+1);

extern int unix_fd;

int main(int argc, char *argv[])
{
	fd_set rset;
	sscanf(argv[1],"%d",&staleness_value);
	/***********************************************************************/
	/*call functions that create unix domain,pf_packet sockets,routing table*/
	/***********************************************************************/
	int unix_socket=create_unix_socket();
	//printf("unix_socket created is %d\n",unix_socket);
	int num_pf_sockets=create_pf_packet_sockets();
	//printf("number of pf_packet sockets created are %d\n",num_pf_sockets);
	create_routing_table();
	//printf("routing table created\n");
	create_port_path_table();
	//printf("create port path table done\n");
	/************************************************************************/
	/*              computing maxfdp for select                             */
	/************************************************************************/
	int maxfdp=0,i=0;
	for(i=0;i<num_pf_sockets;i++)
	{
		if(maxfdp<interface_socket_list[i]->socket)
		{
			maxfdp = interface_socket_list[i]->socket;
		}
	}
	if(maxfdp<unix_socket)
		maxfdp=unix_socket;
	maxfdp = maxfdp + 1;

	FD_ZERO(&rset);
	/************************************************************************/
	/*       infinite loop that monitors sockets using select               */
	/************************************************************************/
	while(1)
	{
		for(i=0;i<num_pf_sockets;i++)
		{
			FD_SET(interface_socket_list[i]->socket, &rset);
		}
		FD_SET(unix_socket,&rset);
		//printf("waiting on select\n");
		int sel = select(maxfdp, &rset, NULL, NULL, NULL);
		if(FD_ISSET(unix_socket,&rset))
		{
			udomain_flag=1;
			unix_socket_set(unix_socket);
		}
		for(i=0;i<num_pf_sockets;i++)
		{
			if (FD_ISSET(interface_socket_list[i]->socket, &rset)) 
			{
				pf_packet_flag=1;
				pf_packet_function(interface_socket_list[i]->socket);               
			}
		}
	}
}
