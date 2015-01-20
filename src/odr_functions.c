#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <assert.h>
#include "msgapi.h"
#include "hw_addrs.h"
#include "hosts.h"   
#include "odr_functions.h"

#define CUSTOM_PROTO_NUM 2236
int send_msg_flag=0; 
struct interface_socket * interface_socket_list[10];
struct routing_table* routing_table_list[100];
int num_routing_list=100;
struct port_path* socket_port_path_list[25];
int a=0;
int unix_fd=0;
struct sockaddr_un odr_addr;
int staleness_value;
int dest_port,rc;
int route_redisc_flag;
int vm_srv_index=-1;
int send_msg_flag;
int udomain_flag;
int first_time_flag;
int pf_packet_flag;
int num_interface_socket;
struct sockaddr_un cli_addr;
extern int default_cli_port;
extern const char* odr_port;
extern const char* vm[10];
pthread_mutex_t route_mutex=PTHREAD_MUTEX_INITIALIZER;

uint8_t* hex_decode(const char *in, size_t len,uint8_t *out)
{
	unsigned int i, t, hn, ln;
	char c='\0',d='\0';

	for (t = 0,i = 0; i < len; i+=2,++t) {
		c = toupper(in[i]); 
		d = toupper(in[i+1]);
		hn = c > '9' ? c - 'A' + 10 : c - '0';
		ln = d > '9' ? d - 'A' + 10 : d - '0';

		out[t] = (hn << 4 ) | ln;
	}

	return out;
}


char * hex_encode(const uint8_t *in,size_t len,char*out)
{
	unsigned int i, t;
	uint8_t temp;
	for (t = 0,i = 0; t < len; i+=2,++t) {
		temp = (in[t]&0xF0)>>4;
		out[i] = temp<0xA? (temp + 0x30):temp+0x57;
		temp = (in[t]&0x0F);
		out[i+1] = temp<0xA? (temp + '0'):temp+0x57;
		//        	printf("%c%c\n",out[i],out[i+1]);
	}
	return out;
}

/*************************************************************************/
/*send_raw_packet*/
/*************************************************************************/
bool send_raw_packet(int sockfd,char* src_mac,char* dest_mac,char*payload)
{
	//printf("entering send_raw_packet\n");

	int j=0;
	/*buffer for ethernet frame*/
	void* buffer = (void*)malloc(ETH_FRAME_LEN);

	/*pointer to ethenet header*/
	unsigned char* etherhead = buffer;

	/*userdata in ethernet frame*/
	unsigned char* data = buffer + 14;

	/*another pointer to ethernet header*/
	struct ethhdr *eh = (struct ethhdr *)etherhead;

	int send_result = 0;

	/*set the frame header*/
	uint8_t res[6];
	hex_decode(dest_mac,strlen(dest_mac),res);
	memcpy((void*)buffer, (void*)res, 6);
	hex_decode(src_mac,strlen(src_mac),res);
	//printf("src_mac in send raw packet%s\n",src_mac);
	//printf("dest mac in send raw packet is  %s\n",dest_mac);
	memcpy((void*)(buffer+6), (void*)res, 6);
	eh->h_proto = htons(CUSTOM_PROTO_NUM);

	for (j = 0; j < 1500; j++) {
		data[j] = payload[j];
		//printf("%c",payload[j]);
	}
	//printf("\n");
	if((send_result = write(sockfd,buffer,ETH_FRAME_LEN)!=ETH_FRAME_LEN))
	{
		printf("unable to write\n");
		exit(0);
	}
	//printf("write works!\n");
}

/************************************************************************/
/* create_pf_packet_sockets                                              */
/*************************************************************************/
int create_pf_packet_sockets()
{
	//printf("entering create_pf_packet_sockets\n");
	struct hwa_info *hwa, *hwahead;
	struct sockaddr *sa;
	char   *ptr;
	char *ptr1;
	int    i, prflag;
	int pf_sockfd;
	int count=0;
	int j=0,k;
	int a=0;
	for(hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) 
	{
		count++;
	}

	for(hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) 
	{

		if(((hwa->if_name[0]=='l')&&(hwa->if_name[1]=='o'))||(0==strncmp(hwa->if_name,"eth0",4)))
		{
			continue;
		}
		i = 0;
		do
		{
			if (hwa->if_haddr[i] != '\0') 
			{
				break;
			}
		} while (++i < IF_HADDR);
		interface_socket_list[j]=(struct interface_socket*)malloc(sizeof(struct interface_socket));
		interface_socket_list[j]->interface_hw_addr = (char*)malloc(13);
		interface_socket_list[j]->interface_hw_addr[12]='\0';
		strcpy(interface_socket_list[j]->interface_hw_addr,hex_encode(hwa->if_haddr,6,interface_socket_list[j]->interface_hw_addr));
		interface_socket_list[j]->interface_index = hwa->if_index;
		if((pf_sockfd=socket(PF_PACKET,SOCK_RAW,htons(CUSTOM_PROTO_NUM))) < 0)
		{
			printf("pf_packet socket creation failed\n");
			exit(-1);
		}
		else
		{
			struct sockaddr_ll* sll = (struct sockaddr_ll*)malloc(sizeof(struct sockaddr_ll));
			socklen_t sll_len=sizeof(*sll);
			bzero(sll,sll_len);
			sll->sll_family=PF_PACKET;
			sll->sll_ifindex=hwa->if_index;
			sll->sll_protocol=htons(CUSTOM_PROTO_NUM);
			if((a=bind(pf_sockfd,(struct sockaddr *)sll,sll_len))<0)
			{
				printf("bind error when binding pf_packet socket to interface addresses with %s\n",strerror(errno));
				exit(-1);
			}
			else
			{
				interface_socket_list[j]->socket=pf_sockfd;
			}
			j++;
		}           
	}
	num_interface_socket=j;
	for(i=0;i<j;i++)
	{
		//printf("%d\t-%s-\t%d\n",interface_socket_list[i]->socket,interface_socket_list[i]->interface_hw_addr,interface_socket_list[i]->interface_index);
	}
	return j;      
	free_hwa_info(hwahead);
	exit(0);
}

