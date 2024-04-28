
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
                        if (chara==player || chara->curr_state == character::TAKING_DAMAGE) {
                            continue;
                        }
                        //update_player(
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
                printf("Tick %d\n",target_tick);
                if (input_send_timer.get() > input_send_delta) {
                    client_send_input(target_tick);
                    input_send_timer.add(input_send_delta);
                }
            }

            render_game_state(sdl_renderer);
            // gui
            SDL_RenderCopy(sdl_renderer,gui_elements.health_text.texture,NULL,&gui_elements.health_text.get_draw_rect());
            if (player && player->reloading) {
                SDL_RenderCopy(sdl_renderer,gui_elements.reload_text.texture,NULL,&gui_elements.reload_text.get_draw_rect());
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

struct intersect_props {
    v2 collision_point;
    bool collides;
};


double distance_between_points(v2 p1, v2 p2) {
    return (double)sqrt(pow(p2.x-p1.x,2) + pow(p2.y - p1.y,2));
}

float cross_product(v2i a, v2i b) {
    return (float)(a.x * b.y - a.y * b.x);
}

intersect_props get_intersection(v2i ray_start, v2i ray_end, v2i seg_start, v2i seg_end) {
    intersect_props result;
    result.collides = false;

    v2i ray_direction = {ray_end.x - ray_start.x, ray_end.y - ray_start.y};
    v2i seg_direction = {seg_end.x - seg_start.x, seg_end.y - seg_start.y};

    float ray_seg_cross = cross_product(ray_direction, seg_direction);

    if (ray_seg_cross == 0) // Ray and segment are parallel
        return result;

    v2i start_to_start = {seg_start.x - ray_start.x, seg_start.y - ray_start.y};

    float t = cross_product(start_to_start, seg_direction) / ray_seg_cross;
    float u = cross_product(start_to_start, ray_direction) / ray_seg_cross;

    if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
        // Collision detected, calculate collision point
        result.collides = true;
        result.collision_point.x = ray_start.x + t * ray_direction.x;
        result.collision_point.y = ray_start.y + t * ray_direction.y;
    }

    return result;
}

intersect_props get_collision(v2i from, v2i to, v2i *walls, int n) {
    intersect_props res;
    res.collides=false;
    double closest=0.0;
    for (i32 ind=0; ind<n; ind++) {
        v2i *wall=&walls[ind];
        v2i p1 = v2i(wall->x,wall->y)*64;
        v2i p2 = v2i(wall->x+1,wall->y)*64;
        v2i p3 = v2i(wall->x,wall->y+1)*64;
        v2i p4 = v2i(wall->x+1,wall->y+1)*64;
        
        intersect_props col1 = get_intersection(from,to,p1,p2);
        intersect_props col2 = get_intersection(from,to,p1,p3);
        intersect_props col3 = get_intersection(from,to,p2,p4);
        intersect_props col4 = get_intersection(from,to,p3,p4);
        
        if (col1.collides) {
            double c1d = distance_between_points(from,col1.collision_point);
            if (res.collides==false || c1d < closest) {
                res = col1;
                closest = c1d;
            }
        } if (col2.collides) {
            double c2d = distance_between_points(from,col2.collision_point);
            if (res.collides==false || c2d < closest) {
                res = col2;
                closest = c2d;
            }
        } if (col3.collides) {
            double c3d = distance_between_points(from,col3.collision_point);
            if (res.collides==false || c3d < closest) {
                res = col3;
                closest = c3d;
            }
        } if (col4.collides) {
            double c4d = distance_between_points(from,col4.collision_point);
            if (res.collides==false || c4d < closest) {
                res = col4;
                closest = c4d;
            }
        }
    }
    return res;
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
            /*
              float angle = ((PI*2.f) * n) / 40.f;
              v2 t = mpos + convert_angle_to_vec(angle) * 1200.f;
            */
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
