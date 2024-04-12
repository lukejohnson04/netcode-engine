
struct client_t {
    u32 client_id=ID_DONT_EXIST;
    int socket;
    sockaddr_in servaddr;

    // server SDL tick time when they received our connection
    i64 server_time_on_connection;
    // local SDL tick time  when we received acceptance of our connection
    i64 local_time_on_connection;
    int ping=0;

    netstate_info_t NetState;

    struct {
        entity_id player_id=ID_DONT_EXIST;
    } game_data;

    bool init_ok=false;
    // delay
    int lerp=100;
    server_header_t server_header;
};


global_variable client_t client_st={};

static inline int get_current_server_time(client_t cl) {
    return cl.server_time_on_connection + ((i64)SDL_GetTicks64() - cl.local_time_on_connection);
}

static inline int client_time_to_server_time(i64 local_time, client_t cl) {
    return cl.server_time_on_connection + (local_time - cl.local_time_on_connection) + cl.ping;
}

static void add_command_to_recent_commands(command_t cmd) {
    client_st.NetState.command_buffer[client_st.NetState.buffer_count++] = cmd;
}


DWORD WINAPI ClientListen(LPVOID lpParamater) {
    const int connect_socket = *(int*)lpParamater;
    printf("Thread started!\n\r");
    sockaddr_in servaddr;
    printf("Started listen thread\n");

    while (1) {
        packet_t pc = {};
        int ret = recieve_packet(connect_socket,&servaddr,&pc);

        if (pc.type == MESSAGE) {
            printf("%s\n", pc.data.msg);

        } else if (pc.type == ADD_PLAYER) {
            add_player();
            if (pc.data.id == client_st.client_id) {
                client_st.game_data.player_id = gs.player_count - 1;
            }

        } else if (pc.type == GS_FULL_INFO) {
            
            while (gs.player_count < pc.data.count) {
                add_player();
            }
            

        } else if (pc.type == SNAPSHOT_DATA) {
            continue;
            // 2. interpret snapshots and rebuild gamestate everytime one comes in
            
            client_st.NetState.snapshots[client_st.NetState.snapshot_count++] = pc.data.snapshot;
            for (u32 ind=0; ind<pc.data.snapshot.player_count; ind++) {
                if (ind != client_st.client_id) {
                    gs.players[ind] = pc.data.snapshot.players[ind];
                }
            }
            

        } else if (pc.type == COMMAND_DATA) {
            continue;
            
            command_t new_commands[256];
            u32 new_command_count=0;
            for (int ind=0;ind<pc.data.command_data.count;ind++) {
                if (pc.data.command_data.commands[ind].sending_id == client_st.client_id) {
                    continue;
                }
                command_t cmd=pc.data.command_data.commands[ind];
                new_commands[new_command_count++] = cmd;
            }
            
            if (new_command_count > 0) {
                quicksort_commands(new_commands,0,new_command_count);
                // should be sorted already
                command_t first_command = new_commands[0];
                // rewind the gamestate to that first command
                rewind_game_state(gs,client_st.NetState,first_command.time);
                // 1. combine command data
                mergesort_commands(new_commands,new_command_count,client_st.NetState.command_stack,client_st.NetState.stack_count);
                update_game_state(gs,client_st.NetState,get_current_server_time(client_st));
                
            }

        } else {
            printf("wtf\n");
        }
    }
    closesocket(connect_socket);
    return 0;
}

static void GameGUIStart();
static void client_connect(int port) {
    if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf( "SDL could not initialize! SDL Error: %s\r\n", SDL_GetError() );
        return;
    }

    int connect_socket = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (connect_socket < 0) {
        printf("socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons((short) port);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    char out_buf[4096];
    char in_buf[4096];
    strcpy_s(out_buf, "Test message from CLIENT to SERVER");
    
    int n, addr_len;

    // connection request
    packet_t p = {};
    p.type = CONNECTION_REQUEST;

    i64 send_time = SDL_GetTicks64();
    int ret = send_packet(connect_socket,&servaddr,&p);

    if (ret < 0) {
        printf("ERROR: sendto failed\n");
        printf("%d",WSAGetLastError());
        return;
    }

    u64 interp_delay = 100;
    
    p = {};
    ret = recieve_packet(connect_socket,&servaddr,&p);
    if (p.type == CONNECTION_ACCEPTED) {
        printf("Connection accepted\n");
        client_st.client_id = p.data.connection_dump.id;
        client_st.server_time_on_connection = p.data.connection_dump.server_time;
        // subtract half of ping from this!
        client_st.local_time_on_connection = SDL_GetTicks64();
        client_st.ping = (client_st.local_time_on_connection - send_time) / 2;
        
        //packet_t res = {};
        //res.type = PING;
        //res.data.ping_dump.server_time_on_accept = client_st.connection_time_server;
        //send_packet(connect_socket,&servaddr,&res);

        printf("Server time on connection: %lld\n", client_st.server_time_on_connection);
        //printf("Server time currently: %d", client_st.connection_server + (SDL_GetTicks() - client_st.connection_time_client));
    } else if (p.type == CONNECTION_DENIED) {
        printf("Connection denied\n");
        return;
    } else {
        printf("Undefined packet type recieved of type %d\n",p.type);
        return;
    }

    client_st.servaddr = servaddr;
    client_st.socket = connect_socket;

    DWORD ThreadID=0;
    HANDLE thread_handle = CreateThread(0, 0, &ClientListen, (LPVOID)&connect_socket, 0, &ThreadID);
    GameGUIStart();
}

static void client_on_tick() {
    // really we should be sending the last second or so of commands
    // to combat packet loss, but we won't worry about that for now
    // because it gives us a massive headache removing duplicates
    
    packet_t p = {};
    p.type = COMMAND_DATA;
    p.data.command_data = {};
    
    p.data.command_data.count = 0;

    for (u32 ind=0; ind<client_st.NetState.buffer_count; ind++) {
        command_t &cmd = client_st.NetState.command_buffer[ind];
        p.data.command_data.commands[p.data.command_data.count++] = cmd;
    }
    client_st.NetState.buffer_count=0;

    send_packet(client_st.socket,&client_st.servaddr,&p);
}
