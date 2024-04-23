
#define MAX_CLIENTS 256

struct server_client_t {
    sockaddr_in addr;
    i64 connection_time;
    int ping;
};

struct server_t {
    sockaddr_in clients[MAX_CLIENTS];
    u32 client_count=0;
    int socket=-1;
    double time;
    double last_tick_time;
    int last_tick;

    server_client_t s_clients[MAX_CLIENTS];

    // commands from previous couple of seconds
    // gonna need an interesting data structure for this
    netstate_info_t NetState;
    server_header_t header;
    timer_t timer;
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

DWORD WINAPI ServerListen(LPVOID lpParamater) {
    const int &connect_socket = gl_server.socket;
    sockaddr_in clientaddr;
    
    while (1) {
        packet_t p = {};
        int ret = recieve_packet(connect_socket,&clientaddr,&p);

        if (ret < 0) {
            printf("ERROR: recvfrom() failed\n");
            printf("%d",WSAGetLastError());
            return 0;
        }

        if (p.type == MESSAGE) {
            printf("%s\n",p.data.msg);
        } else if (p.type == CONNECTION_REQUEST) {
            in_addr client_ip_addr;
            memcpy(&client_ip_addr, &clientaddr.sin_addr.s_addr, 4);
            printf("IP address of client = %s  port = %d) \n", inet_ntoa(client_ip_addr),
                   ntohs(clientaddr.sin_port));
            packet_t res = {};
            res.type = CONNECTION_ACCEPTED;
            res.data.connection_dump.id = gl_server.client_count;
            res.data.connection_dump.server_time = gl_server.timer.get();
            res.data.connection_dump.server_header = gl_server.header;
            res.data.connection_dump.time_of_last_tick = gl_server.last_tick_time;
            res.data.connection_dump.last_tick = gl_server.last_tick;
            
            send_packet(connect_socket,&clientaddr,&res);

            packet_t gs_full_info = {};
            gs_full_info.type = GS_FULL_INFO;
            gs_full_info.data.snapshot = gl_server.NetState.snapshots.back();
            
            send_packet(connect_socket,&clientaddr,&gs_full_info);

            gl_server.s_clients[gl_server.client_count].addr = clientaddr;
            gl_server.clients[gl_server.client_count++] = clientaddr;

            add_player();
            gs.players[gs.player_count-1].id = gs.player_count-1;
            
            packet_t player_join = {};
            player_join.type = ADD_PLAYER;
            player_join.data.new_player = gs.players[gs.player_count-1];
            broadcast(&player_join);

        } else if (p.type == PING) {
            packet_t p = {};
            p.type = PING;
            send_packet(connect_socket,&clientaddr,&p);

            /*
            // ping after first connect
            server_client_t &cl = gl_server.s_clients[gl_server.client_count];
            cl.connection_time = p.data.ping_dump.server_time_on_accept;
            cl.ping = SDL_GetTicks() - cl.connection_time;
            */
            // realistically should send the ping back..?
        } else if (p.type == COMMAND_DATA) {
            if (p.data.command_data.count==0) {
                continue;
            }

            // disregard any command more than 20 ticks old
            if (p.data.command_data.end_time < gl_server.last_tick - 20) {
                continue;
            }
            
            for (i32 n=0;n<p.data.command_data.count;n++) {
                command_t &cmd = p.data.command_data.commands[n];
                if (cmd.tick < gl_server.last_tick - 20) {
                    continue;
                }
                
                if (std::find(gl_server.NetState.command_stack.begin(),gl_server.NetState.command_stack.end(),cmd)==gl_server.NetState.command_stack.end()) {
                    if (std::find(gl_server.NetState.command_buffer.begin(),gl_server.NetState.command_buffer.end(),cmd)==gl_server.NetState.command_buffer.end()) {
                        gl_server.NetState.command_buffer.push_back(cmd);
                    }
                }
                
                //gl_server.NetState.command_buffer.push_back(cmd);
            }
            
            std::vector<command_t> &buf=gl_server.NetState.command_buffer;
            
            // rewind to first command there and update up
            if (buf.size() > 1) {
                std::sort(buf.begin(),buf.end(),command_sort_by_time());
            }
            command_t first_cmd = gl_server.NetState.command_buffer[0];
            if (first_cmd.sending_id == ID_DONT_EXIST) {
                printf("FUCK\n");
                // bad news this just got ran!!!!
            }
            gl_server.NetState.command_stack.insert(gl_server.NetState.command_stack.end(),buf.begin(),buf.end());
            std::sort(gl_server.NetState.command_stack.begin(),gl_server.NetState.command_stack.end(),command_sort_by_time());
            gl_server.NetState.command_buffer.clear();

            // dont rewind state it doesnt run
            // update to past (present - 2 seconds for example) and it runs fine but out of sync
            if (first_cmd.tick < gs.tick) {
                //int curr=gs.tick;
                find_and_load_gamestate_snapshot(gs,gl_server.NetState,first_cmd.tick);
                //load_game_state_up_to_tick(gs,gl_server.NetState,curr);
            }
        }
    }
    return 0;
}

static void server() {
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
    servaddr.sin_port = htons(DEFAULT_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind
    int ret = bind(connect_socket, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    srand((u32)time(NULL));

    gl_server.timer.Start();

    socklen_t addr_len = sizeof(clientaddr);
    for (i32 ind=0; ind<16; ind++) {
        gl_server.NetState.do_charas_interp[ind]=false;
    }
    

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

    bool running=true;

    add_wall({5,3});
    add_wall({6,3});
    add_wall({7,3});
    add_wall({8,3});
    add_wall({5,5});
    add_wall({6,7});

    r_clock_t tick_clock(gl_server.timer);
    r_clock_t snap_clock(gl_server.timer);

    int input_polling_send_hz=30;
    double tick_hz=60;
    double snap_hz=60;
    int frame_hz=0; // uncapped framerate
    double tick_delta=(1.0/tick_hz);
    double snap_delta=(1.0/snap_hz);

    gl_server.last_tick=0;
    gl_server.last_tick_time=gl_server.timer.get();
    gl_server.NetState.authoritative=true;

    // TODO: figure out input bufffer/whether the server should update and send information with
    // a built in latency or not
    // e.g. when sending a snapshot do they send the latest or the one from 2 snapshots ago, so it
    // never sends an incorrect snapshots?
    int snapshot_buffer=0;
    int target_tick=0;
    
    // tick thread
    while (running) {
        PollEvents(&input,&running);
        while(tick_clock.getElapsedTime() > tick_delta) {
            target_tick++;
            tick_clock.start_time += tick_delta;
            gl_server.last_tick_time+=tick_delta;
        }
        gl_server.last_tick=target_tick;

        if (snap_clock.getElapsedTime() > snap_delta) {
            snap_clock.start_time += snap_delta;
            //snap_clock.Restart();
            load_game_state_up_to_tick(gs,gl_server.NetState,target_tick);
            
            gl_server.NetState.add_snapshot(gs);

            if (gl_server.NetState.snapshots.size() > snapshot_buffer) {
                packet_t p = {};
                p.type = SNAPSHOT_DATA;
                p.data.snapshot = gl_server.NetState.snapshots[gl_server.NetState.snapshots.size()-snapshot_buffer-1];

                printf("Sending snapshots for %d\n",p.data.snapshot.tick);
                broadcast(&p);
            }
            /*
            // broadcast round end if the final death took place > 1 second ago
            if (gs.round_end_tick >= 0 && gs.round_end_tick < target_tick-60) {
                packet_t p = {};
                p.type = END_ROUND;
                broadcast(&p);
            }
            */
        }

        render_game_state(sdl_renderer);
        SDL_RenderPresent(sdl_renderer);
        
        // sleep up to next snapshot
        {
            double curr = snap_clock.getElapsedTime();

            if (curr < snap_delta) {
                Sleep((DWORD)((snap_delta - curr)*1000.0));
            }
        }

    }
    closesocket(gl_server.socket);
}