/*************************************************************************/
/*search_port_path_table*/
/*************************************************************************/
char* search_port_path_table(int port)
{
	//printf("entering search_port_path_table\n");
	int i=0;
	for(i=0;i<25;i++)
	{
		if(socket_port_path_list[i]!=NULL&&socket_port_path_list[i]->port==port)
		{
			return socket_port_path_list[i]->socket_file_path;
		}
	}
	return NULL;
}

/*************************************************************************/
/*create_port_path_table*/
/*************************************************************************/
void create_port_path_table()
{
	//printf("entering create_port_path_table\n");
	int i=0;
	for(i=0;i<25;i++)
	{
		if(socket_port_path_list[i]==NULL)
		{
			break;
		}
	}
	int vm_srv_index=get_vm_ip();
	socket_port_path_list[i]=(struct port_path*)malloc(sizeof(struct port_path));
	socket_port_path_list[i]->port=default_port;
	socket_port_path_list[i]->socket_file_path = (char*)malloc(strlen(vm_port[vm_srv_index])+1);
	strcpy(socket_port_path_list[i]->socket_file_path,vm_port[vm_srv_index]);
	//printf("contents of port path table\n");
	for(i=0;i<25;i++)
	{
		if(socket_port_path_list[i]!=NULL)
		{
			//printf("%d\t%s\n",socket_port_path_list[i]->port,socket_port_path_list[i]->socket_file_path);
		}
	}
}
/*************************************************************************/
/*create_rreq*/
/*************************************************************************/
char* create_rreq(int payld_type,char*src_addr,char*dst_addr,int src_port,int dst_port,int hop_cnt,int b_id,int f_discovery,char*cli_path,char*msg)
{
	//printf("entering create_rreq\n");
	char a[13][1500];
	char* rreq_buff=(char*)malloc(1500);
	//it fills into structure variables, creates a buffer(global variable).
	rreq_packet.payload_type=payld_type;
	//printf("payload type in create_rreq(of packet being sent) is %d\n",payld_type);
	rreq_packet.canonical_source_addr = (char*)malloc(16);
	strcpy(rreq_packet.canonical_source_addr,src_addr);
	rreq_packet.canonical_dest_addr = (char*)malloc(16);
	strcpy(rreq_packet.canonical_dest_addr,dst_addr);
	rreq_packet.source_port=src_port;
	rreq_packet.dest_port=dst_port;
	rreq_packet.hop_count=hop_cnt;
	rreq_packet.broadcast_id=b_id;
	rreq_packet.num_bytes_msg=strlen(msg)+1;
	rreq_packet.forced_discovery=f_discovery;
	rreq_packet.rrep_already_sent=0;
	char* pay_ld_msg=(char*)malloc(strlen(msg)+1);
	rreq_packet.payload_msg=pay_ld_msg;
	strcpy(rreq_packet.payload_msg,msg);
	rreq_packet.msg_reply=0;
	char* cli_path_buf= (char*)malloc(128);
	rreq_packet.client_path=cli_path_buf;
	strcpy(rreq_packet.client_path,cli_path);
	sprintf(a[0],"%d",rreq_packet.payload_type);
	strcpy(a[1],rreq_packet.canonical_source_addr);
	strcpy(a[2],rreq_packet.canonical_dest_addr);
	sprintf(a[3],"%d",rreq_packet.source_port);
	sprintf(a[4],"%d",rreq_packet.dest_port);
	sprintf(a[5],"%d",rreq_packet.hop_count);
	sprintf(a[6],"%d",rreq_packet.broadcast_id);
	sprintf(a[7],"%d",rreq_packet.num_bytes_msg);
	sprintf(a[8],"%d",rreq_packet.forced_discovery);
	sprintf(a[9],"%d",rreq_packet.rrep_already_sent);
	sprintf(a[10],"%d",rreq_packet.msg_reply);
	strcpy(a[11],rreq_packet.payload_msg);
	strcpy(a[12],rreq_packet.client_path);
	int count=0;
	//  int x=0,i;
	int j=-1;
	char*temp_rreq = rreq_buff;
	for(count=0;count<13;count++)     //check if '\0' has to be inserted explicitly
	{
		*(temp_rreq+j)='\0';
		temp_rreq=temp_rreq+j+1;
		j=sprintf(temp_rreq,"%s",a[count]);
	}       
	//printf("exiting create_rreq\n");
	return rreq_buff;
}

int get_vm_index(char * ip)
{
	int i=0;
	for(i=0;i<10;i++)
	{
		if(strncmp(ip,vm[i],strlen(vm[i]))==0)
		{
			return i;
		}
	}
	return 0;
}

