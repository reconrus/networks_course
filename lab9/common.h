typedef struct _test_struct{
    
    char name[128];
    char group[10];
    char age[2];
} test_struct_t;


typedef struct _result_struct{

    char info[200]; 

} result_struct_t;

typedef struct _node_info{
    struct sockaddr_in node_addr;
    int addr_len;
    int comm_socket_fd;
    int t_number; //da, eto kostyl. mne ne hochetsya oborachivat' v escho odin struct. but it works! 
} node_info_t;

typedef struct _file_info{
    int size; 
    int count;
} file_info;

