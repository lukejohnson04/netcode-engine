
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <time.h>
#include <cmath>
#include <tchar.h>
#include <conio.h>
#include <strsafe.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#include <vector>
#include <algorithm>
#include <iostream>

#define DEFAULT_PORT 13172
#define DEFAULT_IP "127.0.0.1"
#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include "common.h"

#include "render.cpp"
#include "audio.cpp"
#include "text.cpp"
#include "input.cpp"

#include "entity.cpp"
#include "player.cpp"

#include "gamestate.cpp"
#include "network.cpp"
#include "server.cpp"
#include "client.cpp"


static void GameGUIStart() {
    SDL_Window *window=nullptr;
    SDL_Surface *screenSurface = nullptr;

    if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf( "SDL could not initialize! SDL Error: %s\r\n", SDL_GetError() );
        return;
    }
    
    if (TTF_Init() != 0) {
        printf("Error: %s\n", TTF_GetError()); 
        return;
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        printf("Error: %s\n", IMG_GetError());
        return;
    }

    if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 2048 ) < 0 )
    {
        printf( "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
        return;
    }

    char winstr[] = "Client x";
    winstr[7] = client_st.client_id + '0';
    window = SDL_CreateWindow(winstr,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              1280,
                              720,
                              SDL_WINDOW_SHOWN);
    printf("Created window\n");

    if (window == nullptr) {
        printf("Window could not be created. SDL Error: %s\n", SDL_GetError());
    }

    SDL_Renderer* sdl_renderer;

    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (sdl_renderer == nullptr) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    screenSurface = SDL_GetWindowSurface(window);
    init_textures(sdl_renderer);
    init_sfx();
    m5x7 = TTF_OpenFont("../res/m5x7.ttf",16);
    

    bool running=true;

    
    client_st.init_ok=true;

    int input_send_hz=60;
    int tick_hz=60;
    int frame_hz=0; // uncapped framerate
    double tick_delta=(1.0/tick_hz);
    double input_send_delta = (1.0/input_send_hz);

    // have server send next tick time
    // currently we have the server time and the last tick
    // but we aren't sure how long ago the last tick was

    timer_t input_send_timer;
    input_send_timer.Start();

    int target_tick=0;

    // health text
    generic_drawable health_text = generate_text(sdl_renderer,m5x7,"100",{255,0,0,255});
    health_text.position = {16,720-16-(health_text.get_draw_rect().h)};

    int connected_last_tick = client_st.gms.connected_players;

    client_st.gms.state = GMS::PREGAME_SCREEN;
    
    while (running) {
        // input
        PollEvents(&input,&running);
        character *player=nullptr;
        if (gs.players[client_st.client_id].id != ID_DONT_EXIST) {
            player = &gs.players[client_st.client_id];
        }

        if (client_st.gms.state == GMS::ROUND_PLAYING) {
            update_player_controller(player,target_tick);
            if (client_snapshot_buffer_stack.size() > 0) {
                for (auto &snap:client_snapshot_buffer_stack) {
                    client_st.NetState.snapshots.push_back(snap);
                }
                std::sort(client_st.NetState.snapshots.begin(),client_st.NetState.snapshots.end(),[](game_state &left, game_state &right) {return left.tick < right.tick;});
                client_snapshot_buffer_stack.clear();
            }
            if (load_new_snapshot) {
                gs = new_snapshot;
                load_new_snapshot=false;
                printf("Loaded snapshot\n");
            }
            
            bool update_health_display=false;
            int p_health_before_update = player ? player->health : 0;

            int o_num=target_tick;
            target_tick = client_st.get_exact_current_server_tick();
            
            while(gs.tick<target_tick) {
                gs.update(client_st.NetState,tick_delta);
                gs.tick++;
            }
            // interpolate
            if (client_st.NetState.snapshots.size() > 0) {
                int interp_tick = target_tick-client_st.NetState.interp_delay;
                game_state *prev_snap=nullptr;
                game_state *next_snap=nullptr;
                game_state *exac_snap=nullptr;

                if (client_st.NetState.snapshots.back().tick < interp_tick) {
                    printf("Failed interp tick %d: most recent snapshot is %d\n",interp_tick,client_st.NetState.snapshots.back().tick);
                }
                // actually, this whole process of lerping is a little pointless when we have 60 snapshots a second
                // cause we don't actually lerp at all unless there's packet loss... we just set it to one snapshot
                // position and then the next frame set it to the next

                // this whole process of finding interp snapshots only needs to be done once for all players btw
                for (i32 ind=(i32)client_st.NetState.snapshots.size()-1;ind>=0;ind--){
                    game_state &snap = client_st.NetState.snapshots[ind];
                
                    if (snap.tick == interp_tick) {
                        exac_snap=&snap;
                        break;
                    } else if (snap.tick>interp_tick) {
                        next_snap = &snap;
                
                    } else if (snap.tick<interp_tick) {
                        prev_snap = &snap;
                        break;
                    }
                }
                if (!exac_snap && (!next_snap || !prev_snap)){
                    printf("No snapshots to interpolate between\n");
                } else {


                    FORn(gs.players,gs.player_count,chara) {
                        if (chara==player) {
                            continue;
                        }
                        if (exac_snap) {
                            chara->pos = exac_snap->players[chara->id].pos;
                        } else if (next_snap && prev_snap) {
                            v2 prev_pos = prev_snap->players[player->id].pos;
                            v2 next_pos = next_snap->players[player->id].pos;
                            float f = (float)(interp_tick-prev_snap->tick) / (float)(next_snap->tick-prev_snap->tick);
                            player->pos = lerp(prev_pos,next_pos,f);
                        }
                    }
                }
            }
            last_tick_processed_in_client_loop=target_tick;
            
            if (player && player->health != p_health_before_update) {
                health_text = generate_text(sdl_renderer,m5x7,std::to_string(player->health),{255,0,0,255});
                health_text.position = {16,720-16-(health_text.get_draw_rect().h)};
            }

            if (o_num != target_tick) {
                printf("Tick %d\n",target_tick);
                if (input_send_timer.get() > input_send_delta) {
                    client_send_input(target_tick);
                    input_send_timer.add(input_send_delta);
                }
            }

            render_game_state(sdl_renderer);
            // gui
            SDL_RenderCopy(sdl_renderer,health_text.texture,NULL,&health_text.get_draw_rect());
        } else if (client_st.gms.state == GMS::PREGAME_SCREEN) {
            // if the clock runs out, start the game
            double time_to_start=0.0;
            if (client_st.gms.counting_down_to_game_start) {
                if (client_st.gms.game_start_time.QuadPart - client_st.sync_timer.get_high_res_elapsed().QuadPart <= 0) {
                    // game_start
                    client_st.gms.state = GMS::ROUND_PLAYING;
                    client_st.gms.counting_down_to_game_start=false;
                    input_send_timer.Restart();

                    // setup interp settings
                    for (i32 ind=0; ind<client_st.gms.connected_players; ind++) {
                        client_st.NetState.do_charas_interp[ind] = true;
                    }
                    client_st.NetState.do_charas_interp[client_st.client_id] = false;
                    
                    goto endof_frame;
                }
                time_to_start = static_cast<double>(client_st.gms.game_start_time.QuadPart - client_st.sync_timer.get_high_res_elapsed().QuadPart)/client_st.sync_timer.frequency.QuadPart;
            }
        
            render_pregame_screen(sdl_renderer,client_st.gms,time_to_start);
        }

endof_frame:
        
        SDL_RenderPresent(sdl_renderer);
        
        // sleep until next tick

        // when you don't sleep, it works fine?!?!
        /*
        if (client_st.gms.state == GMS::ROUND_PLAYING) {
            double time_to_next_tick=client_st.get_time_to_next_tick();
            DWORD sleep_time = (DWORD)(time_to_next_tick*1000);
            if (sleep_time > 3) {
                printf("%d\n",sleep_time);
                SDL_Delay(sleep_time);
            }
        }
        */
        
    }
    
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    TTF_Quit();
    IMG_Quit();
    Mix_Quit();
}


int main(int argc, char *argv[]) {
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0) {
        printf("Failed to startup winsock. Error code: %d", WSAGetLastError());
        return -1;
    }

    
    if (argc > 1 && !strcmp(argv[1], "client")) {
        int port = DEFAULT_PORT;
        std::string ip_addr=DEFAULT_IP;
        if (argc < 2) {
            fprintf(stderr, "not enough args!");
            return -1;
        } if (argc == 3) {
            sscanf_s(argv[2], "%d", &port);
        } if (argc == 4) {
            ip_addr = std::string(argv[3]);
        }
        client_connect(port,ip_addr);
    } else {
        server();
    }
    return 0;
}
