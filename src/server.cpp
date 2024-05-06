

#define MAX_CLIENTS 256

struct server_client_t {
    sockaddr_in addr;
    i64 connection_time;
    int ping;
    i32 last_processed_command=0;
};

struct server_t {
    sockaddr_in clients[MAX_CLIENTS];
    u32 client_count=0;
    int socket=-1;
    int last_tick;

    server_client_t s_clients[MAX_CLIENTS];

    // commands from previous couple of seconds
    // gonna need an interesting data structure for this
    netstate_info_t NetState;
    server_header_t header;
    timer_t timer;

    std::vector<command_t> merge_commands;

    overall_game_manager &gms=NetState.gms;

    void start_game();

    int get_target_tick() {
        double elapsed = static_cast<double>(timer.get_high_res_elapsed().QuadPart - gms.game_start_time.QuadPart)/timer.frequency.QuadPart;
        int ticks_elapsed = (int)floor(elapsed / (1.0/60.0));
        return ticks_elapsed;
    }

    // mutex info
    HANDLE _mt_merge_commands;
    // buffer so we don't need a second mutex
    i32 last_proc_command_buffer[MAX_CLIENTS]={0};
};

global_variable server_t gl_server;

internal void broadcast(packet_t *p) {
    for (u32 client_num=0; client_num<gl_server.client_count; client_num++) {
        send_packet(gl_server.socket,&gl_server.clients[client_num],p);
    }
}

//TODO: Everything works except the server displays things in the past. And we don't want that because
// its annoying. So should probably fix it.
// The client is recording inputs based on the gamestate ticks, and the functions to update
// the gamestate have not yet been updated to the newer versions
// Might be optimal to switch to a target tick for the client as well

int last_sent_snapshot_tick=0;

DWORD WINAPI ServerListen(LPVOID lpParamater) {
    const int &connect_socket = gl_server.socket;
    sockaddr_in clientaddr;
    
    while (1) {
        packet_t p = {};
        volatile int ret = recieve_packet(connect_socket,&clientaddr,&p);

        if (ret < 0) {
            printf("ERROR: recvfrom() failed\n");
            printf("%d",WSAGetLastError());
            return 0;
        }

        if (p.type == CONNECTION_REQUEST) {
            volatile LARGE_INTEGER t_1=gl_server.timer.get_high_res_elapsed();
            
            in_addr client_ip_addr;
            memcpy(&client_ip_addr, &clientaddr.sin_addr.s_addr, 4);
            printf("IP address of client = %s  port = %d) \n", inet_ntoa(client_ip_addr),
                   ntohs(clientaddr.sin_port));
            packet_t res = {};
            res.type = CONNECTION_ACCEPTED;
            res.data.connection_dump.id = gl_server.client_count;

            volatile LARGE_INTEGER t_2 = gl_server.timer.get_high_res_elapsed();
            printf("Request recieved");

            res.data.connection_dump.t_1 = *(LARGE_INTEGER*) &t_1;
            res.data.connection_dump.t_2 = *(LARGE_INTEGER*) &t_2;
            
            send_packet(connect_socket,&clientaddr,&res);

            gl_server.s_clients[gl_server.client_count].addr = clientaddr;
            gl_server.clients[gl_server.client_count++] = clientaddr;

            gl_server.gms.connected_players++;

            if (gl_server.gms.state == GMS::GAME_HASNT_STARTED) {
                gl_server.gms.state = PREGAME_SCREEN;
                printf("set to pregame screen\n");
            }

            packet_t initial_info_on_connection = {};
            initial_info_on_connection.type = INFO_DUMP_ON_CONNECTED;
            initial_info_on_connection.data.info_dump_on_connected.client_count = gl_server.gms.connected_players;
            send_packet(connect_socket,&clientaddr,&initial_info_on_connection);
            
            packet_t player_join = {};
            player_join.type = NEW_CLIENT_CONNECTED;
            
            // send to all clients except the one that just joined
            for (u32 client_num=0; client_num<gl_server.client_count-1; client_num++) {
                send_packet(gl_server.socket,&gl_server.clients[client_num],&player_join);
            }

        } else if (p.type == PING) {
            packet_t res = {};
            res.type = PING;
            send_packet(connect_socket,&clientaddr,&res);
            
        } else if (p.type == COMMAND_DATA) {
            WaitForSingleObject(gl_server._mt_merge_commands,INFINITE);
            entity_id client_id = ID_DONT_EXIST;
            for (i32 ind=0;ind<(i32)gl_server.client_count;ind++) {
                sockaddr_in _cl_to_comp = gl_server.clients[ind];
                
                if ((_cl_to_comp.sin_addr.s_addr == clientaddr.sin_addr.s_addr) && (_cl_to_comp.sin_port == clientaddr.sin_port)) {
                    client_id = ind;
                    break;
                }
            }
            assert(client_id != ID_DONT_EXIST);
    
            // ought to mutex this :/
            // merge commands
            for (i32 ind=0; ind<p.data.command_data.count; ind++) {
                command_t cmd = p.data.command_data.commands[ind];
                gl_server.merge_commands.push_back(cmd);
            }
            gl_server.last_proc_command_buffer[client_id] = MAX(gl_server.last_proc_command_buffer[client_id],MAX(gl_server.s_clients[client_id].last_processed_command,p.data.command_data.end_tick));
            
            ReleaseMutex(gl_server._mt_merge_commands);
        }
    }
    return 0;
}



