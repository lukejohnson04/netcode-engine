

struct client_t {
    u32 client_id=ID_DONT_EXIST;
    int socket;
    sockaddr_in servaddr;

    double ping=0;

    // sync_clock getElapsedTime returns the exact time since the SERVER started
    timer_t sync_timer;
    //r_clock_t sync_clock;
    //int last_tick;
    
    netstate_info_t NetState;
    std::vector<command_t> command_history;

    overall_game_manager &gms=NetState.gms;

    /*
      struct {
        entity_id player_id=ID_DONT_EXIST;
    } game_data;
    */

    bool init_ok=false;
    // delay
    server_header_t server_header;

    int get_exact_current_server_tick() {
        // subtract the time the game started at
        double elapsed = static_cast<double>(sync_timer.get_high_res_elapsed().QuadPart - gms.game_start_time.QuadPart)/sync_timer.frequency.QuadPart;
        int ticks_elapsed = (int)floor(elapsed / (1.0/60.0));
        return ticks_elapsed;
    }

    double get_time_to_next_tick() {
        int next_tick = get_exact_current_server_tick()+1;
        LARGE_INTEGER next_tick_time;
        next_tick_time.QuadPart = gms.game_start_time.QuadPart + (next_tick*sync_timer.frequency.QuadPart/60);
        return static_cast<double>(next_tick_time.QuadPart-sync_timer.get_high_res_elapsed().QuadPart)/sync_timer.frequency.QuadPart;
    }
};


global_variable client_t client_st={};

//static inline int get_current_server_time(client_t cl) {
//    return cl.server_time_on_connection + ((i64)SDL_GetTicks64() - cl.local_time_on_connection);
//}

/*
  static inline double client_time_to_server_time(client_t cl) {
    return cl.server_time_on_connection + cl.sync_timer.get() - cl.ping;
}
*/

static void add_command_to_recent_commands(command_t cmd) {
    client_st.NetState.command_buffer.push_back(cmd);
}

static void command_callback(character *player, command_t cmd) {
    if (player->id != client_st.client_id) return;
    switch (cmd.code) {
        case CMD_SHOOT:
            Mix_PlayChannel( -1, sound_effects[SfxType::FLINTLOCK_FIRE_SFX], 0 );
            break;
        case CMD_RELOAD:
            Mix_PlayChannel( -1, sound_effects[SfxType::FLINTLOCK_RELOAD_SFX], 0 );
            break;
        case CMD_INVISIBLE:
            Mix_PlayChannel( -1, sound_effects[SfxType::INVISIBILITY_SFX], 0 );
            break;
        case CMD_SHIELD:
            Mix_PlayChannel( -1, sound_effects[SfxType::SHIELD_SFX], 0 );
            break;
        default:
            return;
    }
    // do something
}

static void no_bullets_fire_effects() {
    Mix_PlayChannel( -1, sound_effects[SfxType::FLINTLOCK_NO_AMMO_FIRE_SFX], 0 );
}


