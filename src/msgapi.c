#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/un.h>
#include<unistd.h>
#include<errno.h>
 
int msg_send(int sockfd,char* dest_addr,int dest_port,char* msg,int flag)
{
 
    int write_count = 0;
    write_count = strlen(dest_addr)+1;
    if(write_count != write(sockfd,(const void*)dest_addr,write_count))
    {
        printf("Message: Error could not write dest_addr: %s\n",strerror(errno));
        exit(1);
    }
    write_count = sizeof(int);
    if(write_count != write(sockfd,(const void*)&dest_port,write_count))
    {
        printf("Message: Error could not write dest_port: %s\n",strerror(errno));
        exit(1);
    }
    write_count = strlen(msg)+1;
    if(write_count != write(sockfd,(const void*)msg,write_count))
    {
        printf("Message: Error could not write msg: %s\n",strerror(errno));
        exit(1);
    }
    write_count = sizeof(int);
    if(write_count != write(sockfd,(const void*)&flag,write_count))
    {
        printf("Message: Error could not write flag: %s\n",strerror(errno));
        exit(1);
    }
    return 0;
}
 
int msg_recv(int sockfd,char** msg,char** src_ip, int** src_port)
{
    /*Allocating buffer of 64 as msg should not be more than 10 characters
      as it is just timestamp
     */
    int rc=0;
    *msg = (char*)malloc(64);
    memset(*msg,'\0',64);
 
    if((rc = read(sockfd,*msg,64))>0)
    {
        *msg = (char*) realloc(*msg,strlen(*msg)+1);
	//printf("reading msg successful in msgapi : %s\n",*msg);
    }
    else
    {
        printf("Message: Error could not read msg: %s\n",strerror(errno));
        exit(1);
    }
 
    *src_ip = (char*) malloc(20);
    memset(*src_ip,'\0',20);
    if((rc = read(sockfd,*src_ip,20))>0)
    {
        *src_ip = (char*) realloc(*src_ip,strlen(*src_ip)+1);
	//printf("reading src_ip successful in msgapi:%s\n",*src_ip);
    }
    else
    {
        printf("Message: Error could not read src ip: %s\n",strerror(errno));
        exit(1);
    }
 
    char * src_port_buf = (char*) malloc(8);
    memset(src_port_buf,'\0',8);
    if((rc = read(sockfd,src_port_buf,8))>0)
    {
        src_port_buf = (char*) realloc(src_port_buf,strlen(src_port_buf)+1);
	//printf("reading source_port successful in msgapi :%s\n",src_port_buf);
    }
    else
    {
        printf("Message: Error could not read src ip: %s\n",strerror(errno));
        exit(1);
    }
    *src_port = (int*)malloc(sizeof(int));
    //printf("%s\n",src_port_buf);
    sscanf(src_port_buf,"%d",*src_port);
	//printf("src_port in msgapi: %d\n",*src_port);
    return 0;
}
