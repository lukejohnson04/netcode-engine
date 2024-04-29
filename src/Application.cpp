
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <time.h>
#include <cmath>
#include <math.h>
#include <tchar.h>
#include <conio.h>
#include <strsafe.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#include <unordered_set>
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
    m5x7 = TTF_OpenFont("res/m5x7.ttf",16);
    

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

    int connected_last_tick = client_st.gms.connected_players;

    client_st.gms.state = GMS::PREGAME_SCREEN;

    struct {
        generic_drawable health_text;
        generic_drawable reload_text;
        generic_drawable ability_sprites[4];
        generic_drawable ability_ui_box;
        generic_drawable magazine_cnt_text;
    } gui_elements;
    gui_elements.health_text = generate_text(sdl_renderer,m5x7,"100",{255,0,0,255});
    gui_elements.health_text.position = {16,720-16-(gui_elements.health_text.get_draw_rect().h)};
    gui_elements.ability_ui_box.texture = textures[TexType::UI_TEXTURE];
    
    for (i32 ind=0;ind<4;ind++) {
        generic_drawable &sprite=gui_elements.ability_sprites[ind];
        sprite.texture = textures[TexType::ABILITIES_TEXTURE];
        
        sprite.bound = {ind*48,0,48,48};
        sprite.scale = {1.5f,1.5f};
        sprite.position = {1280-16-((4-ind)*sprite.get_draw_rect().w),720-16-sprite.get_draw_rect().h};
    }

    
    while (running) {
        // input
        PollEvents(&input,&running);
        character *player=nullptr;
        if (gs.players[client_st.client_id].id != ID_DONT_EXIST) {
            player = &gs.players[client_st.client_id];
        }

        if (client_st.gms.state == GMS::ROUND_PLAYING) {
            i32 prev_wall_count=gs.wall_count;
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
                //printf("Loaded snapshot\n");
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
            // TODO: there's a delay when you punch another player
            // this is because the other player is only interpolated,
            // so we don't update them getting knocked back. We simply
            // punch and then wait for the server to tell us they took
            // knockback. This isn't horribly noticable but is kind of awful
            // from a game feel perspective. The solution is to use command data
            // to interpolate. We have enough ticks worth of data to do it anyways,
            // the linear interpolation is barely even noticable at 60 ticks
            if (client_st.NetState.snapshots.size() > 0) {
                int interp_tick = target_tick-client_st.NetState.interp_delay;
                int old_interp_tick = o_num-client_st.NetState.interp_delay;
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
                        if (chara->curr_state==character::TAKING_DAMAGE) {
                            continue;
                        }
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
                
                // process all command callbacks
                for (i32 ind=0; ind<client_st.NetState.command_callback_info.size(); ind++) {
                    auto p=client_st.NetState.command_callback_info[ind];
                    if (p.second.tick <= interp_tick) {
                        if (p.second.tick>=old_interp_tick) {
                            // need a robust way to check if a sound has been processed or not already
                            // on the client side
                            if (p.first != player->id || p.second.code == CMD_DIE) {
                                command_callback(&gs.players[p.first],p.second);
                            }
                        }
                        // its probably processing the command, deleting it, and then recieving a new
                        // one from the server (because the server buffers a couple ticks of the callbacks)
                        client_st.NetState.command_callback_info.erase(client_st.NetState.command_callback_info.begin()+ind);
                        ind--;
                    }
                }
            }
            last_tick_processed_in_client_loop=target_tick;
            
            if (player) {
                if (player->health != p_health_before_update) {
                    gui_elements.health_text = generate_text(sdl_renderer,m5x7,std::to_string(player->health),{255,0,0,255});
                    gui_elements.health_text.position = {16,720-16-(gui_elements.health_text.get_draw_rect().h)};
                } if (player->reloading) {
                    std::string reload_str = std::to_string(player->reload_timer);
                    if (reload_str.size() > 3) {
                        reload_str.erase(reload_str.begin()+3,reload_str.end());
                    }
                    gui_elements.reload_text = generate_text(sdl_renderer,m5x7,reload_str,{255,0,0,255});
                    gui_elements.reload_text.position = player->pos + v2i(16-8,48);
                }
            }

            if (o_num != target_tick) {
                //printf("Tick %d\n",target_tick);
                if (input_send_timer.get() > input_send_delta) {
                    client_send_input(target_tick);
                    input_send_timer.add(input_send_delta);
                }
            }

            if (prev_wall_count != gs.wall_count) {
                // regenerate raycast points
                auto &rps = client_sided_render_geometry.raycast_points;
                rps.clear();
                for (i32 n=0; n<gs.wall_count; n++) {
                    v2i &wall=gs.walls[n];
                    rps.push_back(v2i(wall.x,wall.y)*64);
                    rps.push_back(v2i(wall.x+1,wall.y)*64);
                    rps.push_back(v2i(wall.x,wall.y+1)*64);
                    rps.push_back(v2i(wall.x+1,wall.y+1)*64);
                }
                printf("Points before: %d\n",(i32)rps.size());
                std::sort( rps.begin(), rps.end(), [](auto&left,auto&right){
                  return *(double*)&left < *(double*)&right;
                });
                
                for (auto pt=rps.begin();pt<rps.end();pt++) {
                    i32 dupes=0;
                    if (pt!=rps.end()-1 && *(pt+1) != *pt) continue;
                    auto it=pt;
                    while (it < rps.end()) {
                        it++;
                        if (*it == *pt) {
                            dupes++;
                        } else {
                            break;
                        }
                    }
                    
                    if (dupes==1 || dupes == 3) {
                        rps.erase(pt,it);
                        pt--;
                    } else {
                        pt=it-1;
                    }
                    
                }
                printf("Points after: %d\n",(i32)rps.size());
            }
            render_game_state(sdl_renderer,player);
            // gui
            SDL_RenderCopy(sdl_renderer,gui_elements.health_text.texture,NULL,&gui_elements.health_text.get_draw_rect());
            if (player) {
                if (player->reloading) {
                    SDL_RenderCopy(sdl_renderer,gui_elements.reload_text.texture,NULL,&gui_elements.reload_text.get_draw_rect());
                }
            }

            // render the score at the end of the round
            if (gs.one_remaining_tick!=0 && gs.tick > gs.one_remaining_tick) {
                std::string left_str = std::to_string(gs.score[0]);
                std::string right_str = std::to_string(gs.score[1]);
                generic_drawable middle_text = generate_text(sdl_renderer,m5x7,"-",{0,0,0,255});
                generic_drawable left_text = generate_text(sdl_renderer,m5x7,left_str,*(SDL_Color*)&gs.players[0].color);
                generic_drawable right_text = generate_text(sdl_renderer,m5x7,right_str,*(SDL_Color*)&gs.players[1].color);
                
                middle_text.scale = {16,16};
                left_text.scale = {16,16};
                right_text.scale = {16,16};
                middle_text.position = {1280/2 - middle_text.get_draw_rect().w/2,720/2+middle_text.get_draw_rect().h/2};
                left_text.position = {middle_text.position.x - left_text.get_draw_rect().w-16,middle_text.position.y};
                right_text.position = {middle_text.position.x + middle_text.get_draw_rect().w+16,middle_text.position.y};

                SDL_RenderCopy(sdl_renderer,left_text.texture,NULL,&left_text.get_draw_rect());
                SDL_RenderCopy(sdl_renderer,middle_text.texture,NULL,&middle_text.get_draw_rect());
                SDL_RenderCopy(sdl_renderer,right_text.texture,NULL,&right_text.get_draw_rect());
            }
            
            if (player) {
                // draw abilities
                generic_drawable cooldown_text;
                std::string cooldown_str;
                for (i32 ind=0;ind<4;ind++) {
                    generic_drawable *sprite=&gui_elements.ability_sprites[ind];

                    // render the box
                    SDL_Rect dest = sprite->get_draw_rect();
                    SDL_RenderCopy(sdl_renderer,textures[TexType::UI_TEXTURE],NULL,&dest);
                    SDL_RenderCopy(sdl_renderer,sprite->texture,(SDL_Rect*)&sprite->bound,&sprite->get_draw_rect());
                    if (ind==0 && player->invisibility_cooldown > 0) {
                        render_ability_sprite(sprite,player->invisibility_cooldown,sdl_renderer);
                    } else if (ind == 1 && player->shield_cooldown > 0) {
                        render_ability_sprite(sprite,player->shield_cooldown,sdl_renderer);
                    } else if (ind == 2 && player->fireburst_cooldown > 0) {
                        render_ability_sprite(sprite,player->fireburst_cooldown,sdl_renderer);
                    } else if (ind == 3 && player->reloading) {
                        render_ability_sprite(sprite,player->reload_timer,sdl_renderer);
                    }
                }
                local_persist i32 ammo_cnt = 0;
                if (player->bullets_in_mag != ammo_cnt) {
                    ammo_cnt = player->bullets_in_mag;
                    std::string ammo_str = std::to_string(ammo_cnt);
                    gui_elements.magazine_cnt_text = generate_text(sdl_renderer,m5x7,ammo_str,{255,255,255,255});
                    gui_elements.magazine_cnt_text.scale = {4,4};
                    auto ab_pos = gui_elements.ability_sprites[3].get_draw_rect();
                    gui_elements.magazine_cnt_text.position = {ab_pos.x+24-gui_elements.magazine_cnt_text.get_draw_rect().w/2,ab_pos.y+16};
                }
                SDL_RenderCopy(sdl_renderer,gui_elements.magazine_cnt_text.texture,NULL,&gui_elements.magazine_cnt_text.get_draw_rect());
            }
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
            // render player color selector
            /*SDL_Rect p_rect = {0,0,32,32};
            SDL_Rect dest_rect = {200,500,64,64};
            SDL_RenderCopy(sdl_renderer,textures[TexType::PLAYER_TEXTURE],&p_rect,&dest_rect);
            */
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

static void demo() {
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

    window = SDL_CreateWindow("Demo",
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
    m5x7 = TTF_OpenFont("res/m5x7.ttf",16);

    bool running=true;

    i32 wall_count=0;
    v2i walls[32];

    for (i32 n=0;n<4;n++) {
        walls[wall_count++] = {12,n+4};
    }
    walls[wall_count++] = {4,4};
    walls[wall_count++] = {4,5};
    walls[wall_count++] = {6,2};
    walls[wall_count++] = {9,7};
    walls[wall_count++] = {15,9};


    SDL_Texture *dot = IMG_LoadTexture(sdl_renderer,"res/dot.png");
    
    while (running) {
        // input
        PollEvents(&input,&running);
        SDL_SetRenderDrawColor(sdl_renderer,255,255,255,255);
        SDL_RenderClear(sdl_renderer);
        SDL_SetRenderDrawColor(sdl_renderer,0,0,0,255);
        FORn(walls,wall_count,wall) {
            SDL_Rect rect = {(int)wall->x*64,wall->y*64,64,64};
            SDL_RenderFillRect(sdl_renderer, &rect);
        }

        SDL_SetRenderDrawColor(sdl_renderer,255,0,0,255);

        v2i mpos = get_mouse_position();

        /*
          Draft 2
        for (i32 n=0;n<40;n++) {
            float angle = ((PI*2.f) * n) / 40.f;
            v2 t = mpos + convert_angle_to_vec(angle) * 1200.f;

            SDL_Rect dot_dest;
            intersect_props col = get_collision(mpos,t,walls,wall_count);
            if (col.collides) {
                dot_dest = {(int)col.collision_point.x-8,(int)col.collision_point.y-8,16,16};
            } else {
                dot_dest = {(int)t.x-8,(int)t.y-8,16,16};
            }
        
            SDL_Rect dot_start = {mpos.x-8,mpos.y-8,16,16};
            SDL_RenderCopy(sdl_renderer,dot,NULL,&dot_start);
            SDL_RenderCopy(sdl_renderer,dot,NULL,&dot_dest);
            SDL_RenderDrawLine(sdl_renderer,dot_start.x+8,dot_start.y+8,dot_dest.x+8,dot_dest.y+8);
            
            }
        */
        /* draft 3 & 4 */
        // so comically slow LMAO
        SDL_Rect dot_start = {mpos.x-8,mpos.y-8,16,16};
        SDL_RenderCopy(sdl_renderer,dot,NULL,&dot_start);

        i32 pt_count=wall_count*4;
        std::vector<v2i> dests;

        for (i32 n=0;n<pt_count+4;n++) {
            v2 t;
            i32 p = n%4;
            if (n>=pt_count) {
                t = p==0?v2(0,0):p==1?v2(1280,0):p==2?v2(0,720):v2(1280,720);
            } else {
                v2i &wall = walls[(i32)floor((float)n/4.f)];
                t = p==0?v2(wall.x,wall.y):p==1?v2(wall.x+1,wall.y):p==2?v2(wall.x,wall.y+1):v2(wall.x+1,wall.y+1);
                t*=64;
            }
            
            intersect_props col = get_collision(mpos,t,walls,wall_count);
            v2i pt;
            if (!col.collides) {
                if (n < pt_count) {
                    printf("ERROR\n");
                }
                pt = t;
            } else {
                pt = v2(col.collision_point.x,col.collision_point.y);
            }

            // two slightly off points
            v2 pt_2 = (convert_angle_to_vec(get_angle_to_point(mpos,t) + 0.005f) * 2000.f) + mpos;
            v2 pt_3 = (convert_angle_to_vec(get_angle_to_point(mpos,t) - 0.005f) * 2000.f) + mpos;
            intersect_props col_2 = get_collision(mpos,pt_2,walls,wall_count);
            intersect_props col_3 = get_collision(mpos,pt_3,walls,wall_count);
            if (col_2.collides==false) {
                dests.push_back(v2(pt_2-mpos).normalize() * 1500.f + mpos);
            } else {
                dests.push_back(col_2.collision_point);
            }
            if (col_3.collides==false) {
                dests.push_back(v2(pt_3-mpos).normalize() * 1500.f + mpos);
            } else {
                dests.push_back(col_3.collision_point);
            }            
            
            dests.push_back(pt);
            
        }

        std::sort(dests.begin(),dests.end(),[mpos](auto &left, auto &right) {
            return get_angle_to_point(mpos,left)<get_angle_to_point(mpos,right);
        });

        for (i32 n=0;n<dests.size();n++) {
            v2i pt=dests[n];
            v2i prev_point=n==0?dests.back():dests[n-1];
            SDL_Vertex vertex_1 = {{(float)mpos.x,(float)mpos.y}, {169,85,75, 255}, {1, 1}};
            SDL_Vertex vertex_2 = {{(float)pt.x,(float)pt.y}, {169,85,75, 255}, {1, 1}};
            SDL_Vertex vertex_3 = {{(float)prev_point.x,(float)prev_point.y}, {169,85,75,255}, {1, 1}};
            SDL_Vertex vertices[] = {vertex_1,vertex_2,vertex_3};
            SDL_RenderGeometry(sdl_renderer, NULL, vertices, 3, NULL, 0);
        }

        /*
        SDL_SetRenderDrawColor(sdl_renderer,255,0,0,255);
        for (i32 n=0; n<dests.size(); n++) {
            v2i pt = dests[n];
            SDL_Rect dot_dest = {pt.x-8,pt.y-8,16,16};
            SDL_RenderCopy(sdl_renderer,dot,NULL,&dot_dest);
            SDL_RenderDrawLine(sdl_renderer,dot_start.x+8,dot_start.y+8,dot_dest.x+8,dot_dest.y+8);
        }
        */
        SDL_RenderPresent(sdl_renderer);

        //SDL_RenderPresent(sdl_renderer);
    }
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
    } else if (argc > 1 && !strcmp(argv[1],"demo")) {
        // demo
        demo();
    } else {
        server();
    }
    return 0;
}