/*************************************************************************/
/*msg_received*/
/*************************************************************************/
void msg_received(int pf_sock_fd, char*src_mac,char*dest_mac, struct generic_payload* gen_payload_ptr)
{
	//printf("entering msg_received\n");
	int a=0,i=0,j=0,k=0,l=0;
	char* dst_port,src_port,dst_ip_addr,src_ip_addr;
	struct sockaddr_un server_addr;
	char  buff[256]={'\0'};
	socklen_t server_len= sizeof(server_addr);
	if((a=check_msg_dest_reached(gen_payload_ptr->canonical_dest_addr))==1)
	{
		//write to server
		//printf("msg dest reached\n");
		for(i=0;i<10;i++)
		{
			if(strcmp(gen_payload_ptr->canonical_dest_addr,vm[i])==0)
			{   
				break;
			}
		}
		assert(i<10);
		memset(&server_addr, 0, sizeof(server_addr));
		//printf("port:%d path:%s\n",gen_payload_ptr->dest_port,search_port_path_table(gen_payload_ptr->dest_port));
		strcpy(server_addr.sun_path,search_port_path_table(gen_payload_ptr->dest_port));
		server_addr.sun_family=AF_UNIX;
		j=sprintf(buff,"%s",gen_payload_ptr->payload_msg);
		//printf("msg buf%s\t%d\n",gen_payload_ptr->payload_msg,strlen(gen_payload_ptr->payload_msg)+1);
		assert(sendto(unix_fd,buff,j+1,0,(struct sockaddr*)&server_addr,server_len)>0);
		//printf("msg written to server,%s\n",buff);
		j=sprintf(buff,"%s",gen_payload_ptr->canonical_source_addr);
		assert(sendto(unix_fd,buff,j+1,0,(struct sockaddr*)&server_addr,server_len)>0);
		//printf("msg written to server,%s\n",buff);
		j=sprintf(buff,"%d",gen_payload_ptr->source_port);
		assert(sendto(unix_fd,buff,j+1,0,(struct sockaddr*)&server_addr,server_len)>0);
		//printf("msg written to server,%s\n",buff);
	}
	else if(a==0)
	{
		//printf("msg dest not reached\n");
		char* rreq_received_rreq_buff=create_rreq(2,gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,gen_payload_ptr->source_port,gen_payload_ptr->dest_port,gen_payload_ptr->hop_count+1,gen_payload_ptr->broadcast_id,gen_payload_ptr->forced_discovery,gen_payload_ptr->client_path,gen_payload_ptr->payload_msg);
		int i=0,j=0;
		for(i=0;i<num_routing_list;i++)
		{
			if((strcmp(gen_payload_ptr->canonical_source_addr,routing_table_list[i]->source_canonical_ip_addr)==0)&&(strcmp(gen_payload_ptr->canonical_dest_addr,routing_table_list[i]->dest_canonical_ip_addr)==0))
			{
				break;		
			}
		}
		for(j=0;j<num_interface_socket;j++)
		{
			if(interface_socket_list[j]->interface_index==routing_table_list[i]->outgoing_interface_index)
			{
				break;
			}
		}
		vm_srv_index = get_vm_ip();
		printf("ODR at node vm%d sending frame hdr src vm%d dest %c%c:%c%c:%c%c:%c%c:%c%c:%c%c ODR msg type %d src vm%d dest vm%d\n",vm_srv_index+1,vm_srv_index+1,routing_table_list[i]->nxt_hop_eth_addr[0],routing_table_list[i]->nxt_hop_eth_addr[1],routing_table_list[i]->nxt_hop_eth_addr[2],routing_table_list[i]->nxt_hop_eth_addr[3],routing_table_list[i]->nxt_hop_eth_addr[4],routing_table_list[i]->nxt_hop_eth_addr[5],routing_table_list[i]->nxt_hop_eth_addr[6],routing_table_list[i]->nxt_hop_eth_addr[7],routing_table_list[i]->nxt_hop_eth_addr[8],routing_table_list[i]->nxt_hop_eth_addr[9],routing_table_list[i]->nxt_hop_eth_addr[10],routing_table_list[i]->nxt_hop_eth_addr[11],2,get_vm_index(gen_payload_ptr->canonical_source_addr)+1,get_vm_index(gen_payload_ptr->canonical_dest_addr)+1);
		send_raw_packet(interface_socket_list[j]->socket,interface_socket_list[j]->interface_hw_addr,routing_table_list[i]->nxt_hop_eth_addr,rreq_received_rreq_buff);
	}

}

void print_routing_table()
{
	//printf("entering print_routing_table\n");
	//printf("source_canonical_ip_addr\tdest_canonical_ip_addr\tnxt_hop_eth_addr\treceived_from_eth_addr\toutgoing_interface_index\thop_count\tnum_hops_to_dest\tbroadcast_id\ttv\n");
	int k=0;
	for(k=0;(k<num_routing_list);k++)
	{
		if(routing_table_list[k]!=NULL)
		{
			//printf("%d\t-%s\t-%s\t-%s\t-%s\t-%d\t-%d\t-%d\t-%d\t-%d sec %dusec\n",k,routing_table_list[k]->source_canonical_ip_addr,routing_table_list[k]->dest_canonical_ip_addr,routing_table_list[k]->nxt_hop_eth_addr,routing_table_list[k]->received_from_eth_addr,routing_table_list[k]->outgoing_interface_index,routing_table_list[k]->incoming_interface_index,routing_table_list[k]->hop_count,routing_table_list[k]->num_hops_to_dest,routing_table_list[k]->broadcast_id,routing_table_list[k]->tv.tv_sec,routing_table_list[k]->tv.tv_usec);
		}	
	}

}

/*************************************************************************/
/*enter_in_routing_table*/
/*************************************************************************/
void enter_in_routing_table(char* source_addr,char* dest_addr,char* nxt_hp,char*recv_from_eth_addr,int out_if_index,int incmg_if_index,int hop_cnt,int hop_dest,int b_id,int sock_fd)
{
	//printf("entering enter_in_routing_table\n");
	//printf("Acquiring lock\n");
	pthread_mutex_lock(&route_mutex);
	//printf("Acquired lock\n");
	int k=0,i=0;
	time_t ltime;
	for(k=0;(routing_table_list[k]!=NULL)&&(k<num_routing_list);k++)
	{
	}
	//printf("-------------%s\t%s\t%d\n",source_addr,dest_addr,sock_fd);
	routing_table_list[k]=(struct routing_table*)malloc(sizeof(struct routing_table));
	gettimeofday(&(routing_table_list[k])->tv,NULL);
	routing_table_list[k]->dest_canonical_ip_addr = (char*)malloc(16);
	routing_table_list[k]->source_canonical_ip_addr = (char*)malloc(16);
	routing_table_list[k]->received_from_eth_addr= (char*)malloc(13);
	routing_table_list[k]->nxt_hop_eth_addr = (char*)malloc(13);
	strcpy(routing_table_list[k]->dest_canonical_ip_addr,dest_addr);
	strcpy(routing_table_list[k]->source_canonical_ip_addr,source_addr);
	strcpy(routing_table_list[k]->received_from_eth_addr,recv_from_eth_addr);
	strcpy(routing_table_list[k]->nxt_hop_eth_addr,nxt_hp);
	routing_table_list[k]->outgoing_interface_index=out_if_index;
	routing_table_list[k]->incoming_interface_index=incmg_if_index;
	routing_table_list[k]->hop_count=hop_cnt;
	routing_table_list[k]->num_hops_to_dest=hop_dest;
	routing_table_list[k]->broadcast_id=b_id;
	//printf("unlocking\n");
	pthread_mutex_unlock(&route_mutex);
	//printf("unlocked\n");
}

