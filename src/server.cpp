
struct server_client_t {
    sockaddr_in addr;
    i64 connection_time;
    int ping;
    i32 last_processed_command=0;
    v2i last_captured_command_times={0,0};
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
            res.data.connection_dump.id = (entity_id)gl_server.client_count;

            volatile LARGE_INTEGER t_2 = gl_server.timer.get_high_res_elapsed();
            printf("Request recieved");

            res.data.connection_dump.t_1 = *(LARGE_INTEGER*) &t_1;
            res.data.connection_dump.t_2 = *(LARGE_INTEGER*) &t_2;
            
            send_packet(connect_socket,&clientaddr,&res);

            gl_server.s_clients[gl_server.client_count].addr = clientaddr;
            gl_server.clients[gl_server.client_count++] = clientaddr;

            gl_server.gms.connected_players++;

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
                    client_id = (entity_id)ind;
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
            // if we recieve commands that start after the end of the last block recieved, we must have a gap!
            v2i last_cmd_block = gl_server.s_clients[client_id].last_captured_command_times;
            if (p.data.command_data.start_tick > last_cmd_block.y) {
                printf("Command gap between %d and %d for client %d\n",last_cmd_block.y,p.data.command_data.start_tick,client_id);
            }
            gl_server.s_clients[client_id].last_captured_command_times = {p.data.command_data.start_tick,p.data.command_data.end_tick};
            gl_server.last_proc_command_buffer[client_id] = MAX(gl_server.last_proc_command_buffer[client_id],MAX(gl_server.s_clients[client_id].last_processed_command,p.data.command_data.end_tick));
            
