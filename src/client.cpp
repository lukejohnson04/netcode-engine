

struct client_t {
    client_t() : sync_clock(sync_timer) {}
    u32 client_id=ID_DONT_EXIST;
    int socket;
    sockaddr_in servaddr;

    double server_time_on_connection;
    double local_time_on_connection;
    double time_of_last_tick_on_connection;
    double ping=0;

    // sync_clock getElapsedTime returns the exact time since the SERVER started
    timer_t sync_timer;
    r_clock_t sync_clock;
    int last_tick;

    netstate_info_t NetState;

    struct {
        entity_id player_id=ID_DONT_EXIST;
    } game_data;

    bool init_ok=false;
    // delay
    server_header_t server_header;

    int get_exact_current_server_tick() {
        // time since we connected
        double elapsed = sync_clock.getElapsedTime();
        int ticks_elapsed = (int)floor(elapsed / (1.0/60.0));
        return ticks_elapsed;
    }
};


global_variable client_t client_st={};

//static inline int get_current_server_time(client_t cl) {
//    return cl.server_time_on_connection + ((i64)SDL_GetTicks64() - cl.local_time_on_connection);
//}

static inline double client_time_to_server_time(client_t cl) {
    return cl.server_time_on_connection + cl.sync_timer.get() - cl.ping;
}

static void add_command_to_recent_commands(command_t cmd) {
    client_st.NetState.command_buffer.push_back(cmd);
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
            gs.players[gs.player_count++] = pc.data.new_player;
            if (pc.data.new_player.id == client_st.client_id) {
                client_st.game_data.player_id = client_st.client_id;
            } else {
                client_st.NetState.do_charas_interp[pc.data.new_player.id] = true;
            }

        } else if (pc.type == GS_FULL_INFO) {
            printf("changing from %d players to %d players\n", gs.player_count, pc.data.full_info_dump.p_count);
            gs = pc.data.snapshot;
            
            if (gs.player_count-1 >= (i32)client_st.client_id) {
                client_st.game_data.player_id = (i32)client_st.client_id;
            }
            
        } else if (pc.type == SNAPSHOT_DATA) {
            client_st.NetState.snapshots.push_back(pc.data.snapshot);
            //if (client_st.NetState.snapshots.front().tick < gs.tick) {
            int curr_tick=gs.tick;
            if (pc.data.snapshot.tick<gs.tick) {
                gs = pc.data.snapshot;
                // this wont work for client because we only have commands for the local player
                
                // load_game_state_up_to_tick(gs,client_st.NetState,curr_tick,false);
            }
        } else if (pc.type == COMMAND_DATA) {
            continue;
            std::vector<command_t> new_commands;
            for (i32 ind=0; ind<pc.data.command_data.count; ind++) {
                command_t &cmd=pc.data.command_data.commands[ind];
                // remove duplicates
                if (find(client_st.NetState.command_stack.begin(),client_st.NetState.command_stack.end(),cmd) == client_st.NetState.command_stack.end()) {
                    new_commands.push_back(cmd);
                    if (cmd.sending_id != client_st.client_id) {
                        printf("New command: %d %c%d\n",cmd.sending_id,cmd.press?'+':'-',cmd.code);
                    }
                }
            }

            if (new_commands.size() != 0) {
                std::sort(new_commands.begin(),new_commands.end(),command_sort_by_time());
                if (new_commands[0].time < gs.time) {
                    rewind_game_state(gs,client_st.NetState,new_commands[0].time);
                }
                // merge
                client_st.NetState.command_stack.insert(client_st.NetState.command_stack.end(),new_commands.begin(),new_commands.end());
                std::sort(client_st.NetState.command_stack.begin(),client_st.NetState.command_stack.end(),command_sort_by_time());
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

    // connection request
    packet_t p = {};
    p.type = CONNECTION_REQUEST;

    client_st.sync_timer.Start();
    client_st.sync_clock.start_time = client_st.sync_timer.get();
    int ret = send_packet(connect_socket,&servaddr,&p);

    if (ret < 0) {
        printf("ERROR: sendto failed\n");
        printf("%d",WSAGetLastError());
        return;
    }
    
    p = {};
    ret = recieve_packet(connect_socket,&servaddr,&p);
    if (p.type == CONNECTION_ACCEPTED) {
        printf("Connection accepted\n");
        client_st.client_id = p.data.connection_dump.id;
        printf("Client id is %d\n",client_st.client_id);
        client_st.local_time_on_connection = client_st.sync_timer.get();
        client_st.server_time_on_connection = p.data.connection_dump.server_time;
        // subtract half of ping from this!
        client_st.ping = client_st.local_time_on_connection / 2.0;
        client_st.server_header = p.data.connection_dump.server_header;
        client_st.time_of_last_tick_on_connection=p.data.connection_dump.time_of_last_tick;
        client_st.NetState.do_charas_interp[client_st.client_id]=false;
        client_st.last_tick=p.data.connection_dump.last_tick;
        client_st.sync_clock.start_time -= client_st.server_time_on_connection+client_st.ping;
        
        //packet_t res = {};
        //res.type = PING;
        //res.data.ping_dump.server_time_on_accept = client_st.connection_time_server;
        //send_packet(connect_socket,&servaddr,&res);

        //printf("Server time on connection: %lld\n", client_st.server_time_on_connection);
        //printf("Server time currently: %d", client_st.connection_server + (SDL_GetTicks() - client_st.connection_time_client));
    } else if (p.type == CONNECTION_DENIED) {
        printf("Connection denied\n");
        return;
    } else {
        printf("Undefined packet type recieved of type %d\n",p.type);
        return;
    }

    TCHAR szNewTitle[MAX_PATH];
    StringCchPrintf(szNewTitle, MAX_PATH, TEXT("Client %d"), client_st.client_id);
    if( !SetConsoleTitle(szNewTitle) ) {
        _tprintf(TEXT("SetConsoleTitle failed (%d)\n"), GetLastError());
        return;
    } else {
        _tprintf(TEXT("SetConsoleTitle succeeded.\n"));
    }

    client_st.servaddr = servaddr;
    client_st.socket = connect_socket;

    DWORD ThreadID=0;
    HANDLE thread_handle = CreateThread(0, 0, &ClientListen, (LPVOID)&connect_socket, 0, &ThreadID);
    GameGUIStart();
}

static void client_send_input() {
    // really we should be sending the last second or so of commands
    // to combat packet loss, but we won't worry about that for now
    // because it gives us a massive headache removing duplicates
    if (client_st.NetState.command_buffer.empty()) {
        return;
    }
    packet_t p = {};
    p.type = COMMAND_DATA;
    p.data.command_data = {};
    
    p.data.command_data.count = 0;

    for (i32 ind=0; ind<client_st.NetState.command_buffer.size(); ind++) {
        command_t cmd = client_st.NetState.command_buffer[ind];
        // reverse the interpolation delay on the command
        p.data.command_data.commands[p.data.command_data.count++] = cmd;
    }
    p.data.command_data.start_time = client_st.NetState.command_buffer[0].tick;
    p.data.command_data.end_time = client_st.NetState.command_buffer.back().tick;

    client_st.NetState.command_stack.insert(client_st.NetState.command_stack.end(),client_st.NetState.command_buffer.begin(),client_st.NetState.command_buffer.end());
    std::sort(client_st.NetState.command_stack.begin(),client_st.NetState.command_stack.end(),command_sort_by_time());

    client_st.NetState.command_buffer.clear();
    
    send_packet(client_st.socket,&client_st.servaddr,&p);
}