/*************************************************************************/
/*rreq_received*/
/*************************************************************************/
void rreq_received(int pf_sock_fd, char*src_mac,char*dest_mac, struct generic_payload* gen_payload_ptr)
{
	//printf("entering rreq_received\n");
	int a,i,j;
	int route_exists=0;
	time_t ltime;
	int in_if_index=0;
	for(i=0;i<num_interface_socket;i++)
	{
		if(interface_socket_list[i]->socket==pf_sock_fd)
		{
			in_if_index=interface_socket_list[i]->interface_index;
			break;
		}
	}
	if((a=check_rreq_dest_reached(gen_payload_ptr->canonical_dest_addr))==1)
	{
		//printf("dest reached\n");
		enter_in_routing_table(gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,"",src_mac,-1,in_if_index,gen_payload_ptr->hop_count,0,gen_payload_ptr->broadcast_id,pf_sock_fd);
		char* rreq_received_rreq_buff=create_rreq(1,gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,gen_payload_ptr->source_port,gen_payload_ptr->dest_port,1,gen_payload_ptr->broadcast_id,gen_payload_ptr->forced_discovery,gen_payload_ptr->client_path,gen_payload_ptr->payload_msg);

		vm_srv_index = get_vm_ip();
		printf("ODR at node vm%d sending frame hdr src vm%d dest %c%c:%c%c:%c%c:%c%c:%c%c:%c%c ODR msg type %d src vm%d dest vm%d\n",vm_srv_index+1,vm_srv_index+1,src_mac[0],src_mac[1],src_mac[2],src_mac[3],src_mac[4],src_mac[5],src_mac[6],src_mac[7],src_mac[8],src_mac[9],src_mac[10],src_mac[11],1,get_vm_index(gen_payload_ptr->canonical_source_addr)+1,get_vm_index(gen_payload_ptr->canonical_dest_addr)+1);
		send_raw_packet(pf_sock_fd,interface_socket_list[i]->interface_hw_addr,src_mac,rreq_received_rreq_buff);
	}
	else if(a==0)   
	{
		print_routing_table();	
		if((route_exists=check_routing_table(gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,pf_sock_fd))==1)
		{
			//printf("route exists\n");
			for(i=0;i<num_routing_list;i++)
			{
				if(routing_table_list[i]!=NULL)
				{
					if(strcmp(gen_payload_ptr->canonical_dest_addr,routing_table_list[i]->dest_canonical_ip_addr)==0)
					{
						if(strcmp(gen_payload_ptr->canonical_source_addr,routing_table_list[i]->source_canonical_ip_addr)==0)
						{
							if(routing_table_list[i]->broadcast_id>=gen_payload_ptr->broadcast_id)
							{
								//printf("Already broadcasted\n");
								return;
							}
							else
							{
								if(gen_payload_ptr->forced_discovery=1)
								{
									//printf("Acquiring lock\n");
									pthread_mutex_lock(&route_mutex);
									//printf("Acquired lock\n");
									free(routing_table_list[i]);
									routing_table_list[i]=NULL;
									//printf("unlocking\n");
									pthread_mutex_unlock(&route_mutex);
									//printf("unlocked\n");
									enter_in_routing_table(gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,"",src_mac,-1,in_if_index,gen_payload_ptr->hop_count,0,gen_payload_ptr->broadcast_id,pf_sock_fd);
									char* rreq_received_rreq_buff=create_rreq(0,gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,gen_payload_ptr->source_port,gen_payload_ptr->dest_port,gen_payload_ptr->hop_count+1,gen_payload_ptr->broadcast_id,gen_payload_ptr->forced_discovery,gen_payload_ptr->client_path,gen_payload_ptr->payload_msg);
									//delete and enter in routing table
									broadcast_rreq(rreq_received_rreq_buff,gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr);
								}
								else
								{
									if(gen_payload_ptr->hop_count < routing_table_list[i]->hop_count)
									{
										//printf("Acquiring lock\n");
										pthread_mutex_lock(&route_mutex);
										//printf("Acquired lock\n");
										routing_table_list[i]->incoming_interface_index=in_if_index;
										strcpy(routing_table_list[i]->received_from_eth_addr,src_mac);
										routing_table_list[i]->hop_count=gen_payload_ptr->hop_count;
										gettimeofday(&(routing_table_list[i])->tv,NULL);
										//printf("unlocking\n");
										pthread_mutex_unlock(&route_mutex);
										//printf("unlocked\n");
										break;
									}
									if(gen_payload_ptr->hop_count == routing_table_list[i]->hop_count)
									{
										for(j=0;num_routing_list;j++)
										{
											if(routing_table_list[j]!=NULL)
											{
												if(strcmp(routing_table_list[i]->received_from_eth_addr,src_mac)!=0)
												{
													enter_in_routing_table(gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,routing_table_list[i]->nxt_hop_eth_addr,src_mac,routing_table_list[i]->outgoing_interface_index,in_if_index,gen_payload_ptr->hop_count,routing_table_list[i]->num_hops_to_dest,gen_payload_ptr->broadcast_id,pf_sock_fd);
												}
											}
										}
									}   
									char* rreq_received_rreq_buff=create_rreq(1,gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,gen_payload_ptr->source_port,gen_payload_ptr->dest_port,1+routing_table_list[i]->num_hops_to_dest,gen_payload_ptr->broadcast_id,gen_payload_ptr->forced_discovery,gen_payload_ptr->client_path,gen_payload_ptr->payload_msg);
									vm_srv_index = get_vm_ip();
									printf("ODR at node vm%d sending frame hdr src vm%d dest %c%c:%c%c:%c%c:%c%c:%c%c:%c%c ODR msg type %d src vm%d dest vm%d\n",vm_srv_index+1,vm_srv_index+1,src_mac[0],src_mac[1],src_mac[2],src_mac[3],src_mac[4],src_mac[5],src_mac[6],src_mac[7],src_mac[8],src_mac[9],src_mac[10],src_mac[11],1,get_vm_index(gen_payload_ptr->canonical_source_addr)+1,get_vm_index(gen_payload_ptr->canonical_dest_addr)+1);
									send_raw_packet(pf_sock_fd,interface_socket_list[i]->interface_hw_addr,src_mac,rreq_received_rreq_buff);
								}
							}
						}   
					}                   
				}
			}       

		}
		else
		{
			//printf("route does not exist\n");
			enter_in_routing_table(gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,"",src_mac,-1,in_if_index,gen_payload_ptr->hop_count,0,gen_payload_ptr->broadcast_id,pf_sock_fd);
			char* rreq_received_rreq_buff=create_rreq(0,gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,gen_payload_ptr->source_port,gen_payload_ptr->dest_port,gen_payload_ptr->hop_count+1,gen_payload_ptr->broadcast_id,gen_payload_ptr->forced_discovery,gen_payload_ptr->client_path,gen_payload_ptr->payload_msg);
			broadcast_rreq(rreq_received_rreq_buff,gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr);
		}
	}
}