            ReleaseMutex(gl_server._mt_merge_commands);

        } else if (p.type == CHAT_MESSAGE) {
            printf("Received a chat message from the client\n");
            // NOTE: the client shall send chat messages until the client recieves
            // a message broadcast from the server that matches the message.
            // NOTE: the server shall send chat messages to each client until the
            // client sends back a packet confirming the message was recieved
            std::string name(p.data.chat_message.name);
            std::string message(p.data.chat_message.message);
            
            add_to_chatlog(name,message,gl_server.last_tick);
            broadcast(&p);
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
    servaddr.sin_port = htons((u_short)port);
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

#ifdef PROFILE_BUILD
    timer_t perfCounter;
    perfCounter.Start();
#endif
    initialize_systems("server",false,false);

#ifdef PROFILE_BUILD
    double system_init_time = perfCounter.get();
#endif
    init_textures();
    
#ifdef PROFILE_BUILD
    double texture_init_time = perfCounter.get();
    std::cout << "Took " << system_init_time << " seconds to initialize systems" << std::endl;
    std::cout << "Took " << texture_init_time-system_init_time << " seconds to initialize textures" << std::endl;
#endif

    bool running=true;

    timer_t snap_clock;
    snap_clock.Start();

    double tick_hz=60;
    double snap_hz=60;
    double tick_delta=(1.0/tick_hz);
    double snap_delta=(1.0/snap_hz);

    gl_server.last_tick=0;
    gl_server.NetState.authoritative=true;
    gl_server.gms.state = PREGAME_SCREEN;

    // TODO: figure out input bufffer/whether the server should update and send information with
    // a built in latency or not
    // e.g. when sending a snapshot do they send the latest or the one from 2 snapshots ago, so it
    // never sends an incorrect snapshots?
    
    // base the snapshot buffer off of the ping of the players
    // make the snapshot buffer greater if some of the players have very high ping
    // set to 7 over wan
    // can be super low on lan
    int snapshot_buffer=0;
    int target_tick=0;
    // interp delay on clients should always be below the snapshot buffer on the server

    // TODO:
    // lower snapshot rate significantly

    entity_id spectate_player=ID_DONT_EXIST;
    camera_t game_camera;

    draw_shadows=false;
    
    // tick thread
    while (running) {
        bool new_frame_ready=false;

        if (gl_server.gms.state == GMS::GAME_PLAYING) {
            target_tick = gl_server.get_target_tick();
            gl_server.last_tick=target_tick;

            if (snap_clock.get() > snap_delta) {
                PollEvents(&input,&running);
                glClearColor(0.25f, 0.25f, 0.5f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
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

                    if (earliest_new_cmd_tick!=-1) {
                        i32 tick_before=gs.tick;
                        if (gs.tick-earliest_new_cmd_tick > 180) {
                            printf("WARNING: Loading 3 seconds back in time!!!\n");
                        }
                        find_and_load_gamestate_snapshot(gs,gl_server.NetState,earliest_new_cmd_tick);
                        // NOTE: check if we can comment this out
                        load_game_state_up_to_tick((void*)&gs,gl_server.NetState,tick_before);
                    }
                    
            command_merge_finish:
                    ReleaseMutex(gl_server._mt_merge_commands);
                }
                
                
                if (target_tick-snapshot_buffer >= 0) {
                    load_game_state_up_to_tick((void*)&gs,gl_server.NetState,target_tick-snapshot_buffer);

                    {
                        snapshot_t snap = {};
                        game_state &gms = *(game_state*)snap.data;
                        gms = gs;
                        for (i32 ind=0;ind<(i32)gl_server.client_count;ind++) {
                            snap.last_processed_command_tick[ind] = gl_server.s_clients[ind].last_processed_command;
                        }
                        gl_server.NetState.snapshots.push_back(snap);
                    }
                    
                    for (i32 client_id=0;client_id<(i32)gl_server.client_count;client_id++) {
                        // send each client a personalized snapshot that matches up to the last command received from them
                        packet_t p = {};
                        p.type = SNAPSHOT_TRANSIENT_DATA;
                        game_state &gms = *(game_state*)p.data.snapshot.data;
                        
                        i32 last_proc_command = gl_server.s_clients[client_id].last_processed_command;
                        // either update to the last EXACT update (i.e. where every client has processed all inputs)
                        // OR leave a little buffer
                        if (last_proc_command >= gs.tick) {
                            gms = gs;
                        } else {
                            i32 update_tick = last_proc_command;
                            if (update_tick<0) continue;

                            if (update_tick<gs.tick) {
                                find_and_load_gamestate_snapshot(gs,gl_server.NetState,update_tick);
                            }
                            load_game_state_up_to_tick((void*)&gs,gl_server.NetState,update_tick,true);
                            gms = gs;
                        }
                        p.data.snapshot.last_processed_command_tick[client_id] = last_proc_command;
                        send_packet(gl_server.socket,&gl_server.clients[client_id],&p);
                    }


                    // send important command data
                    packet_t p = {};
                    p.type = COMMAND_CALLBACK_INFO;
                    p.data.command_callback_info.count = 0;
                    for(i32 ind=0;ind<(i32)sound_queue.size();ind++){
                        sound_event event = sound_queue[ind];
                        if (event.tick<(target_tick-snapshot_buffer)-120) {
                            sound_queue.erase(sound_queue.begin()+ind);
                            ind--;
                            continue;
                        }
                        p.data.command_callback_info.sounds[p.data.command_callback_info.count++] = event;
                    }
                    broadcast(&p);
                    
                    // delete old snapshots
                    // OK but why are we loading snapshots from like, many seconds ago in the first place???
                    for (auto snap=gl_server.NetState.snapshots.begin();snap<gl_server.NetState.snapshots.end();) {
                        auto &snap_gms = *(game_state*)snap->data;
                        if (snap_gms.tick < target_tick-240) {
                            snap = gl_server.NetState.snapshots.erase(snap);
                        } else {
                            snap++;
                        }
                    }

                }
                new_frame_ready=true;

                if (input.just_pressed[SDL_SCANCODE_P]) {
                    if (spectate_player == ID_DONT_EXIST) {
                        if (gs.player_count>0) {
                            spectate_player = 0;
                        }
                    } else {
                        spectate_player++;
                        if (spectate_player >= gs.player_count) {
                            spectate_player = ID_DONT_EXIST;
                        }
                    }
                }
                if (input.just_pressed[SDL_SCANCODE_LEFTBRACKET]) {
                    snapshot_buffer--;
                    snapshot_buffer = MAX(snapshot_buffer,0);
                    printf("Changed snapshot buffer to %d\n",snapshot_buffer);
                } else if (input.just_pressed[SDL_SCANCODE_RIGHTBRACKET]) {
                    snapshot_buffer++;
                    printf("Changed snapshot buffer to %d\n",snapshot_buffer);
                }

                if (spectate_player != ID_DONT_EXIST) {
                    game_camera.follow = &gs.players[spectate_player];
                    game_camera.pos = game_camera.follow->pos;
                } else {
                    v2 mvec={0,0};
                    if (input.is_pressed[SDL_SCANCODE_D]) {
                        mvec.x = 5;
                    } if (input.is_pressed[SDL_SCANCODE_A]) {
                        mvec.x = -5;
                    } if (input.is_pressed[SDL_SCANCODE_S]) {
                        mvec.y = 5;
                    } if (input.is_pressed[SDL_SCANCODE_W]) {
                        mvec.y = -5;
                    }
                    game_camera.pos += mvec;
                }
                render_game_state(game_camera.follow,&game_camera);
            }
        } else if (gl_server.gms.state == GMS::PREGAME_SCREEN) {
            PollEvents(&input,&running);
            glClearColor(0.25f, 0.25f, 0.5f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            new_frame_ready=true;

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
                    load_new_game();

                    // is this correct..?
                    snap_clock.start_time = gl_server.gms.game_start_time;
                    gl_server.gms.counting_down_to_game_start=false;
                    gl_server.gms.state = GMS::GAME_PLAYING;
                    
                    goto endof_frame;                    
                }
            }

            double time_to_start = 0.0;
            if (gl_server.gms.counting_down_to_game_start) {
                time_to_start = static_cast<double>(gl_server.gms.game_start_time.QuadPart - gl_server.timer.get_high_res_elapsed().QuadPart)/gl_server.timer.frequency.QuadPart;
            }

            if (new_frame_ready) {
                render_pregame_screen(gl_server.gms,time_to_start);
            }

            v2i mpos = get_mouse_position();
            if (!gl_server.gms.counting_down_to_game_start) {
                iRect start_button_dest = {1280/2-(236/2),560,236,36};
                if (new_frame_ready) {
                    GL_DrawTexture(gl_textures[STARTGAME_BUTTON_TEXTURE],start_button_dest);
                }
                if (input.mouse_just_pressed && rect_contains_point(start_button_dest,mpos)) {
                    gl_server.gms.game_start_time.QuadPart = gl_server.timer.get_high_res_elapsed().QuadPart + (3 * gl_server.timer.frequency.QuadPart);

                    packet_t p = {};
                    p.type = GAME_START_ANNOUNCEMENT;
                    p.data.game_start_info.start_time = gl_server.gms.game_start_time;
                    p.data.game_start_info.gmode = gl_server.gms.gmode;

                    Random::Init();
                    p.data.game_start_info.seed = Random::seed;
                    broadcast(&p);
                    gl_server.gms.counting_down_to_game_start=true;
                }
            }

            // draw selector
            local_persist generic_drawable game_mode_text;
            local_persist bool generated=false;
            
            iRect l_arrow_src = {16,0,16,16};
            iRect r_arrow_src = {16,0,16,16};


            if (!generated) {
                generated=true;
                std::string n_str="UndefinedGameMode";
                if (gl_server.gms.gmode == GM_STRIKE)
                    n_str = "JoshStrike";
                else if (gl_server.gms.gmode == GM_JOSHFARE)
                    n_str = "JoshFare";
                else if (gl_server.gms.gmode == GM_JOSHUAGAME)
                    n_str = "Joshua Game";
                game_mode_text = generate_text(m5x7,n_str,COLOR_WHITE,game_mode_text.gl_texture);
                game_mode_text.scale = {2,2};
                game_mode_text.position = {WINDOW_WIDTH/2-game_mode_text.get_draw_irect().w/2, 450};
            }

            glUseProgram(sh_textureProgram);
            GL_DrawTexture(game_mode_text.gl_texture,game_mode_text.get_draw_irect());

            if (!gl_server.gms.counting_down_to_game_start) {            
                iRect left_arrow_rect = {game_mode_text.position.x - 20 - 32, game_mode_text.position.y + 16, 32,32};
                iRect right_arrow_rect = {game_mode_text.position.x + game_mode_text.get_draw_irect().w + 20, game_mode_text.position.y + 16, 32,32};
                if (rect_contains_point(left_arrow_rect,mpos)) {
                    l_arrow_src.x+=16;
                    if (input.mouse_just_pressed) {
                        gl_server.gms.gmode=(GAME_MODE)((i32)gl_server.gms.gmode-1);
                        if (gl_server.gms.gmode < 0) gl_server.gms.gmode = (GAME_MODE)(GM_COUNT-1);
                        generated=false;
                    }
                }

                if (rect_contains_point(right_arrow_rect,mpos)) {
                    r_arrow_src.x+=16;
                    if (input.mouse_just_pressed) {
                        gl_server.gms.gmode = (GAME_MODE)((i32)gl_server.gms.gmode+1);
                        if (gl_server.gms.gmode >= GM_COUNT) gl_server.gms.gmode=(GAME_MODE)0;
                        generated=false;
                    }
                }
            
                GL_DrawTextureEx(gl_textures[UI_TEXTURE],left_arrow_rect,l_arrow_src,true);
                GL_DrawTexture(gl_textures[UI_TEXTURE],right_arrow_rect,r_arrow_src);
            }            
        } else if (gl_server.gms.state == GMS::GAME_HASNT_STARTED) {
        }
endof_frame:
        
        if (new_frame_ready) {
            SDL_GL_SwapWindow(window);
            new_frame_ready=false;
        }
        
        
        // sleep up to next snapshot
        // this is based on snapshots during gameplay but ticks during the menu
        // perhaps a uniform system that works regardless of gamestate is best
        // when calculating current frame times and how long to sleep

        // TODO: GET RID OF THIS!!! Once things are working again!!!
        
        if (gl_server.gms.state == GMS::GAME_PLAYING) {
            // sleep to next tick
            double len = snap_delta - snap_clock.get() - (snap_delta / 8.0);
            if (len > 0) {
                SDL_Delay(u32(len*1000.0));
            }
        } else if (gl_server.gms.state == GMS::PREGAME_SCREEN) {
            SDL_Delay(16);
        }
    }
    closesocket(gl_server.socket);
}
