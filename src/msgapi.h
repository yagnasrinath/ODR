/*
This is the send/recv function to be used only by time client and time server
programs to send and recv messages to each other via ODR service
*/
int msg_send(int sockfd,char* dest_addr,int dest_port,char* msg,int flag);
int msg_recv(int sockfd,char** msg,char** src_ip, int** src_port);