/*************************************************************************/
/*insert_port_path_table*/
/*************************************************************************/
int insert_port_path_table(char* cli_path)    //do this also has a timeout parameter?==> entries removed after timeout?
{
	//printf("entering insert_port_path_table\n");
	int i=0,max=0;
	for(i=0;i<25;i++)
	{
		if(socket_port_path_list[i]!=NULL)
		{
			if(max<socket_port_path_list[i]->port)
			{
				max= socket_port_path_list[i]->port;
			}	
		}
	}
	for(i=0;i<25;i++)
	{
		if(socket_port_path_list[i]!=NULL)
		{
			if(strcmp(cli_path,socket_port_path_list[i]->socket_file_path)==0)
			{
				return  socket_port_path_list[i]->port;	
			}
		}
	}
	for(i=0;i<25;i++)
	{
		if(socket_port_path_list[i]==NULL)
		{
			break;
		}
	}
	socket_port_path_list[i]=(struct port_path*)malloc(sizeof(struct port_path));
	socket_port_path_list[i]->port=max+1;
	socket_port_path_list[i]->socket_file_path = (char*)malloc(strlen(cli_path)+1);
	strcpy(socket_port_path_list[i]->socket_file_path,cli_path);
	return  socket_port_path_list[i]->port;	

}
/*************************************************************************/
/*create_unix_socket*/
/*************************************************************************/
int create_unix_socket()
{
	//printf("entering create_unix_socket\n");
	if((unix_fd = socket(AF_UNIX,SOCK_DGRAM,0)) == -1)
	{
		perror("socket error");
		exit(-1);
	}

	memset(&odr_addr, 0, sizeof(odr_addr));
	odr_addr.sun_family = AF_UNIX;
	strncpy(odr_addr.sun_path, odr_port, sizeof(odr_addr.sun_path)-1);
	//printf("odr_port in odr_functions is %s\n",odr_port);
	unlink(odr_port);
	if((a=bind(unix_fd,(struct sockaddr*)&odr_addr,sizeof(odr_addr))) < 0)
	{       
		printf("bind failed in odr during unix socket bind with %s\n",strerror(errno));
	}
	return unix_fd;
}

/*************************************************************************/
/*create_routing_table*/
/*************************************************************************/
void create_routing_table()
{
	//printf("entering create_routing_table\n");
}


/*************************************************************************/
/*check_rreq_dest_reached*/
/*************************************************************************/
int check_rreq_dest_reached(char*canonical_dest_addr)
{
	//printf("entering check_rreq_dest_reached\n");
	// when it reaches dest, dest_mac addr should be copied into it
	int vm_srv_index=get_vm_ip();
	int i=0;
	if(strcmp(canonical_dest_addr,vm[vm_srv_index])==0)
	{   
		i= 1;
	}
	return i;
}
/*************************************************************************/
/*check_rrep_dest_reached*/
/*************************************************************************/
int check_rrep_dest_reached(int pf_sockfd,char*canonical_source_addr)
{
	//printf("entering check_rrep_dest_reached\n");
	int vm_srv_index=get_vm_ip();
	int i=0;
	if(strcmp(canonical_source_addr,vm[vm_srv_index])==0)
	{  
		i=1;
	}
	return i;
}
/*************************************************************************/
/*check_msg_dest_reached*/
/*************************************************************************/
int check_msg_dest_reached(char*canonical_dest_addr)
{
	//printf("entering check_msg_dest_reached\n");
	//  msg_reached_dest_flag=0;
	int vm_srv_index=get_vm_ip();
	int i=0;
	if(strcmp(canonical_dest_addr,vm[vm_srv_index])==0)
	{  i=1; 
	}   
	return i;
}


