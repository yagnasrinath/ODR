#define CUSTOM_PROTO_NUM 2236
  
struct interface_socket
{
    int interface_index;
    int socket;
    char* interface_hw_addr;
};
 
struct routing_table
{
    char* source_canonical_ip_addr;
    char* dest_canonical_ip_addr;
    char* nxt_hop_eth_addr;     //fwd path
    char* received_from_eth_addr;  //filled by rreq
    int outgoing_interface_index;
    int incoming_interface_index;
    int hop_count;
    int num_hops_to_dest;
    int broadcast_id;
    struct timeval tv;
};  
 
struct port_path
{
    int port;
    char* socket_file_path;
};
 
struct generic_payload
{
    int payload_type;
    char *canonical_source_addr;
    char *canonical_dest_addr;
    int source_port;
    int dest_port;
    int hop_count;
    int broadcast_id;
    int num_bytes_msg;
    int forced_discovery;
    int rrep_already_sent;
    int msg_reply;
    char* payload_msg;
    char* client_path;
}rreq_packet;
 
extern int send_msg_flag;
extern int udomain_flag;
extern int first_time_flag;
extern int pf_packet_flag;
 
//extern struct routing_table** routing_table_list;
extern struct port_path* socket_port_path_list[25];
extern struct interface_socket * interface_socket_list[10];
int create_pf_packet_sockets();
int create_unix_socket();
void create_routing_table();
void unix_socket_read();
void unix_socket_set();
void pf_packet_function();
char* create_rreq_packet();
void broadcast_rreq();
int check_routing_table();
void create_port_path_table();
