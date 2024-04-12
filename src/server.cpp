
#define MAX_CLIENTS 256

struct server_client_t {
    sockaddr_in addr;
    int connection_time;
    int ping;
};

struct server_header_t {
    int command_sliding_window_len = 1000;
    int tickrate=40;
    int snaprate=20;
};


struct server_t {
    sockaddr_in clients[MAX_CLIENTS];
    u32 client_count=0;
    int socket=-1;
    i64 time;

    server_client_t s_clients[MAX_CLIENTS];

    // commands from previous couple of seconds
    // gonna need an interesting data structure for this
    netstate_info_t NetState;
    server_header_t header;
};

global_variable server_t gl_server;

internal void broadcast(packet_t *p) {
    for (u32 client_num=0; client_num<gl_server.client_count; client_num++) {
        send_packet(gl_server.socket,&gl_server.clients[client_num],p);
    }
}

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
            res.data.connection_dump.server_time = SDL_GetTicks();
            
            add_player();
            
            send_packet(connect_socket,&clientaddr,&res);

            packet_t gs_full_info = {};
            gs_full_info.type = GS_FULL_INFO;
            gs_full_info.data.count = gl_server.client_count;

            send_packet(connect_socket,&clientaddr,&gs_full_info);

            gl_server.s_clients[gl_server.client_count].addr = clientaddr;
            gl_server.clients[gl_server.client_count++] = clientaddr;

            packet_t player_join = {};
            player_join.type = ADD_PLAYER;
            player_join.data.id = res.data.id;
            broadcast(&player_join);

        } else if (p.type == PING) {

            // ping after first connect
            server_client_t &cl = gl_server.s_clients[gl_server.client_count];
            cl.connection_time = p.data.ping_dump.server_time_on_accept;
            cl.ping = SDL_GetTicks() - cl.connection_time;
            // realistically should send the ping back..?
        } else if (p.type == COMMAND_DATA) {
            
            int new_commands=0;
            for (i32 n=0;n<p.data.command_data.count;n++) {
                command_t &cmd = p.data.command_data.commands[n];
                gl_server.NetState.command_buffer[gl_server.NetState.buffer_count++] = cmd;
                new_commands++;
            }
            
            printf("%d new commands\n", new_commands);
        }
    }
    return 0;
}

int partition(command_t *arr, int low, int high) {
    int pivot = arr[low].time;
    int k = high;
    for (int i=high; i > low; i--) {
        if (arr[i].time > pivot) {
            command_t temp=arr[i];
            arr[i] = arr[k];
            arr[k] = temp;
            k--;
        }
    }
    command_t temp = arr[low];
    arr[low] = arr[k];
    arr[k] = temp;

    return k;
}

void quicksort_commands(command_t *arr, int low, int high) {
    if (low < high) {
        int idx = partition(arr,low,high);
        quicksort_commands(arr, low, idx-1);
        quicksort_commands(arr, idx+1, high);
    }
}

// removes duplicates
void mergesort_commands(command_t *buffer, u32 &buffer_count, command_t *stack, u32 &stack_count) {
    // inefficient? lol
    // not merging in place, definitely slow
    if (buffer_count == 0) {
        return;
    }
    
    command_t new_stack[1024];

    int i=0,j=0,k=0;
    int n1 = stack_count;
    int n2 = buffer_count;
    while (i<n1 && j<n2) {
        if (stack[i].time < buffer[j].time) {
            new_stack[k++] = stack[i++];
        } else if (stack[i] == buffer[j]) {
            i++;
            j++;
        } else {
            new_stack[k++] = buffer[j++];
        }
    }
    while (i<n1) {
        new_stack[k++] = stack[i++];
    }
    while (j<n2) {
        new_stack[k++] = buffer[j++];
    }

    stack_count += buffer_count;
    buffer_count = 0;
    memcpy(stack,new_stack,sizeof(command_t)*stack_count);
}


static void send_command_data() {
    // send last 1 second of commands
    int end_time=gl_server.time;
    int start_time=end_time-1000;

    packet_t p = {};
    p.type = COMMAND_DATA;
    p.data.command_data.start_time = start_time;
    p.data.command_data.end_time = end_time;
    p.data.command_data.count = 0;

    for (u32 ind=0; ind<gl_server.NetState.stack_count; ind++) {
        command_t &cmd = gl_server.NetState.command_stack[ind];
        if (cmd.time > end_time) {
            break;
        } else if (cmd.time > start_time) {
            p.data.command_data.commands[p.data.command_data.count++] = cmd;
        }
    }
    printf("%d\n",p.data.command_data.count);
    broadcast(&p);
}