/*************************************************************************/
/*rrep_received*/
/*************************************************************************/
void rrep_received(int pf_sock_fd, char*src_mac,char*dest_mac, struct generic_payload* gen_payload_ptr)
{
	//printf("entering rrep_received\n");
	int a=0,b=0,c=0,d=0,i,j,k,l=0;
	int out_index=0;
	for(k=0;k<num_interface_socket;k++)
	{
		if(interface_socket_list[k]->socket==pf_sock_fd)
		{
			out_index=interface_socket_list[k]->interface_index;
			break;
		}
	}
	if((a=check_rrep_dest_reached(pf_sock_fd,gen_payload_ptr->canonical_source_addr))==1)
	{   
		//printf("rrep dest reached\n");
		for(i=0;i<num_routing_list;i++)
		{
			if(routing_table_list[i]!=NULL &&(strcmp(routing_table_list[i]->source_canonical_ip_addr,gen_payload_ptr->canonical_source_addr)==0)&&(strcmp(routing_table_list[i]->dest_canonical_ip_addr,gen_payload_ptr->canonical_dest_addr)==0))
			{
				//printf("Acquiring lock\n");
				pthread_mutex_lock(&route_mutex);
				//printf("Acquired lock\n");
				strcpy(routing_table_list[i]->nxt_hop_eth_addr,src_mac);
				routing_table_list[i]->outgoing_interface_index=out_index;
				routing_table_list[i]->num_hops_to_dest=gen_payload_ptr->hop_count;
				gettimeofday(&(routing_table_list[i])->tv,NULL);
				//printf("unlocking\n");
				pthread_mutex_unlock(&route_mutex);
				//printf("unlocked\n");
				break;	
			}
		}
	}
	else if(a==0)
	{
		//printf("rrep dest not reached\n");
		for(i=0;i<num_routing_list;i++)
		{

			if(routing_table_list[i]!=NULL)
			{
				if(((b=strcmp(routing_table_list[i]->dest_canonical_ip_addr,gen_payload_ptr->canonical_dest_addr))==0)&&((c=strcmp(routing_table_list[i]->source_canonical_ip_addr,gen_payload_ptr->canonical_source_addr))==0))
				{
					for(j=0;j<num_interface_socket;j++)
					{
						if(interface_socket_list[j]->socket==pf_sock_fd)
						{
							//printf("Acquiring lock\n");
							pthread_mutex_lock(&route_mutex);
							//printf("Acquired lock\n");
							strcpy(routing_table_list[i]->nxt_hop_eth_addr,src_mac);
							routing_table_list[i]->outgoing_interface_index = interface_socket_list[j]->interface_index;
							routing_table_list[i]->num_hops_to_dest = gen_payload_ptr->hop_count;
							gettimeofday(&(routing_table_list[i])->tv,NULL);
							//printf("unlocking\n");
							pthread_mutex_unlock(&route_mutex);
							//printf("unlocked\n");
							break;
						}
					}
					char* rreq_received_rreq_buff=create_rreq(1,gen_payload_ptr->canonical_source_addr,gen_payload_ptr->canonical_dest_addr,gen_payload_ptr->source_port,gen_payload_ptr->dest_port,gen_payload_ptr->hop_count+1,gen_payload_ptr->broadcast_id,gen_payload_ptr->forced_discovery,gen_payload_ptr->client_path,gen_payload_ptr->payload_msg);
					for(l=0;l<num_interface_socket;l++)
					{
						if(interface_socket_list[l]->interface_index == routing_table_list[i]->incoming_interface_index)
						{
							break;
						}
					}
					//printf("rrep destination:%s\tsource:%s\n",routing_table_list[i]->received_from_eth_addr,interface_socket_list[l]->interface_hw_addr);
					vm_srv_index = get_vm_ip();
					printf("ODR at node vm%d sending frame hdr src vm%d dest %c%c:%c%c:%c%c:%c%c:%c%c:%c%c ODR msg type %d src vm%d dest vm%d\n",vm_srv_index+1,vm_srv_index+1,routing_table_list[i]->received_from_eth_addr[0],routing_table_list[i]->received_from_eth_addr[1],routing_table_list[i]->received_from_eth_addr[2],routing_table_list[i]->received_from_eth_addr[3],routing_table_list[i]->received_from_eth_addr[4],routing_table_list[i]->received_from_eth_addr[5],routing_table_list[i]->received_from_eth_addr[6],routing_table_list[i]->received_from_eth_addr[7],routing_table_list[i]->received_from_eth_addr[8],routing_table_list[i]->received_from_eth_addr[9],routing_table_list[i]->received_from_eth_addr[10],routing_table_list[i]->received_from_eth_addr[11],1,get_vm_index(gen_payload_ptr->canonical_source_addr)+1,get_vm_index(gen_payload_ptr->canonical_dest_addr)+1);
					send_raw_packet(interface_socket_list[l]->socket,interface_socket_list[l]->interface_hw_addr,routing_table_list[i]->received_from_eth_addr,rreq_received_rreq_buff);

					break;
				}
			}
		}
	}
}

/*************************************************************************/
/*pf_packet_function*/
/*************************************************************************/
void pf_packet_function(int pf_sockfd)
{
	//printf("entering pf_packet_function\n");
	char src_mac[13]={'\0'};
	char dest_mac[13]={'\0'};
	struct generic_payload payload;
	char cli_path[255];
	char msg[255];  
	payload.client_path = cli_path;
	payload.payload_msg = msg;
	read_pf_socket(pf_sockfd,src_mac,dest_mac,&payload);
	int i=0;

	//printf("payoad type%d\n",payload.payload_type);
	if(payload.payload_type==0)
		rreq_received(pf_sockfd,src_mac,dest_mac,&payload);
	else if(payload.payload_type==1)
		rrep_received(pf_sockfd,src_mac,dest_mac,&payload);
	else if(payload.payload_type==2)
		msg_received(pf_sockfd,src_mac,dest_mac,&payload);
	//printf("exiting pf_packet_function\n");
}
/*************************************************************************/
/*read_pf_socket*/
/*************************************************************************/
read_pf_socket(int pf_sock_fd, char*src_mac,char*dest_mac,struct generic_payload*payload)
{
	//printf("entering read_pf_socket\n");
	int num_read=0;
	char buff[1514];
	int buff_len=1514;
	bzero(buff,buff_len);
	int x=0,count=0,i,j;
	char a[13][1500];  
	char* ether_protocol;
	char* pyld_type;
	char* src_port;
	char* dest_port;
	char* hp_cnt;
	char* b_id;
	char* num_bytes;
	char* forced_disc;
	char* rrep_sent;
	char* msg_reply_str;
	//i have to read a buf into its parts and fill in my structure;
	if((num_read=read(pf_sock_fd,buff,buff_len))<0)
	{
		printf("ethernet frame reading error\n");
		exit(0);
	}
	else
	{
	//	printf("num of bytes of ethernet frame read are %d\n",num_read);
	}
	char * eth_frame = buff;
	hex_encode((uint8_t*)eth_frame,6,dest_mac);
	hex_encode((uint8_t*)eth_frame+6,6,src_mac);

	for(i=14;i<1514;i++)
	{
	//	printf("%c",buff[i]);
	}
	//printf("--\n");
	i=0;//to skip header 
	x=14;
	while(x < buff_len-14 && count < 13)    //skipping crc and header
	{
		for(i=x,j=0;buff[i]!='\0';i++,j++)
		{
			a[count][j]=buff[i];

		}
		a[count][j]='\0';
		count++;
		x=i+1;
	}
	sscanf(a[0],"%d",&payload->payload_type);
	char *csa=(char*)malloc(16);
	payload->canonical_source_addr = csa;
	strcpy(payload->canonical_source_addr,a[1]);
	char *cda=(char*)malloc(16);
	payload->canonical_dest_addr = cda;
	strcpy(payload->canonical_dest_addr,a[2]);
	sscanf(a[3],"%d",&payload->source_port);
	sscanf(a[4],"%d",&payload->dest_port);
	sscanf(a[5],"%d",&payload->hop_count);
	sscanf(a[6],"%d",&payload->broadcast_id);
	sscanf(a[7],"%d",&payload->num_bytes_msg);
	sscanf(a[8],"%d",&payload->forced_discovery);
	sscanf(a[9],"%d",&payload->rrep_already_sent);
	sscanf(a[10],"%d",&payload->msg_reply);
	strcpy(payload->payload_msg,a[11]);
	strcpy(payload->client_path,a[12]);
}