int last_tick_processed_in_client_loop=0;
game_state new_snapshot;
bool load_new_snapshot=false;
std::vector<game_state> client_snapshot_buffer_stack;

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

        } else if (pc.type == NEW_CLIENT_CONNECTED) {
            client_st.gms.connected_players++;

        } else if (pc.type == ADD_PLAYER) {
            gs.players[gs.player_count++] = pc.data.new_player;
            client_st.gms.connected_players++;

        } else if (pc.type == INFO_DUMP_ON_CONNECTED) {
            client_st.gms.connected_players=pc.data.info_dump_on_connected.client_count;
            client_st.client_id = client_st.gms.connected_players-1;
            printf("%d\n",client_st.gms.connected_players);

        } else if (pc.type == GS_FULL_INFO) {
            printf("changing from %d players to %d players\n", gs.player_count, pc.data.full_info_dump.p_count);
            gs = pc.data.snapshot;
            client_st.gms.connected_players=gs.player_count;
                        
        } else if (pc.type == SNAPSHOT_DATA) {
            // mutex this
            if (load_new_snapshot && new_snapshot.tick == pc.data.snapshot.tick) {
                new_snapshot = pc.data.snapshot;
                continue;
            }
            bool found=false;
            for (i32 ind=0; ind<client_snapshot_buffer_stack.size(); ind++) {
                if (client_snapshot_buffer_stack[ind].tick==pc.data.snapshot.tick) {
                    client_snapshot_buffer_stack[ind] = pc.data.snapshot;
                    found=true;
                    break;
                }
            }
            if (found) continue;
            for (i32 ind=0; ind<client_st.NetState.snapshots.size(); ind++) {
                if (client_st.NetState.snapshots[ind].tick==pc.data.snapshot.tick) {
                    client_st.NetState.snapshots[ind] = pc.data.snapshot;
                    found=true;
                    break;
                }
            }
            if (found) continue;            
            
            client_snapshot_buffer_stack.push_back(pc.data.snapshot);

            //std::sort(client_st.NetState.snapshots.begin(),client_st.NetState.snapshots.end(),[](game_state &left, game_state &right) {return left.tick < right.tick;});
            int server_tick = client_st.get_exact_current_server_tick();
            
            if (pc.data.snapshot.tick <= last_tick_processed_in_client_loop) {
                // should we also check if the snapshot is greater than the current gs tick?
                // because that way if we receive two snapshots between client ticks we would
                // load the second snapshot
                //gs = pc.data.snapshot;
                load_new_snapshot=true;
                new_snapshot = pc.data.snapshot;

                // dont do this update loop because we could skip past the immediate next tick that
                // we otherwise would have updated on in the main update loop
                
                

            } else {
                printf("Received snap %d at gs tick %d on server tick %d\n",pc.data.snapshot.tick,gs.tick,server_tick);
            }

        } else if (pc.type == GAME_START_ANNOUNCEMENT) {
            client_st.gms.game_start_time = pc.data.game_start_time;
            client_st.gms.counting_down_to_game_start=true;

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
static void client_connect(int port,std::string ip_addr) {
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
    servaddr.sin_addr.s_addr = inet_addr(ip_addr.c_str());

    // connection request
    packet_t p = {};
    p.type = CONNECTION_REQUEST;


    client_st.sync_timer.Start();
    volatile LARGE_INTEGER t_0=client_st.sync_timer.get_high_res_elapsed();

    volatile int ret = send_packet(connect_socket,&servaddr,&p);

    // default interpolation delay - 100ms with default tickrate of 60

    
    p = {};
    ret = recieve_packet(connect_socket,&servaddr,&p);

    volatile LARGE_INTEGER t_3=client_st.sync_timer.get_high_res_elapsed();

    if (p.type == CONNECTION_ACCEPTED) {
        //client_st.local_time_on_connection = client_st.sync_timer.get();
        LARGE_INTEGER t_1 = p.data.connection_dump.t_1;
        LARGE_INTEGER t_2 = p.data.connection_dump.t_2;
        LARGE_INTEGER time_offset;
        time_offset.QuadPart = ((t_1.QuadPart - t_0.QuadPart) + (t_2.QuadPart - t_3.QuadPart)) / 2;
        LARGE_INTEGER ping;
        ping.QuadPart = (t_3.QuadPart - t_0.QuadPart) - (t_2.QuadPart - t_1.QuadPart);

        client_st.ping = static_cast<double>(ping.QuadPart) / client_st.sync_timer.frequency.QuadPart;
        client_st.ping *= 1000;
        LARGE_INTEGER curr_server_time;
        curr_server_time.QuadPart = t_2.QuadPart + ping.QuadPart/2;
        // dest start_time is whatever makes it so t_3 + start_time = curr_server_time
        
        client_st.sync_timer.start_time.QuadPart -= curr_server_time.QuadPart - t_3.QuadPart;

        client_st.client_id = p.data.connection_dump.id;
        client_st.server_header = p.data.connection_dump.server_header;

        // whatever is higher - 6 or 4 ticks plus the amount of ticks of ping you have
        client_st.NetState.interp_delay = MAX(6,4 + (int)ceil(((client_st.ping/1000.0)/2.0)/(1.0/60.0) * 1.75));
        printf("Interp delay of %d\n",client_st.NetState.interp_delay);

        printf("Connection accepted\n");
        printf("Client id is %d with ping %f\n",client_st.client_id,client_st.ping);
        
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

static void client_send_input(int curr_tick) {
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

    // right now its possible to send inputs from the current tick, but then add more inputs on the next
    // client tick. Then, you update it during the client tick with the extra input. This could be the
    // cause of rubber banding

    // send last x ticks worth of commands
    p.data.command_data.start_time = curr_tick-20;
    p.data.command_data.end_time = curr_tick;

    client_st.command_history.insert(client_st.command_history.end(),client_st.NetState.command_buffer.begin(),client_st.NetState.command_buffer.end());
    i32 first_elem_in_range=0;
    for (i32 ind=0; ind<client_st.command_history.size(); ind++) {
        if (client_st.command_history[ind].tick >= p.data.command_data.start_time) {
            p.data.command_data.commands[p.data.command_data.count++] = client_st.command_history[ind];
        } else {
            first_elem_in_range++;
        }
    }
    
    if (first_elem_in_range >= client_st.command_history.size()) {
        client_st.command_history.clear();
    }
    
    for (i32 ind=0; ind<client_st.command_history.size(); ind++) {
        command_t cmd = client_st.command_history[ind];
        // reverse the interpolation delay on the command
        p.data.command_data.commands[p.data.command_data.count++] = cmd;
    }

    client_st.NetState.command_stack.insert(client_st.NetState.command_stack.end(),client_st.NetState.command_buffer.begin(),client_st.NetState.command_buffer.end());
    std::sort(client_st.NetState.command_stack.begin(),client_st.NetState.command_stack.end(),command_sort_by_time());

    client_st.NetState.command_buffer.clear();
    
    send_packet(client_st.socket,&client_st.servaddr,&p);
}