static void send_snapshot() {
    // merge command buffer if it exists
    if (gl_server.NetState.buffer_count) {
        mergesort_commands(gl_server.NetState.command_buffer,gl_server.NetState.buffer_count,gl_server.NetState.command_stack,gl_server.NetState.stack_count);
    }
    // go through recent commands, rebuild up to present
    update_game_state(gs,gl_server.NetState,gl_server.time);
    
    packet_t p = {};
    p.type = SNAPSHOT_DATA;
    p.data.snapshot = gs;
    broadcast(&p);
}


static void process_buffer_commands(u64 server_time) {
    // disregard commands from greater than 500ms ago
    for (i32 ind=0;ind<gl_server.NetState.buffer_count;ind++) {
        if (gl_server.NetState.command_buffer[ind].time < server_time-500) {
            gl_server.NetState.command_buffer[ind] = gl_server.NetState.command_buffer[--gl_server.NetState.buffer_count];
        }
    }
    if (gl_server.NetState.buffer_count == 0) {
        return;
    }
    command_t old_first_cmd = gl_server.NetState.command_buffer[0];
    u32 old_size=gl_server.NetState.buffer_count;
    // rewind to first command there and update up
    if (gl_server.NetState.buffer_count > 1) {
        quicksort_commands(gl_server.NetState.command_buffer,0,gl_server.NetState.buffer_count);
    }
    command_t first_cmd = gl_server.NetState.command_buffer[0];
    if (first_cmd.sending_id == ID_DONT_EXIST) {
        if (old_first_cmd.sending_id != first_cmd.sending_id) {
            printf("MEGA FUCK\n");
            if (old_size != gl_server.NetState.buffer_count) {
                printf("TRIPLE FUCK\n");
            }
        }
        printf("FUCK\n");
        // bad news this just got ran!!!!
    }

    // dont rewind state it doesnt run
    // update to past (present - 2 seconds for example) and it runs fine but out of sync
    u64 present=gs.time;
    if (first_cmd.time < server_time) {
        printf("Rewinding %lldms\n",server_time-first_cmd.time);
        rewind_game_state(gs,gl_server.NetState,first_cmd.time);
    } else {
        printf("oh fuck\n");
    }
    mergesort_commands(gl_server.NetState.command_buffer,gl_server.NetState.buffer_count,gl_server.NetState.command_stack,gl_server.NetState.stack_count);
    gl_server.NetState.buffer_count=0;

}


static void server() {
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

    int ticks=0;
    
    const int TICK_TIME = (int)((1.f/(float)gl_server.header.tickrate) * 1000.f);
    const int SNAP_TIME = (int)((1.f/(float)gl_server.header.snaprate) * 1000.f);

    u32 start_time = SDL_GetTicks();
    u32 last_frame_time=start_time;
    u32 last_snapshot_time = start_time;
    gl_server.time = start_time;

    socklen_t addr_len = sizeof(clientaddr);
    
    gs.time = gl_server.time;

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

    bool running=true;

    // tick thread
    while (running) {
        PollEvents(&input,&running);
        u64 server_time = SDL_GetTicks64();
        int delta = server_time - last_frame_time;

        if (gl_server.NetState.buffer_count > 0) {
            process_buffer_commands(server_time);
        }
        update_game_state(gs,gl_server.NetState,server_time);
        
        // send last second of commands
        // send_command_data();

        // Render start
        SDL_SetRenderDrawColor(sdl_renderer,255,255,0,255);
        SDL_RenderClear(sdl_renderer);

        for (u32 id=0; id<gs.player_count; id++) {
            character &p = gs.players[id];
            SDL_SetRenderDrawColor(sdl_renderer,255,0,0,255);
            
            SDL_Rect rect = {(int)p.pos.x,(int)p.pos.y,32,32};
            SDL_RenderFillRect(sdl_renderer, &rect);
        }
        
        SDL_RenderPresent(sdl_renderer);

        // send out the last x seconds worth of commands
        int timer = server_time - last_frame_time;
        gl_server.time = server_time;

        if (timer < TICK_TIME) {
            u32 next_frame_time = last_frame_time + TICK_TIME;
            if (next_frame_time - server_time > 0) {
                Sleep(next_frame_time - server_time);
            }
            last_frame_time = next_frame_time;
        } else {
            last_frame_time = server_time;
        }
    }
    closesocket(gl_server.socket);
}