/*************************************************************************/
/*broadcast_rreq*/
/*************************************************************************/
void broadcast_rreq(char* payload,char*canonical_src_addr,char*canonical_dst_addr)
{
	//printf("entering broadcast_rreq\n");
	int i=0,j=0,n=0,dont_broadcast=0;

	for(i=0;i<num_interface_socket;i++)
	{
		//printf("src_mac in broadcast_rreq:%s\n",interface_socket_list[i]->interface_hw_addr);
		vm_srv_index = get_vm_ip();
		printf("ODR at node vm%d sending frame hdr src vm%d dest FF:FF:FF:FF:FF:FF ODR msg type %d src vm%d dest vm%d\n",vm_srv_index+1,vm_srv_index+1,0,get_vm_index(canonical_src_addr)+1,get_vm_index(canonical_dst_addr)+1);
		send_raw_packet(interface_socket_list[i]->socket,interface_socket_list[i]->interface_hw_addr,"FFFFFFFFFFFF",payload);
	}

}

int timevaldiff(struct timeval *starttime)
{
	int sec;
	struct timeval finishtime;
	gettimeofday(&finishtime, NULL);
	sec=(finishtime.tv_sec-starttime->tv_sec);
	sec+=(finishtime.tv_usec-starttime->tv_usec)/1000000;
	return sec;
}


/*************************************************************************/
/*check_routing_table*/
/*************************************************************************/
int check_routing_table(char*source_addr,char*dest_addr,int sock_fd)
{
	int k=0;
	int j=0;
	//printf("entering check_routing_table\n");
	//printf("Acquiring lock\n");
	pthread_mutex_lock(&route_mutex);
	//printf("Acquired lock\n");
	
	for(k=0;k<num_routing_list;k++)
	{
		if(routing_table_list[k]!=NULL&&(timevaldiff(&routing_table_list[k]->tv)>staleness_value))
		{
			free(routing_table_list[k]);		
		}
	}
	
	//printf("unlocking\n");
	pthread_mutex_unlock(&route_mutex);
	//printf("unlocked\n");
	for(k=0;k<num_routing_list;k++)
	{
		if(routing_table_list[k]!=NULL)
		{
			if(strcmp(dest_addr,routing_table_list[k]->dest_canonical_ip_addr)==0)
			{
				return 1;
			}
		}
	}
	return 0;
}

int get_routing_table_index(char*source_addr,char*dest_addr)
{
	//printf("entering check_routing_table\n");
	int k=0;
	int j=0;
	for(k=0;k<num_routing_list;k++)
	{
		if(routing_table_list[k]!=NULL)
		{
			if(strcmp(dest_addr,routing_table_list[k]->dest_canonical_ip_addr)==0)
			{
				return k;
			}
		}
	}
	return -1;
}

/*************************************************************************/
/*get_vm_ip*/
/*************************************************************************/
int get_vm_ip()
{
	//printf("entering get_vm_ip\n");
	extern const char* vm[10];
	struct hwa_info *hwa, *hwahead;
	struct sockaddr *sa;
	char *cli_vm_addr;
	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next)
	{
		if ( (sa = hwa->ip_addr) != NULL)
		{
			cli_vm_addr = Sock_ntop_host(sa, sizeof(*sa));
			int j = 0;
			for(j = 0;j<10;j++)
			{
				//printf("-----------%s-----%s\n",cli_vm_addr,vm[j]);
				if(strncmp(cli_vm_addr,vm[j],14)==0)
				{
					free_hwa_info(hwahead);
					return j;
				}
			}
		}
	}
	//free_hwa_info(hwahead);
	return -1;
}

