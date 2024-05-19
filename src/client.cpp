
struct client_t {
    u32 client_id=ID_DONT_EXIST;
    int socket;
    sockaddr_in servaddr;

    double ping=0;
    int last_tick_processed=0;
    int last_command_processed_by_server=0;
    i32 last_bomb_tick=0;
    double bomb_tick_timer=1.0;

    timer_t sync_timer;
    netstate_info_t NetState;
    std::vector<snapshot_t> merge_snapshots;
    std::vector<chatline_props> merge_chat;

    std::queue<packet_t> packet_queue;

    overall_game_manager &gms=NetState.gms;

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

    std::string username="";

    bool loaded_new_map=false;
    bool load_new_map_signal=false;

    // mutex
    HANDLE _mt_merge_snapshots, _mt_merge_chat, _mt_map, _mt_gms, _mt_packet_queue;
    bool _new_merge_snapshot=false;
    i32 _last_proc_tick_buffer=0;
};


global_variable client_t client_st={};

internal void add_command_to_recent_commands(command_t cmd) {
    command_node node = {cmd,false};
    client_st.NetState.command_stack.push_back(node);
}

internal void process_packet(packet_t pc) {
    if (pc.type == NEW_CLIENT_CONNECTED) {
        //WaitForSingleObject(client_st._mt_gms,INFINITE);
        client_st.gms.connected_players++;
        //ReleaseMutex(client_st._mt_gms);

    } else if (pc.type == INFO_DUMP_ON_CONNECTED) {
        //WaitForSingleObject(client_st._mt_gms,INFINITE);
        client_st.gms.connected_players=pc.data.info_dump_on_connected.client_count;
        client_st.client_id = client_st.gms.connected_players-1;
        printf("there are %d connceted players\n",client_st.gms.connected_players);
        //ReleaseMutex(client_st._mt_gms);
            
    } else if (pc.type == SNAPSHOT_TRANSIENT_DATA) {
        //WaitForSingleObject(client_st._mt_merge_snapshots,INFINITE);

        client_st.merge_snapshots.push_back(pc.data.snapshot);
        client_st._new_merge_snapshot = true;
        client_st._last_proc_tick_buffer=MAX(client_st._last_proc_tick_buffer,pc.data.snapshot.last_processed_command_tick[client_st.client_id]);

        //ReleaseMutex(client_st._mt_merge_snapshots);
            
    } else if (pc.type == SNAPSHOT_PERSIST_DATA) {
            
    } else if (pc.type == COMMAND_CALLBACK_INFO) {
        for (i32 ind=0;ind<pc.data.command_callback_info.count;ind++) {
            if (pc.data.command_callback_info.sounds[ind].type == SfxType::PLANT_FINISHED_SFX) {
                printf("Plant finished sound\n");
            }
            queue_sound(pc.data.command_callback_info.sounds[ind]);
        }
                        
    } else if (pc.type == SNAPSHOT_TRANSIENT_DATA) {
        //WaitForSingleObject(client_st._mt_merge_snapshots,INFINITE);

        client_st.merge_snapshots.push_back(pc.data.snapshot);
        client_st._new_merge_snapshot = true;
        client_st._last_proc_tick_buffer=MAX(client_st._last_proc_tick_buffer,pc.data.snapshot.last_processed_command_tick[client_st.client_id]);

        //ReleaseMutex(client_st._mt_merge_snapshots);
            
    } else if (pc.type == COMMAND_CALLBACK_INFO) {
        for (i32 ind=0;ind<pc.data.command_callback_info.count;ind++) {
            if (pc.data.command_callback_info.sounds[ind].type == SfxType::PLANT_FINISHED_SFX) {
                printf("Plant finished sound\n");
            }
            queue_sound(pc.data.command_callback_info.sounds[ind]);
        }

    } else if (pc.type == GAME_START_ANNOUNCEMENT) {
        std::cout << "Recieved game start announcement\n";

        //WaitForSingleObject(client_st._mt_gms,INFINITE);
        client_st.gms.game_start_time = pc.data.game_start_info.start_time;
        client_st.gms.counting_down_to_game_start=true;
        client_st.gms.gmode = pc.data.game_start_info.gmode;

        Random::Init(pc.data.game_start_info.seed);
        load_new_game();
        client_st.loaded_new_map=true;

    } else if (pc.type == CHAT_MESSAGE) {
        //WaitForSingleObject(client_st._mt_merge_snapshots,INFINITE);
        printf("Received chat message from the server\n");
        std::string name(pc.data.chat_message.name);
        std::string message(pc.data.chat_message.message);
        std::cout << name << ": " << message << std::endl;

        client_st.merge_chat.push_back({name,message,client_st.last_tick_processed});
        //ReleaseMutex(client_st._mt_merge_chat);

    } else {
        // this gets called when it times out each time from no packet recieved
        printf("wtf\n");
    }
}