static void server(int port) {
    TCHAR szNewTitle[MAX_PATH];
    StringCchPrintf(szNewTitle, MAX_PATH, TEXT("Server"));
    if( !SetConsoleTitle(szNewTitle) ) {
        _tprintf(TEXT("SetConsoleTitle failed (%d)\n"), GetLastError());
        return;
    } else {
        _tprintf(TEXT("SetConsoleTitle succeeded.\n"));
    }


    if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf( "SDL could not initialize! SDL Error: %s\r\n", SDL_GetError() );
        return;
    }

    // SOCK_STREAM is TCP
    // SOCK_DGRAM is UDP
    int &connect_socket = gl_server.socket;
    connect_socket = (int)socket(AF_INET, SOCK_DGRAM, 0);
    
    if (connect_socket == INVALID_SOCKET) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // fill out info
    sockaddr_in servaddr, clientaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind
    int ret = bind(connect_socket, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Started server on port %d\n",port);

    srand((u32)time(NULL));

    gl_server.timer.Start();

    socklen_t addr_len = sizeof(clientaddr);
    for (i32 ind=0; ind<16; ind++) {
        gl_server.NetState.do_charas_interp[ind]=false;
    }


    gl_server._mt_merge_commands = CreateMutex(NULL,FALSE,NULL);

    DWORD ThreadID=0;
    HANDLE thread_handle = CreateThread(0, 0, &ServerListen, (LPVOID)&connect_socket, 0, &ThreadID);

    SDL_Window *window=nullptr;
    SDL_Surface *screenSurface = nullptr;

    if (TTF_Init() != 0) {
        printf("Error: %s\n", TTF_GetError()); 
        return;
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        printf("Error: %s\n", IMG_GetError());
        return;
    }

    window = SDL_CreateWindow("Main",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              1280,
                              720,
                              SDL_WINDOW_SHOWN);

    if (window == nullptr) {
        printf("Window could not be created. SDL Error: %s\n", SDL_GetError());
    }

    SDL_Renderer* sdl_renderer;

    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (sdl_renderer == nullptr) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    screenSurface = SDL_GetWindowSurface(window);
    init_textures(sdl_renderer);
    m5x7 = TTF_OpenFont("res/m5x7.ttf",16);

    bool running=true;

    timer_t snap_clock;
    snap_clock.Start();

    double tick_hz=60;
    double snap_hz=60;
    double tick_delta=(1.0/tick_hz);
    double snap_delta=(1.0/snap_hz);

    gl_server.last_tick=0;
    gl_server.NetState.authoritative=true;
    gl_server.gms.state = GAME_HASNT_STARTED;

    // TODO: figure out input bufffer/whether the server should update and send information with
    // a built in latency or not
    // e.g. when sending a snapshot do they send the latest or the one from 2 snapshots ago, so it
    // never sends an incorrect snapshots?
    
    // base the snapshot buffer off of the ping of the players
    // make the snapshot buffer greater if some of the players have very high ping
    // set to 7 over wan
    // can be super low on lan
    int snapshot_buffer=2;
    int target_tick=0;
    // interp delay on clients should always be below the snapshot buffer on the server

    // TODO:
    // lower snapshot rate significantly
    
    // tick thread
    while (running) {
        PollEvents(&input,&running);
        bool new_frame_ready=false;

        if (gl_server.gms.state == GMS::ROUND_PLAYING) {
            target_tick = gl_server.get_target_tick();
            gl_server.last_tick=target_tick;

            if (snap_clock.get() > snap_delta) {
                snap_clock.Restart();
                // merge command mutex
                DWORD do_merge_commands = WaitForSingleObject(gl_server._mt_merge_commands,0);
                if (do_merge_commands == WAIT_OBJECT_0) {
                    for (i32 ind=0;ind<(i32)gl_server.client_count;ind++) {
                        gl_server.s_clients[ind].last_processed_command = gl_server.last_proc_command_buffer[ind];
                    }
                    if (gl_server.merge_commands.size()==0) {
                        goto command_merge_finish;
                    }
                    // merge newly received commands, but don't overwrite duplicates
                    i32 earliest_new_cmd_tick=-1;
                    for (auto new_cmd=gl_server.merge_commands.begin();new_cmd<gl_server.merge_commands.end();new_cmd++) {
                        // if the command is already in the stack discard it
                        bool cont=false;
                        for (i32 ind=0; ind<gl_server.NetState.command_stack.size(); ind++) {
                            command_node old_command = gl_server.NetState.command_stack[ind];
                            if (old_command.cmd == *new_cmd) {
                                cont = true;
                                break;
                            }
                        }
                        if (cont) continue;
                        command_node node = {*new_cmd,true};
                        gl_server.NetState.command_stack.push_back(node);
                        if (earliest_new_cmd_tick == -1) {
                            earliest_new_cmd_tick = new_cmd->tick;
                        } else {
                            earliest_new_cmd_tick = MIN(new_cmd->tick,earliest_new_cmd_tick);
                        }
                    }
                    gl_server.merge_commands.clear();
                    
            command_merge_finish:
                    ReleaseMutex(gl_server._mt_merge_commands);
                }
                
                load_game_state_up_to_tick(gs,gl_server.NetState,target_tick-snapshot_buffer);

                if (target_tick-snapshot_buffer > 0) {
                    // NOTE: add snapshot
                    packet_t p = {};
                    p.type = SNAPSHOT_TRANSIENT_DATA;
                    p.data.snapshot.gms = gs;
                    // last processed command data
                    for (i32 ind=0;ind<(i32)gl_server.client_count;ind++) {
                        p.data.snapshot.last_processed_command_tick[ind] = gl_server.s_clients[ind].last_processed_command;
                    }

                    broadcast(&p);
                    
                    // send the last 6 ticks worth of snapshots
                    // that way the server doesn't need to buffer any snapshots, and the client just overwrites
                    // the latest snapshots they have with the one from the server

                    if (gl_server.NetState.snapshots.size() > snapshot_buffer) {

                        // send important command data
                        p = {};
                        p.type = COMMAND_CALLBACK_INFO;
                        p.data.command_callback_info.count = 0;
                        for(i32 ind=0;ind<(i32)sound_queue.size();ind++){
                            sound_event event = sound_queue[ind];
                            if (event.tick<(target_tick-snapshot_buffer)-60) {
                                sound_queue.erase(sound_queue.begin()+ind);
                                ind--;
                                continue;
                            }
                            p.data.command_callback_info.sounds[p.data.command_callback_info.count++] = event;
                        }
                        broadcast(&p);
                    }
                }
                new_frame_ready=true;
                render_game_state(sdl_renderer);                
            }
        } else if (gl_server.gms.state == GMS::PREGAME_SCREEN) {
            new_frame_ready=false;
            static int p_connected_last_time=0;
            if (p_connected_last_time != gl_server.gms.connected_players) {
                new_frame_ready=true;
                p_connected_last_time = gl_server.gms.connected_players;
            } 
            // if the clock runs out, start the game
            if (gl_server.gms.counting_down_to_game_start) {
                new_frame_ready=true;
                LARGE_INTEGER curr = gl_server.timer.get_high_res_elapsed();
                if (gl_server.gms.game_start_time.QuadPart - curr.QuadPart <= 0) {
                    // game_start
                    gl_server.gms.state = GMS::ROUND_PLAYING;
                    load_gamestate_for_round(gl_server.gms);

                    // is this correct..?
                    snap_clock.start_time = gl_server.gms.game_start_time;
                    
                    goto endof_frame;                    
                }
            }

            double time_to_start = 0.0;
            if (gl_server.gms.counting_down_to_game_start) {
                time_to_start = static_cast<double>(gl_server.gms.game_start_time.QuadPart - gl_server.timer.get_high_res_elapsed().QuadPart)/gl_server.timer.frequency.QuadPart;
            }

            if (new_frame_ready) {
                render_pregame_screen(sdl_renderer,gl_server.gms,time_to_start);
            }

            if (!gl_server.gms.counting_down_to_game_start) {
                SDL_Rect dest = {1280/2-(236/2),460,236,36};
                v2i mpos = get_mouse_position();
                if (new_frame_ready) {
                    SDL_RenderCopy(sdl_renderer,textures[TexType::STARTGAME_BUTTON_TEXTURE],NULL,&dest);
                }
                if (input.mouse_just_pressed && rect_contains_point(*(iRect*)&dest,mpos)) {
                    gl_server.gms.game_start_time.QuadPart = gl_server.timer.get_high_res_elapsed().QuadPart + (3 * gl_server.timer.frequency.QuadPart);

                    packet_t p = {};
                    p.type = GAME_START_ANNOUNCEMENT;
                    p.data.game_start_time = gl_server.gms.game_start_time;
                    broadcast(&p);
                    gl_server.gms.counting_down_to_game_start=true;
                }
            }
            
        } else if (gl_server.gms.state == GMS::GAME_HASNT_STARTED) {
        }
        //printf("%f\n",gl_server.timer.get());
endof_frame:
        if (new_frame_ready) {
            SDL_RenderPresent(sdl_renderer);
        }
        
        // sleep up to next snapshot
        // this is based on snapshots during gameplay but ticks during the menu
        // perhaps a uniform system that works regardless of gamestate is best
        // when calculating current frame times and how long to sleep
        if (gl_server.gms.state == GMS::ROUND_PLAYING) {
            // sleep to next tick
            double len = snap_delta - snap_clock.get() - (snap_delta / 10.0);
            if (len > 0) {
                SDL_Delay(u32(len*1000.0));
            }
        }

    }
    closesocket(gl_server.socket);
}