/*************************************************************************/
/*check_routing_table_unix*/
/*************************************************************************/
/*
bool check_routing_table_unix(char*source_addr,char*dest_addr)
{
	printf("entering check_routing_table_unix\n");
	int k=0;
	for(k=0;k<100;k++)
	{
		if(routing_table_list[k]!=NULL)
		{
			if(strcmp(dest_addr,routing_table_list[k]->dest_canonical_ip_addr)==0)
			{
				return true;
			}
		}
	}
	return false;
}
*/
void *thread_send_msg(void* gp_ptr)
{
	pthread_detach(pthread_self());
	//printf("entering thread_send_msg\n");
	struct generic_payload *gp = (struct generic_payload*)gp_ptr;
	//create mesage and send	
	while(1)
	{
		//printf("Acquiring lock\n");
		pthread_mutex_lock(&route_mutex);
		//printf("Acquired lock\n");
		int route_index=get_routing_table_index(gp->canonical_source_addr,gp->canonical_dest_addr);
		int i=0;
		if((route_index!=-1)&&(routing_table_list[route_index]->outgoing_interface_index!=-1))
		{
			//printf("in if of thread\n");
			print_routing_table();
			for(i=0;i<num_interface_socket;i++)
			{
				if(interface_socket_list[i]->interface_index==routing_table_list[route_index]->outgoing_interface_index)
				{
					break;
				}
			}
			char *payload = create_rreq(2,gp->canonical_source_addr,gp->canonical_dest_addr,gp->source_port,gp->dest_port,routing_table_list[route_index]->num_hops_to_dest,routing_table_list[route_index]->broadcast_id,0,gp->client_path,gp->payload_msg);

			//printf("before send raw packet----%d\n",i);
			//vm_srv_index = get_vm_ip();
			printf("ODR at node vm%d sending frame hdr src vm%d dest %c%c:%c%c:%c%c:%c%c:%c%c:%c%c ODR msg type %d src vm%d dest vm%d\n",vm_srv_index+1,vm_srv_index+1,routing_table_list[route_index]->nxt_hop_eth_addr[0],routing_table_list[route_index]->nxt_hop_eth_addr[1],routing_table_list[route_index]->nxt_hop_eth_addr[2],routing_table_list[route_index]->nxt_hop_eth_addr[3],routing_table_list[route_index]->nxt_hop_eth_addr[4],routing_table_list[route_index]->nxt_hop_eth_addr[5],routing_table_list[route_index]->nxt_hop_eth_addr[6],routing_table_list[route_index]->nxt_hop_eth_addr[7],routing_table_list[route_index]->nxt_hop_eth_addr[8],routing_table_list[route_index]->nxt_hop_eth_addr[9],routing_table_list[route_index]->nxt_hop_eth_addr[10],routing_table_list[route_index]->nxt_hop_eth_addr[11],2,get_vm_index(gp->canonical_source_addr)+1,get_vm_index(gp->canonical_dest_addr)+1);
			send_raw_packet(interface_socket_list[i]->socket,interface_socket_list[i]->interface_hw_addr,routing_table_list[route_index]->nxt_hop_eth_addr,payload);
			//printf("unlocking\n");
			pthread_mutex_unlock(&route_mutex);
			//printf("unlocked\n");
			pthread_exit(NULL);
		}
		//printf("unlocking\n");
		pthread_mutex_unlock(&route_mutex);
		//printf("unlocked\n");
		sleep(2);	
	}
}
/*************************************************************************/
/*unix_socket_set*/
/*************************************************************************/
void unix_socket_set(int unix_fd)
{
	//printf("entering unix_socket_set\n");
	char msg[255];
	char source_addr[16]={'\0'};
	char dest_addr[16]={'\0'};
	socklen_t cli_len=sizeof(cli_addr);
	if((rc=recvfrom(unix_fd,dest_addr,sizeof(dest_addr),0,(struct sockaddr*)&cli_addr,&cli_len))<0)
	{
		printf("recvfrom failed\n");
		exit(0);
	}
	rc=0;
	if((rc=read(unix_fd,(void *)&dest_port,sizeof(dest_port))) < 0)
	{
		printf("destination port reading error\n");
		exit(0);
	}
	rc=0;
	if((rc=read(unix_fd,msg,sizeof(msg))) < 0)
	{
		printf("msg reading error\n");
		exit(0);
	}
	rc=0;

	if((rc=read(unix_fd,(void *)&route_redisc_flag,sizeof(route_redisc_flag))) < 0)
	{
		printf("route rediscovery flag reading error\n");
		exit(0);
	}
	vm_srv_index = get_vm_ip();
	if(vm_srv_index == -1)
	{       
		printf("cant find IP\n");
		exit(-1);
	}
	else
	{
		strcpy(source_addr,vm[vm_srv_index]);
	}
	int cli_port = insert_port_path_table(cli_addr.sun_path);
	int i=0;
	if(check_routing_table(source_addr,dest_addr,0)==1&&route_redisc_flag!=1)
	{
		//create mesage and send	
		int route_index=get_routing_table_index(source_addr,dest_addr);
		char *payload = create_rreq(2,source_addr,dest_addr,cli_port,dest_port,routing_table_list[route_index]->num_hops_to_dest,routing_table_list[route_index]->broadcast_id,0,cli_addr.sun_path,msg);
		for(i=0;i<num_interface_socket;i++)
		{
			if(interface_socket_list[i]->interface_index==routing_table_list[route_index]->outgoing_interface_index)
			{
				break;
			}
		}
		vm_srv_index = get_vm_ip();
		printf("ODR at node vm%d sending frame hdr src vm%d dest %c%c:%c%c:%c%c:%c%c:%c%c:%c%c ODR msg type %d src vm%d dest vm%d\n",vm_srv_index+1,vm_srv_index+1,routing_table_list[route_index]->nxt_hop_eth_addr[0],routing_table_list[route_index]->nxt_hop_eth_addr[1],routing_table_list[route_index]->nxt_hop_eth_addr[2],routing_table_list[route_index]->nxt_hop_eth_addr[3],routing_table_list[route_index]->nxt_hop_eth_addr[4],routing_table_list[route_index]->nxt_hop_eth_addr[5],routing_table_list[route_index]->nxt_hop_eth_addr[6],routing_table_list[route_index]->nxt_hop_eth_addr[7],routing_table_list[route_index]->nxt_hop_eth_addr[8],routing_table_list[route_index]->nxt_hop_eth_addr[9],routing_table_list[route_index]->nxt_hop_eth_addr[10],routing_table_list[route_index]->nxt_hop_eth_addr[11],2,get_vm_index(source_addr)+1,get_vm_index(dest_addr)+1);
		send_raw_packet(interface_socket_list[i]->socket,interface_socket_list[i]->interface_hw_addr,routing_table_list[route_index]->nxt_hop_eth_addr,payload);
	}
	else
	{
		print_routing_table();
		if(route_redisc_flag==1)
		{	
			//printf("if in unix_socket\n");
			//printf("Acquiring lock\n");
			pthread_mutex_lock(&route_mutex);
			//printf("Acquired lock\n");
			int route_index=get_routing_table_index(source_addr,dest_addr);
			assert(route_index>-1);
			int br_id = routing_table_list[route_index]->broadcast_id+1;
			free(routing_table_list[route_index]);
			routing_table_list[route_index]=NULL;
			//printf("unlocking\n");
			pthread_mutex_unlock(&route_mutex);
			//printf("unlocked\n");
			enter_in_routing_table(source_addr,dest_addr,"","",-1,-1,1,0,br_id,0);
			char * payload = create_rreq(0,source_addr,dest_addr,cli_port,dest_port,1,br_id,1,cli_addr.sun_path,msg);
			broadcast_rreq(payload,source_addr,dest_addr);
		}
		else
		{
			//printf("else in unix_socket\n");
			enter_in_routing_table(source_addr,dest_addr,"","",-1,-1,1,0,1,0);
			char * payload = create_rreq(0,source_addr,dest_addr,cli_port,dest_port,1,1,0,cli_addr.sun_path,msg);
			broadcast_rreq(payload,source_addr,dest_addr);
		}
		//  create_thread();
		struct generic_payload* gp = (struct generic_payload*)malloc(sizeof(struct generic_payload));
		gp->payload_type = 2;
		gp->canonical_source_addr = (char*)malloc(16);
		strcpy(gp->canonical_source_addr,source_addr);
		gp->canonical_dest_addr = (char*)malloc(16);
		strcpy(gp->canonical_dest_addr,dest_addr);
		gp->source_port=cli_port;
		gp->dest_port=dest_port;
		gp->hop_count=0;//fill from routing entry
		gp->broadcast_id=0;//fill from routing entry
		gp->forced_discovery=0;
		gp->rrep_already_sent=0;
		gp->msg_reply=0;
		gp->payload_msg=(char*)malloc(255);
		strcpy(gp->payload_msg,msg);
		gp->client_path=(char*)malloc(128);
		strcpy(gp->client_path,cli_addr.sun_path);

		pthread_t tid;
		pthread_create(&tid,NULL,&thread_send_msg,gp);
		print_routing_table();
	}
	//printf("exiting unix socket set\n");
}