DWORD WINAPI ClientListen(LPVOID lpParamater) {
    const int connect_socket = *(int*)lpParamater;
    printf("Thread started!\n\r");
    sockaddr_in servaddr;
    printf("Started listen thread\n");

    while (1) {
        packet_t pc = {};
        int ret = recieve_packet(connect_socket,&servaddr,&pc);

        WaitForSingleObject(client_st._mt_packet_queue,INFINITE);
        client_st.packet_queue.push(pc);
        ReleaseMutex(client_st._mt_packet_queue);
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

#ifdef RELEASE_BUILD
    std::cout << "Please enter your username: ";
    std::cin >> client_st.username;
#endif

    printf("Attempting connection on port %d to ip %s\n",port,ip_addr.c_str());

    // connection request
    packet_t p = {};
    p.type = CONNECTION_REQUEST;


    client_st.sync_timer.Start();
    volatile LARGE_INTEGER t_0=client_st.sync_timer.get_high_res_elapsed();

    volatile int ret = send_packet(connect_socket,&servaddr,&p);

    p = {};
    ret = recieve_packet(connect_socket,&servaddr,&p);
    printf("received packet\n");

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
        client_st.NetState.interp_delay = MAX(6,4 + (int)ceil(((client_st.ping/1000.0)/2.0)/(1.0/60.0) * 1.25));
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

    client_st._mt_merge_snapshots = CreateMutex(NULL,FALSE,NULL);
    client_st._mt_merge_chat = CreateMutex(NULL,FALSE,NULL);
    client_st._mt_map = CreateMutex(NULL,FALSE,NULL);
    client_st._mt_gms = CreateMutex(NULL,FALSE,NULL);
    client_st._mt_packet_queue = CreateMutex(NULL,FALSE,NULL);

#ifndef RELEASE_BUILD
    client_st.username = "Player"+std::to_string(client_st.client_id);
#endif

    DWORD ThreadID=0;
    HANDLE thread_handle = CreateThread(0, 0, &ClientListen, (LPVOID)&connect_socket, 0, &ThreadID);
    GameGUIStart();
}

#define CLIENT_TICK_BUFFER_FOR_SENDING_COMMANDS 30


static void client_send_input(int curr_tick) {
    // really we should be sending the last second or so of commands
    // to combat packet loss, but we won't worry about that for now
    // because it gives us a massive headache removing duplicates
    packet_t p = {};
    p.type = COMMAND_DATA;
    p.data.command_data = {};

    i32 start_tick = curr_tick - CLIENT_TICK_BUFFER_FOR_SENDING_COMMANDS;
    i32 end_tick = curr_tick;
    p.data.command_data.count=0;

    // cmd tick can be equal to either start tick or end tick
    for (auto node=client_st.NetState.command_stack.begin();node<client_st.NetState.command_stack.end();node++) {
        if (node->cmd.tick < start_tick) {
            /*
            if (node->cmd.tick < end_tick - 180) {
                node = client_st.NetState.command_stack.erase(node);
                node--;
                continue;
            }
            */
            continue;
        }
        
        if (node->cmd.tick > end_tick) {
            continue;
        }
        if (node->verified==false) {
            p.data.command_data.commands[p.data.command_data.count++] = node->cmd;
        }
    }

    p.data.command_data.start_tick = start_tick;
    p.data.command_data.end_tick = end_tick;
    
    assert(p.data.command_data.count <= 64);
    
    send_packet(client_st.socket,&client_st.servaddr,&p);
}
