
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
            
            send_packet(connect_socket,&clientaddr,&res);

            packet_t gs_full_info = {};
            gs_full_info.type = GS_FULL_INFO;
            gs_full_info.data.full_info_dump = {};
            gs_full_info.data.full_info_dump.p_count=gs.player_count;

            for (i32 ind=0; ind<gs.player_count; ind++) {
                gs_full_info.data.full_info_dump.players[ind] = gs.players[ind];
            }
            
            send_packet(connect_socket,&clientaddr,&gs_full_info);

            gl_server.s_clients[gl_server.client_count].addr = clientaddr;
            gl_server.clients[gl_server.client_count++] = clientaddr;

            add_player();
            gs.players[gs.player_count-1].id = gs.player_count-1;
            
            packet_t player_join = {};
            player_join.type = ADD_PLAYER;
            player_join.data.id = res.data.id;
            broadcast(&player_join);

        } else if (p.type == PING) {

            /*
            // ping after first connect
            server_client_t &cl = gl_server.s_clients[gl_server.client_count];
            cl.connection_time = p.data.ping_dump.server_time_on_accept;
            cl.ping = SDL_GetTicks() - cl.connection_time;
            */
            // realistically should send the ping back..?
        } else if (p.type == COMMAND_DATA) {
            
            int new_commands=0;
            for (i32 n=0;n<p.data.command_data.count;n++) {
                command_t &cmd = p.data.command_data.commands[n];
                gl_server.NetState.command_buffer.push_back(cmd);
                new_commands++;
            }
            
            printf("%d new commands from %f to %f\n", new_commands, p.data.command_data.start_time, p.data.command_data.end_time);
        }
    }
    return 0;
}

int partition(command_t *arr, int low, int high) {
    double pivot = arr[low].time;
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
    double end_time=gl_server.time;
    double start_time=end_time-0.5;

    packet_t p = {};
    p.type = COMMAND_DATA;
    p.data.command_data.start_time = start_time;
    p.data.command_data.end_time = end_time;
    p.data.command_data.count = 0;

    for (auto &cmd: gl_server.NetState.command_stack) {
        if (cmd.time > end_time) {
            break;
        } else if (cmd.time >= start_time) {
            p.data.command_data.commands[p.data.command_data.count++] = cmd;
        }
    }
    printf("%d\n",p.data.command_data.count);
    broadcast(&p);
}


static void send_snapshot() {
    
}


static void process_buffer_commands(double server_time) {
    // disregard commands from greater than 500ms ago
    std::vector<command_t> &buf=gl_server.NetState.command_buffer;
    for (i32 ind=0; ind<gl_server.NetState.command_buffer.size(); ind++) {
        if (buf[ind].time < server_time-0.5) {
            buf.erase(buf.begin()+ind);
            ind--;
        }
    }
    if (gl_server.NetState.command_buffer.size()==0) {
        return;
    }
    // rewind to first command there and update up
    if (buf.size() > 1) {
        std::sort(buf.begin(),buf.end(),command_sort_by_time());
        //quicksort_commands(gl_server.NetState.command_buffer,0,gl_server.NetState.buffer_count);
    }
    command_t first_cmd = gl_server.NetState.command_buffer[0];
    if (first_cmd.sending_id == ID_DONT_EXIST) {
        printf("FUCK\n");
        // bad news this just got ran!!!!
    }

    // dont rewind state it doesnt run
    // update to past (present - 2 seconds for example) and it runs fine but out of sync
    if (first_cmd.time < gs.time) {
        printf("Rewinding %fs\n",server_time-first_cmd.time);
        rewind_game_state(gs,gl_server.NetState,first_cmd.time);
    }
    gl_server.NetState.command_stack.insert(gl_server.NetState.command_stack.end(),buf.begin(),buf.end());
    std::sort(gl_server.NetState.command_stack.begin(),gl_server.NetState.command_stack.end(),command_sort_by_time());
    gl_server.NetState.command_buffer.clear();
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
    gl_server.header.tickrate = 50;
    
    const double TICK_TIME = (1.0/(double)gl_server.header.tickrate);
    const double SNAP_TIME = (1.0/(double)gl_server.header.snaprate);

    /*
    {
        i64 start_time;
        LARGE_INTEGER frequency;        // Frequency of the performance counter
        LARGE_INTEGER start, end;       // Ticks at the start and end of the operation
        double interval;

        // Retrieve the frequency of the performance counter
        if (!QueryPerformanceFrequency(&frequency)) {
            std::cerr << "High-resolution performance counter not supported." << std::endl;
            return;
        }

        // Get the current counter value at start
        QueryPerformanceCounter(&start_time);

        // Perform some operations whose duration you want to measure
        Sleep(1000);  // Simulating a delay (1000 milliseconds)

        // Get the current counter value at end
        QueryPerformanceCounter(&end);

        // Calculate the interval in seconds
        interval = static_cast<double>(end.QuadPart - start_time.QuadPart) / frequency.QuadPart;

        std::cout << "Duration: " << interval << " seconds." << std::endl;
    }
    */
    gl_server.timer.Start();
    
    double last_tick_time=0;
    double last_snap_time=0;
    gl_server.time=0;
    gs.time = 0;

    socklen_t addr_len = sizeof(clientaddr);
    

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

    // tick thread
    while (running) {
        PollEvents(&input,&running);
        double server_time = gl_server.timer.get();
        // best guess is the commands are recorded on the client as being far off from when they
        // occured in actual server time
        double delta = server_time - last_tick_time;

        if (gl_server.NetState.command_buffer.size() > 0) {
            process_buffer_commands(server_time);
        }
        update_game_state(gs,gl_server.NetState,server_time);

        // send last second of commands
        gl_server.time = server_time;
        send_command_data();
        // Render start
        SDL_SetRenderDrawColor(sdl_renderer,255,255,0,255);
        SDL_RenderClear(sdl_renderer);
        
        for (i32 id=0; id<gs.player_count; id++) {
            character &p = gs.players[id];
            //SDL_SetRenderDrawColor(sdl_renderer,255,0,0,255);

            SDL_Rect src_rect = {p.curr_state == character::PUNCHING ? 64 : 0,0,32,32};
            SDL_Rect rect = {(int)p.pos.x,(int)p.pos.y,64,64};
            SDL_RenderCopy(sdl_renderer,textures[PLAYER_TEXTURE],&src_rect,&rect);
            //SDL_RenderFillRect(sdl_renderer, &rect);
        }

        for (i32 id=0; id<gs.bullet_count; id++) {
            bullet_t &b = gs.bullets[id];
            SDL_SetRenderDrawColor(sdl_renderer,0,0,0,255);
            
            SDL_Rect rect = {(int)b.pos.x,(int)b.pos.y,8,8};
            SDL_RenderFillRect(sdl_renderer, &rect);
        }

        
        SDL_RenderPresent(sdl_renderer);

        // send out the last x seconds worth of commands

        {
            double next_snap_time = last_snap_time + SNAP_TIME;
            double next_tick_time = last_tick_time + TICK_TIME;
            double curr = gl_server.timer.get();
            if (curr > next_snap_time) {
                snapshot_t snap;
                // align this to current tick..?
                snap.time = gs.time;
                snap.state = gs;
                packet_t p = {};
                p.type = SNAPSHOT_DATA;
                p.data.snapshot = snap;

                broadcast(&p);
            }
            
            if (next_tick_time > curr) {
                Sleep((DWORD)((next_tick_time - curr)*1000.0));
            }
            last_tick_time = server_time;
        }

    }
    closesocket(gl_server.socket);
}
