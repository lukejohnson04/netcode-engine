
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
#include <assert.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

#define DEFAULT_PORT 51516
#define RELEASE_BUILD
#define DEFAULT_IP "127.0.0.1"
#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define SDL_MAIN_HANDLED

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include "common.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

SDL_Window *window = nullptr;
SDL_Surface *screenSurface = nullptr;
SDL_Renderer *sdl_renderer=nullptr;
SDL_GLContext glContext;
glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT), 0.0f, -1.0f, 1.0f);

void initialize_systems(const char* winstr, bool vsync, bool init_renderer=true) {
    if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_VIDEO_OPENGL) < 0) {
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

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);  // Use OpenGL 3.x
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);  // Version 3.3, for example
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);    

    window = SDL_CreateWindow(winstr,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              1280,
                              720,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    printf("Created window\n");

    if (window == nullptr) {
        printf("Window could not be created. SDL Error: %s\n", SDL_GetError());
    }

    glContext = SDL_GL_CreateContext(window);

    if (!glContext) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    SDL_GL_MakeCurrent(window, glContext);
    
    glewInit();

    if (init_renderer) {
        int flags = SDL_RENDERER_ACCELERATED;
        if (vsync) flags |= SDL_RENDERER_PRESENTVSYNC;
        sdl_renderer = SDL_CreateRenderer(window, -1, (vsync ? SDL_RENDERER_PRESENTVSYNC : 0) | SDL_RENDERER_ACCELERATED);
        if (sdl_renderer == nullptr) {
            printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
            return;
        }
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLenum err = glGetError();
    
    if (err != GL_NO_ERROR) {
        std::string error;
        switch(err) {
            case GL_INVALID_OPERATION:      error="INVALID_OPERATION";      break;
            case GL_INVALID_ENUM:           error="INVALID_ENUM";           break;
            case GL_INVALID_VALUE:          error="INVALID_VALUE";          break;
            case GL_OUT_OF_MEMORY:          error="OUT_OF_MEMORY";          break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:  error="INVALID_FRAMEBUFFER_OPERATION";  break;
        }
 
        std::cout << "OPENGL ERROR: " << error << std::endl;
    }

    SDL_GL_SetSwapInterval(vsync ? 1 : 0);
    screenSurface = SDL_GetWindowSurface(window);
}

#define MAX_MAP_SIZE 32


#include "render.cpp"
#include "audio.cpp"
#include "text.cpp"
#include "input.cpp"



#ifdef RELEASE_BUILD
#define BUYTIME_LENGTH 10
#define DEFUSE_TIME 8
#define DEFUSE_TIME_WITH_KIT 4
#define ROUND_ENDTIME_LENGTH 6
#else
#define BUYTIME_LENGTH 1
#define DEFUSE_TIME 8
#define DEFUSE_TIME_WITH_KIT 4
#define ROUND_ENDTIME_LENGTH 6
#endif

#define FIRST_ROUND_START_MONEY 800
#define BOMB_TIME_TO_DETONATE 30
#define BOMB_RADIUS 1000
#define BOMB_KILL_RADIUS 350

#define BOMB_PLANT_TIME 3.75
#define BOMB_COUNTDOWN_STARTTIME_BEEPS_PER_SECOND 0.5
#define BOMB_COUNTDOWN_ENDTIME_BEEPS_PER_SECOND 3.0

#include "entity.cpp"
#include "player.cpp"

#define MAX_PLAYERS 16
#define MAX_CLIENTS 16

#include "gamestate.cpp"
#include "network.cpp"
#include "server.cpp"
#include "client.cpp"


static void GameGUIStart() {
    
    char winstr[] = "Client x";
    winstr[7] = (char)client_st.client_id + '0';
    initialize_systems(winstr,true);

    if (window == nullptr) {
        printf("Window could not be created. SDL Error: %s\n", SDL_GetError());
    }
    
    init_textures();
    init_sfx();

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
        generic_drawable money_text;
    } gui_elements;
    gui_elements.health_text = generate_text(m5x7,"100",{255,0,0,255});
    gui_elements.health_text.scale = {2,2};
    gui_elements.health_text.position = {16,720-16-(gui_elements.health_text.get_draw_irect().h)};
    gui_elements.ability_ui_box.gl_texture = gl_textures[TexType::UI_TEXTURE];

    
    gui_elements.money_text = generate_text(m5x7,"$800",{0,255,100,255});
    gui_elements.money_text.scale = {2,2};
    gui_elements.money_text.position = {16,16};
    
    for (i32 ind=0;ind<4;ind++) {
        generic_drawable &sprite=gui_elements.ability_sprites[ind];
        sprite.gl_texture = gl_textures[TexType::ABILITIES_TEXTURE];
        
        sprite.bound = {ind*48,0,48,48};
        sprite.scale = {1.5f,1.5f};
        sprite.position = {1280-16-((4-ind)*sprite.get_draw_irect().w),720-16-sprite.get_draw_irect().h};
    }

    camera_t game_camera;

    // WAY too big its like 8kb lol
    printf("gamestate size: %d\n",(i32)sizeof(game_state));
    
    while (running) {
        // input
        PollEvents(&input,&running);
        character *player=nullptr;
        if (gs.players[client_st.client_id].id != ID_DONT_EXIST) {
            player = &gs.players[client_st.client_id];
        }

        if (client_st.gms.state == GMS::GAME_PLAYING) {
            if (input.just_pressed[SDL_SCANCODE_LEFTBRACKET]) {
                client_st.NetState.interp_delay--;
                // TODO: pull interp delay out of netstate and put it in client_t
                // The server never has an interp delay so it doesn't make sense for
                // it to be a shared variable
                client_st.NetState.interp_delay = MAX(client_st.NetState.interp_delay,2);
                printf("Changed interp delay to %d\n",client_st.NetState.interp_delay);
            } else if (input.just_pressed[SDL_SCANCODE_RIGHTBRACKET]) {
                client_st.NetState.interp_delay++;
                printf("Changed interp delay to %d\n",client_st.NetState.interp_delay);
            }
            
            i32 prev_wall_count=mp.wall_count;
            update_player_controller(player,target_tick,&game_camera);
            
            bool update_health_display=false;
            int p_health_before_update = player ? player->health : 0;
            int p_money_before_update = player ? gs.money[player->id] : 0;

            int o_num=target_tick;
            target_tick = client_st.get_exact_current_server_tick();

            // TODO: make it so the server sends clients snapshots based on the last recieved input
            // e.g. if the server is at time 100 and last client command recieved was at 95, send
            // snapshot for 95 instead of sending each client just the snapshot for 100
            
            // snapshots
            DWORD _merge_snapshots_wait = WaitForSingleObject(client_st._mt_merge_snapshots,0);
            if (_merge_snapshots_wait == WAIT_OBJECT_0) {
                if (client_st._new_merge_snapshot) {
                    client_st.NetState.snapshots.insert(client_st.NetState.snapshots.end(),client_st.merge_snapshots.begin(),client_st.merge_snapshots.end());
                    client_st.merge_snapshots.clear();
                    client_st._new_merge_snapshot=false;
                } else {
                    goto snapshot_merge_finish;
                }

                client_st.last_command_processed_by_server = MAX(client_st._last_proc_tick_buffer,client_st.last_command_processed_by_server);
                for (auto node:client_st.NetState.command_stack) {
                    if (node.verified) continue;
                    if (node.cmd.tick<=client_st.last_command_processed_by_server && node.cmd.tick >= client_st.last_command_processed_by_server-CLIENT_TICK_BUFFER_FOR_SENDING_COMMANDS) {
                        node.verified=true;
                    }
                }

                
                // load the last snapshot we had an input processed on
                
                // load right before the
                // TODO: Keep track of EXACTLY which commands are verified and not.
                // that way, when there's packet loss, we can load the gamestate from
                // our last NON VERIFIED command
                //gs = snapshots.back().gms;
                std::sort(client_st.NetState.snapshots.begin(),client_st.NetState.snapshots.end(),[](auto&left,auto&right) {
                    return left.gms.tick < right.gms.tick;//left.last_processed_command_tick[client_st.client_id] < right.last_processed_command_tick[client_st.client_id];
                });
                gs = client_st.NetState.snapshots.back().gms;
                /*
                for (auto snap=snapshots.end()-1;snap>=snapshots.begin();snap--) {
                    if (snap->gms.tick <= target_tick-5) {
                        gs = snap->gms;
                        break;
                    }
                }
                */
                
        snapshot_merge_finish:
                
                /*for (auto snap=snapshots.begin();snap<snapshots.end();snap++) {
                    if (snap->gms.tick < target_tick - 180) {
                        snap = snapshots.erase(snap);
                        snap--;
                    } else {
                        break;
                    }
                    }*/
                //client_st.NetState.snapshots.clear();
                ReleaseMutex(client_st._mt_merge_snapshots);
            }

            DWORD _merge_chat_wait = WaitForSingleObject(client_st._mt_merge_chat,0);
            if (_merge_chat_wait == WAIT_OBJECT_0) {
                if (client_st.merge_chat.size() > 0) {
                    for (auto entry: client_st.merge_chat) {
                        add_to_chatlog(entry.name,entry.message,entry.tick,&chatlog_display);
                    }
                    client_st.merge_chat.clear();
                }
                ReleaseMutex(client_st._mt_merge_chat);
            }
            
            while(gs.tick<target_tick) {
                gs.update(client_st.NetState,tick_delta);
                gs.tick++;
            }
            
            if (gs.bomb_planted && target_tick >= gs.bomb_planted_tick && target_tick < gs.bomb_planted_tick+BOMB_TIME_TO_DETONATE*60) {
                client_st.bomb_tick_timer -= (target_tick - o_num) * tick_delta;
                if (client_st.bomb_tick_timer <= 0) {
                    play_sfx(SfxType::BOMB_BEEP_SFX);
                    double percentage_of_bomb_left = 1.0 - ((double)(target_tick-gs.bomb_planted_tick)/(double)(BOMB_TIME_TO_DETONATE*60));
                    double time_to_next = 0.1 + 1.0 * percentage_of_bomb_left;
                    time_to_next = MAX(time_to_next,0.15);
                    
                    client_st.bomb_tick_timer += time_to_next;
                }
            }

            for (i32 ind=0; ind<6;ind++) {
                if (input.just_pressed[SDL_SCANCODE_5+ind]) {
                    queue_sound((SfxType)((i32)SfxType::VO_FAVORABLE+ind),client_st.client_id,target_tick);
                }
            }

            if (input.just_pressed[SDL_SCANCODE_MINUS]) {
                queue_sound(SfxType::VO_IM_GOING_TO_KILL_THEM,client_st.client_id,target_tick);
            } if (input.just_pressed[SDL_SCANCODE_EQUALS]) {
                queue_sound(SfxType::VO_UM_ITS_CLOBBER_TIME,client_st.client_id,target_tick);
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
                snapshot_t *prev_snap=nullptr;
                snapshot_t *next_snap=nullptr;
                snapshot_t *exac_snap=nullptr;
                
                if (client_st.NetState.snapshots.back().gms.tick < interp_tick) {
                    printf("Failed interp tick %d: most recent snapshot is %d\n",interp_tick,client_st.NetState.snapshots.back().gms.tick);
                }
                // actually, this whole process of lerping is a little pointless when we have 60 snapshots a second
                // cause we don't actually lerp at all unless there's packet loss... we just set it to one snapshot
                // position and then the next frame set it to the next

                // this whole process of finding interp snapshots only needs to be done once for all players btw
                for (i32 ind=(i32)client_st.NetState.snapshots.size()-1;ind>=0;ind--){
                    snapshot_t *snap = &client_st.NetState.snapshots[ind];
                
                    if (snap->gms.tick == interp_tick) {
                        exac_snap = snap;
                        break;
                    } else if (snap->gms.tick>interp_tick) {
                        next_snap = snap;
                
                    } else if (snap->gms.tick<interp_tick) {
                        prev_snap = snap;
                        break;
                    }
                }
                if (!exac_snap && (!next_snap || !prev_snap)){
                    printf("No snapshots to interpolate between\n");
                } else {
                    FORn(gs.players,gs.player_count,chara) {
                        if (chara==player) {
                            continue;
                        } else if (chara->id == player->id || chara->id == client_st.client_id) {
                            printf("WHATHTEFUCKCKJKCKCJ\n");
                        }
                        if (chara->curr_state==character::TAKING_DAMAGE) {
                            continue;
                        }

                        if (exac_snap) {
                            chara->pos = exac_snap->gms.players[chara->id].pos;
                        } else if (next_snap && prev_snap) {
                            v2 prev_pos = prev_snap->gms.players[chara->id].pos;
                            v2 next_pos = next_snap->gms.players[chara->id].pos;
                            float f = (float)(interp_tick-prev_snap->gms.tick) / (float)(next_snap->gms.tick-prev_snap->gms.tick);
                            chara->pos = lerp(prev_pos,next_pos,f);
                        }
                    }
                }

                for (auto event=sound_queue.begin(); event<sound_queue.end(); event++) {
                    // delete old
                    if (event->tick < target_tick - 120) {
                        event = sound_queue.erase(event);
                        event--;
                        continue;
                    }
                    // dont play a command twice
                    if (event->played) continue;
                    i32 t = event->tick;
                    if (event->id != (entity_id)client_st.client_id) {
                        t -= client_st.NetState.interp_delay;
                    }
                    if (t <= target_tick) {
                        play_sfx(event->type);
                        event->played=true;
                    }
                }
            }
            client_st.last_tick_processed=target_tick;

            if (o_num != target_tick) {
                //printf("Tick %d\n",target_tick);
                if (input_send_timer.get() > input_send_delta) {
                    client_send_input(target_tick);
                    input_send_timer.add(input_send_delta);
                }
            }
            
            if (client_st.loaded_new_map) {
                client_st.loaded_new_map=false;
                // regenerate raycast points
                auto &rps = client_sided_render_geometry.raycast_points;
                rps.clear();
                client_sided_render_geometry.segments.clear();
                for (i32 n=0; n<mp.wall_count; n++) {
                    v2i wall=mp.walls[n];
                    rps.push_back(v2(wall.x,wall.y)*64);
                    rps.push_back(v2(wall.x+1,wall.y)*64);
                    rps.push_back(v2(wall.x,wall.y+1)*64);
                    rps.push_back(v2(wall.x+1,wall.y+1)*64);
                }
                printf("Points before: %d\n",(i32)rps.size());
                std::sort( rps.begin(), rps.end(), [](auto&left,auto&right){
                    return *(double*)&left < *(double*)&right;
                });
                std::unordered_map<double,int> p_set;
                for (auto &p:rps) p_set[*(double*)&p]++;

                // removes all points surrounded by tiles
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
                
                for (i32 ind=0; ind<mp.wall_count; ind++) {
                    v2 wall=mp.walls[ind];
                    v2 p1 = v2(wall.x,wall.y)*64;
                    v2 p2 = v2(wall.x+1,wall.y)*64;
                    v2 p3 = v2(wall.x,wall.y+1)*64;
                    v2 p4 = v2(wall.x+1,wall.y+1)*64;

                    bool fp1 = p_set[*(double*)&p1] != 4;
                    bool fp2 = p_set[*(double*)&p2] != 4;
                    bool fp3 = p_set[*(double*)&p3] != 4;
                    bool fp4 = p_set[*(double*)&p4] != 4;

                    if (fp1) {
                        if (fp2)
                            client_sided_render_geometry.segments.push_back({p1,p2});
                        if (fp3)
                            client_sided_render_geometry.segments.push_back({p1,p3});
                    } if (fp4) {
                        if (fp2)
                            client_sided_render_geometry.segments.push_back({p2,p4});
                        if (fp3)
                            client_sided_render_geometry.segments.push_back({p3,p4});
                    }
                }
                
                std::sort( rps.begin(), rps.end(), [](auto&left,auto&right){
                    return *(double*)&left < *(double*)&right;
                });

                rps.erase(unique(rps.begin(),rps.end()),rps.end());
                
                printf("Points after: %d\n",(i32)rps.size());

                std::sort( client_sided_render_geometry.segments.begin(), client_sided_render_geometry.segments.end(), [](auto&left,auto&right){
                    return (*(double*)&left.p1 + *(double*)&left.p2) < (*(double*)&right.p1 + *(double*)&right.p2);
                });
                i32 s = (i32)client_sided_render_geometry.segments.size();
                client_sided_render_geometry.segments.erase(unique(client_sided_render_geometry.segments.begin(),client_sided_render_geometry.segments.end()),client_sided_render_geometry.segments.end());
                printf("Removed %d segments\n",(i32)client_sided_render_geometry.segments.size()-s);
            }

            
            if (player) {
                game_camera.follow = player;
                game_camera.pos = game_camera.follow->pos;
            } else {
                game_camera.pos = {1280/2,720/2};
            }

            
            // DRAW BEGIN
            if (player) {
                if (player->health != p_health_before_update) {
                    gui_elements.health_text = generate_text(m5x7,std::to_string(player->health),{255,0,0,255});
                    gui_elements.health_text.scale = {2,2};
                    gui_elements.health_text.position = {16,720-16-(gui_elements.health_text.get_draw_irect().h)};
                } if (player->reloading) {
                    std::string reload_str = std::to_string(player->reload_timer);
                    if (reload_str.size() > 3) {
                        reload_str.erase(reload_str.begin()+3,reload_str.end());
                    }
                    gui_elements.reload_text = generate_text(m5x7,reload_str,{255,0,0,255});
                    gui_elements.reload_text.position = player->pos + v2i(16-8,48);
                    gui_elements.reload_text.position += v2(1280/2,720/2) - game_camera.pos;
                }
                // draw money
                if (gs.money[player->id] != p_money_before_update) {
                    gui_elements.money_text = generate_text(m5x7,"$" + std::to_string(gs.money[player->id])+"huh",{0,255,100,255});
                    gui_elements.money_text.scale = {2,2};
                    gui_elements.money_text.position = {16,16};
                }
            }

            render_game_state(player,&game_camera);
            glUseProgram(sh_textureProgram);
            // gui
            GL_DrawTexture(gui_elements.money_text.gl_texture,gui_elements.money_text.get_draw_irect());
            GL_DrawTexture(gui_elements.health_text.gl_texture,gui_elements.health_text.get_draw_irect());
            if (player) {
                if (player->reloading) {
                    GL_DrawTexture(gui_elements.reload_text.gl_texture,gui_elements.reload_text.get_draw_irect());
                }
            }

            /*
            // render the score at the end of the round
            if (gs.one_remaining_tick!=0 && gs.tick > gs.one_remaining_tick) {
                std::string left_str = std::to_string(gs.score[0]);
                std::string right_str = std::to_string(gs.score[1]);
                generic_drawable middle_text = generate_text(m5x7,"-",{0,0,0,255});
                generic_drawable left_text = generate_text(m5x7,left_str,*(SDL_Color*)&gs.players[0].color);
                generic_drawable right_text = generate_text(m5x7,right_str,*(SDL_Color*)&gs.players[1].color);
                
                middle_text.scale = {16,16};
                left_text.scale = {16,16};
                right_text.scale = {16,16};
                middle_text.position = {1280/2 - middle_text.get_draw_rect().w/2,720/2+middle_text.get_draw_rect().h/2};
                left_text.position = {middle_text.position.x - left_text.get_draw_rect().w-16,middle_text.position.y};
                right_text.position = {middle_text.position.x + middle_text.get_draw_rect().w+16,middle_text.position.y};

                SDL_Rect left_rect,middle_rect,right_rect;
                left_rect = left_text.get_draw_rect();
                middle_rect = middle_text.get_draw_rect();
                right_rect = right_text.get_draw_rect();
                SDL_RenderCopy(sdl_renderer,left_text.texture,NULL,&left_rect);
                SDL_RenderCopy(sdl_renderer,middle_text.texture,NULL,&middle_rect);
                SDL_RenderCopy(sdl_renderer,right_text.texture,NULL,&right_rect);
            }

            
            if (player) {
                // draw abilities
                generic_drawable cooldown_text;
                std::string cooldown_str;
                for (i32 ind=0;ind<4;ind++) {
                    generic_drawable *sprite=&gui_elements.ability_sprites[ind];

                    // render the box
                    SDL_Rect dest = sprite->get_draw_rect();
                    SDL_Rect sp_rect = sprite->get_draw_rect();
                    SDL_RenderCopy(sdl_renderer,textures[TexType::UI_TEXTURE],NULL,&dest);
                    SDL_RenderCopy(sdl_renderer,sprite->texture,(SDL_Rect*)&sprite->bound,&sp_rect);
                    if (ind==0 && player->invisibility_cooldown > 0) {
                        render_ability_sprite(sprite,player->invisibility_cooldown);
                    } else if (ind == 1 && player->shield_cooldown > 0) {
                        render_ability_sprite(sprite,player->shield_cooldown);
                    } else if (ind == 2 && player->fireburst_cooldown > 0) {
                        render_ability_sprite(sprite,player->fireburst_cooldown);
                    } else if (ind == 3 && player->reloading) {
                        render_ability_sprite(sprite,player->reload_timer);
                    }
                }
                local_persist i32 ammo_cnt = 0;
                if (player->bullets_in_mag != ammo_cnt) {
                    ammo_cnt = player->bullets_in_mag;
                    std::string ammo_str = std::to_string(ammo_cnt);
                    gui_elements.magazine_cnt_text = generate_text(m5x7,ammo_str,{255,255,255,255});
                    gui_elements.magazine_cnt_text.scale = {4,4};
                    auto ab_pos = gui_elements.ability_sprites[3].get_draw_rect();
                    gui_elements.magazine_cnt_text.position = {ab_pos.x+24-gui_elements.magazine_cnt_text.get_draw_rect().w/2,ab_pos.y+16};
                }
                SDL_Rect ammo_rect = gui_elements.magazine_cnt_text.get_draw_rect();
                SDL_RenderCopy(sdl_renderer,gui_elements.magazine_cnt_text.texture,NULL,&ammo_rect);
            }
            */
            // buy menu
            if (gs.round_state == ROUND_BUYTIME) {
                local_persist bool is_buy_menu_open = false;
                if (input.just_pressed[SDL_SCANCODE_B]) {
                    is_buy_menu_open = !is_buy_menu_open;
                } if (is_buy_menu_open) {
                    i32 width = 800;
                    i32 height = 620;
                    iRect dest = {1280/2 - width/2, 720/2 - height/2 + 40,width,height};
                    GL_DrawTexture(gl_textures[BUY_MENU_TEXTURE],dest);
                    //SDL_RenderCopy(sdl_renderer,textures[BUY_MENU_TEXTURE],NULL,&dest);
                }
            }
            

            local_persist bool chatbox_isopen=false;
            local_persist std::string chatbox_text="";
            if (chatbox_isopen == false && input.just_pressed[SDL_SCANCODE_T]) {
                chatbox_isopen = true;
                SDL_StartTextInput();
                input.text_input_captured=true;
                input.input_field = &chatbox_text;
                input.text_modified=true;

            } else if (chatbox_isopen && input.just_pressed[SDL_SCANCODE_ESCAPE]) {
                chatbox_isopen=false;
                SDL_StopTextInput();
                input.text_input_captured=false;
                input.input_field = nullptr;
            }

            if (chatbox_isopen) {
                // if you press the enter key, submit that field!
                iRect text_input_rect = {48,500+40,400,36};
                glUseProgram(sh_colorProgram);
                GL_DrawRect(text_input_rect,{255,255,255,100});

                local_persist generic_drawable chatbox_entry;
                if (input.text_submitted) {
                    // send to server
                    packet_t p = {};
                    p.type = CHAT_MESSAGE;

                    strcpy_s(p.data.chat_message.message,MAX_CHAT_MESSAGE_LENGTH,chatbox_text.c_str());
                    strcpy_s(p.data.chat_message.name,MAX_USERNAME_LENGTH,client_st.username.c_str());
                    // ought to put this in the client tick code? idk man. doesn't matter too much tho
                    send_packet(client_st.socket,&client_st.servaddr,&p);
                    //add_to_chatlog("Luke","Hello",target_tick,&chatlog_display);
                    //add_to_chatlog("Luke","Hello",target_tick,&chatlog_display);

                    //add_to_chatlog("Luke",chatbox_text,target_tick,&chatlog_display);
                    chatbox_text="";
                    chatbox_isopen=false;
                    
                    SDL_StopTextInput();
                    input.text_submitted = false;
                    input.text_input_captured=false;
                    input.input_field = nullptr;
                } else if (input.text_modified) {
                    
                    chatbox_entry = generate_text(m5x7,chatbox_text==""?" ":chatbox_text,{255,255,255,255},chatbox_entry.gl_texture);
                    chatbox_entry.scale = {1,1};
                    chatbox_entry.position = {text_input_rect.x + 4, text_input_rect.y + text_input_rect.h - chatbox_entry.get_draw_irect().h-4};
                }

                if (chatbox_entry.gl_texture) {
                    glUseProgram(sh_textureProgram);
                    GL_DrawTexture(chatbox_entry.gl_texture,chatbox_entry.get_draw_irect());
                }
            }

            glUseProgram(sh_textureProgram);
            /*
            GLuint colUni = glGetUniformLocation(sh_textureProgram, "color");
            glUniform4f(colUni,255,255,255,255);
            */
            for (i32 ind=chatlog.entry_count-1;ind>=0;ind--) {
                i32 fade_start_tick = chatlog.tick_added[ind]+(CHAT_MESSAGE_DISPLAY_TIME*60);
                i32 fade_end_tick = fade_start_tick + (CHAT_FADE_LEN * 60);
                if (true || target_tick < fade_end_tick) {
                    if (false && target_tick > fade_start_tick) {
                        // fade out
                        u8 a = 255 - (u8)(((double)(target_tick - fade_start_tick) / (double)(fade_end_tick - fade_start_tick)) * 255.0);
                        //glUniform4f(colUni,255,255,255,a);
                    } else {
                        //glUniform4f(colUni,255,255,255,255);
                    }
                    iRect dest = chatlog_display.sprites[ind].get_draw_irect();
                    // TODO: why TF is this width and height constantly stuck at 0???
                    GL_DrawTexture(chatlog_display.sprites[ind].gl_texture,dest);
                }
            }
            
            // visualize net data
            local_persist bool visualize_netgraph=true;
            if (input.just_pressed[SDL_SCANCODE_GRAVE]) {
                visualize_netgraph = !visualize_netgraph;
            }
            if (visualize_netgraph) {
                
                i32 end_tick = target_tick + 10;
                i32 interp_tick = target_tick-client_st.NetState.interp_delay;
                i32 begin_tick = end_tick-60;

                i32 last_proc_tick=client_st.last_command_processed_by_server;

                iRect graph_box_rect = {16,16,880,100};
                int padding=20;
                glUseProgram(sh_colorProgram);
                GL_DrawRect(graph_box_rect,{255,255,255,255});
                /*
                SDL_SetRenderDrawColor(sdl_renderer,255,255,255,255);
                SDL_RenderFillRect(sdl_renderer,&graph_box_rect);
                */
                //SDL_SetRenderDrawColor(sdl_renderer,0,0,0,255);
                
                i32 timeline_x = graph_box_rect.x;
                i32 timeline_y = graph_box_rect.y + graph_box_rect.h/2;
                i32 timeline_w = graph_box_rect.w;
                i32 timeline_h = 3;

                // scuffed ah line thickness work around lol
                GL_DrawRect({timeline_x,timeline_y-1,timeline_w,timeline_h},{0,0,0,255});

                for (i32 ind=begin_tick;ind<end_tick;ind++) {
                    const int stride = (graph_box_rect.w-padding) / 60;
                    i32 xpos = (ind-begin_tick) * stride + padding/2;
                    v2i p1 = {graph_box_rect.x + xpos,timeline_y-3};
                    v2i p2 = {graph_box_rect.x + xpos,timeline_y+3};
                    Color col = {0,0,0,255};
                    if (ind == target_tick) {
                        p1.y -= 16;
                        p2.y += 16;
                        col = {255,0,0,255};
                    } else if (ind == interp_tick) {
                        p1.y -= 8;
                        p2.y += 8;
                        col = {0,0,255,255};
                    }
                    i32 count=0;
                    for (auto &snap:client_st.NetState.snapshots) {
                        if (snap.gms.tick==ind) {
                            count++;
                        }
                    }
                    if (count) {
                        if (last_proc_tick > ind) {
                            col = {0,255,0,255};
                        } else {
                            col = {0,255,0,255};
                        }
                        iRect rect = {p1.x-(stride/2),timeline_y-8,stride,16};
                        // draw a rectangle
                        GL_DrawRect(rect,col);
                        // TODO: draw black rectangle outline here!!!
                        //SDL_SetRenderDrawColor(sdl_renderer,0,0,0,255);
                        //SDL_RenderDrawRect(sdl_renderer,&r);
                    }
                    GL_DrawRect({p1.x,p1.y,p2.x-p1.x,p2.y-p1.y+2},col);
                    // TODO: draw these lines!! important!!
                    /*SDL_RenderDrawLine(sdl_renderer,p1.x,p1.y,p2.x,p2.y);
                    SDL_RenderDrawLine(sdl_renderer,p1.x+1,p1.y,p2.x+1,p2.y);
                    */
                }

                std::string curr_tick_str = "Tick: " + std::to_string(target_tick);
                std::string last_input_processed_by_server_str = "Last processed input: " + std::to_string(last_proc_tick);
                local_persist generic_drawable curr_tick;
                curr_tick = generate_text(m5x7,curr_tick_str, {0,255,0,255}, curr_tick.gl_texture);
                curr_tick.position = {graph_box_rect.x, graph_box_rect.y + graph_box_rect.h+4};
                //curr_tick.scale = {2,2};

                Color col = {255,0,0,255};
                if (last_proc_tick > target_tick) {
                    col = {0,255,0,255};
                }
                local_persist generic_drawable last_proc;
                last_proc = generate_text(m5x7,last_input_processed_by_server_str, col, last_proc.gl_texture);
                last_proc.position = curr_tick.position + v2(0,curr_tick.get_draw_irect().h + 4);
                //last_proc.scale = {2,2};

                glUseProgram(sh_textureProgram);
                GL_DrawTexture(curr_tick.gl_texture,curr_tick.get_draw_irect());
                GL_DrawTexture(last_proc.gl_texture,last_proc.get_draw_irect());
                
            }
        } else if (client_st.gms.state == GMS::PREGAME_SCREEN) {
            // if the clock runs out, start the game
            double time_to_start=0.0;
            if (client_st.gms.counting_down_to_game_start) {
                if (client_st.gms.game_start_time.QuadPart - client_st.sync_timer.get_high_res_elapsed().QuadPart <= 0) {
                    // game_start
                    client_st.gms.state = GMS::GAME_PLAYING;
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

            render_pregame_screen(client_st.gms,time_to_start);
            // render player color selector
            /*SDL_Rect p_rect = {0,0,32,32};
            SDL_Rect dest_rect = {200,500,64,64};
            SDL_RenderCopy(sdl_renderer,textures[TexType::PLAYER_TEXTURE],&p_rect,&dest_rect);
            */
        }

endof_frame:
        SDL_GL_SwapWindow(window);
        
        //SDL_RenderPresent(sdl_renderer);
        
        // sleep until next tick

        // when you don't sleep, it works fine?!?!
        
    }
    
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    TTF_Quit();
    IMG_Quit();
    Mix_Quit();
}

static void demo() {
    initialize_systems("Demo",true,false);

    init_textures();

    bool running=true;

    i32 wall_count=0;
    v2i walls[32];

    for (i32 n=0;n<4;n++) {
        walls[wall_count++] = {12,n+4};
    }
    walls[wall_count++] = {0,1};
    walls[wall_count++] = {4,4};
    walls[wall_count++] = {4,5};
    walls[wall_count++] = {6,2};
    walls[wall_count++] = {9,7};
    walls[wall_count++] = {15,9};

    glUseProgram(sh_textureProgram);
    glUseProgram(0);


    GLuint bg_texture;
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenTextures(1,&bg_texture);
    glBindTexture(GL_TEXTURE_2D,bg_texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D,0);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Error! Framebuffer is not complete!" << std::endl;
    }

    // temporarily flip the projection for the framebuffer since their y axis is flipped automatically 


    glUseProgram(sh_colorProgram);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (i32 ind=0; ind<wall_count; ind++) {
        
        v2 wall=walls[ind];
        v2 p1 = v2(wall.x,wall.y)*64;
        v2 p2 = v2(wall.x+1,wall.y)*64;
        v2 p3 = v2(wall.x,wall.y+1)*64;
        v2 p4 = v2(wall.x+1,wall.y+1)*64;

        client_sided_render_geometry.segments.push_back({p1,p2});
        client_sided_render_geometry.segments.push_back({p1,p3});
        client_sided_render_geometry.segments.push_back({p2,p4});
        client_sided_render_geometry.segments.push_back({p3,p4});

        GL_DrawRect({(i32)wall.x*64,(i32)wall.y*64,64,64},COLOR_BLACK);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDeleteFramebuffers(1,&framebuffer);
    glBindTexture(GL_TEXTURE_2D,0);

    GLuint dot_tex = GL_load_texture("res/flintlock.png");
    clock_t start_time = clock();

    GLuint shadow_VAO,shadow_VBO;
    glGenBuffers(1,&shadow_VBO);
    glGenVertexArrays(1,&shadow_VAO);
    generic_drawable text_drawable = generate_text(m5x7,"hello",{255,0,255,255});
    text_drawable.position = {500,500};
    
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error: " << err << std::endl;
    }

    /*
    int w, h;
    int miplevel = 0;
    glBindTexture(GL_TEXTURE_2D,text_tex);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_HEIGHT, &h);
    //std::cout << std::endl << temp_->w << ", " << temp_surf->h << std::endl;
    std::cout << w << ", " << h << std::endl;
    glBindTexture(GL_TEXTURE_2D,0);
    */

    GLuint test_mask_texture,test_world_texture;
    glGenTextures(1,&test_mask_texture);
    glGenTextures(1,&test_world_texture);
    GL_load_texture(test_mask_texture,"res/test_mask.png");

    /*
    glBindTexture(GL_TEXTURE_2D,test_world_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D,0);
    */
    GLuint test_shadow_texture,test_objects_texture;
    glGenTextures(1,&test_shadow_texture);
    glGenTextures(1,&test_objects_texture);

    GL_load_texture_for_framebuffer(test_world_texture);
    GL_load_texture_for_framebuffer(test_shadow_texture);
    GL_load_texture_for_framebuffer(test_objects_texture);
    GLuint test_fb=GL_create_framebuffer(test_world_texture);
    GLuint shadow_fb=GL_create_framebuffer(test_shadow_texture);
    GLuint objects_fb=GL_create_framebuffer(test_objects_texture);
    /*
    glGenFramebuffers(1,&test_fb);
    glBindFramebuffer(GL_FRAMEBUFFER,test_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, test_world_texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    */

    while (running) {
        // input
        PollEvents(&input,&running);
        clock_t curr_time = clock();
        double elapsed = (double)(curr_time - start_time) / CLOCKS_PER_SEC;
        
        double sin_val = (sin(elapsed)+1)/2;
        double cos_val = (cos(elapsed)+1)/2;
        double tan_val = (tan(elapsed)+1)/2;
        Color clear_col = Color((u8)(sin_val*255.f),(u8)(cos_val*255.f),255,255);//(u8)(tan_val*255.f),255);

        glBindFramebuffer(GL_FRAMEBUFFER,test_fb);
        glClearColor(clear_col.r/255.f, clear_col.g/255.f, clear_col.b/255.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(sh_colorProgram);

        // Draw the rectangle
        

        
        /*FORn(walls,wall_count,wall) {
            GL_DrawRect({wall->x*64,wall->y*64,64,64},col);
        }
        */
        
        glUseProgram(sh_textureProgram);
        GL_DrawTextureEx(bg_texture,{0,0,0,0},{0,0,0,0},false,true);


        v2i mpos = get_mouse_position();
        //glUseProgram(sh_textureProgram);
        GL_DrawTexture(dot_tex,{mpos.x-16,mpos.y-16,32,32});
        
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
        //SDL_RenderCopy(sdl_renderer,dot,NULL,&dot_start);

        i32 pt_count=wall_count*4;
        std::vector<v2i> dests;

        for (i32 n=0;n<pt_count+4;n++) {
            v2 t;
            i32 p = n%4;
            if (n>=pt_count) {
                t = p==0?v2(0,0):p==1?v2(1280,0):p==2?v2(0,720):v2(1280,720);
            } else {
                v2i wall = walls[(i32)floor((float)n/4.f)];
                t = p==0?v2(wall.x,wall.y):p==1?v2(wall.x+1,wall.y):p==2?v2(wall.x,wall.y+1):v2(wall.x+1,wall.y+1);
                t*=64;
            }
            
            intersect_props col = get_collision(mpos,t,client_sided_render_geometry.segments);
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
            intersect_props col_2 = get_collision(mpos,pt_2,client_sided_render_geometry.segments);
            intersect_props col_3 = get_collision(mpos,pt_3,client_sided_render_geometry.segments);
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

        glBindFramebuffer(GL_FRAMEBUFFER,objects_fb);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(sh_textureProgram);
        GL_DrawTextureEx(gl_textures[PLAYER_TEXTURE], {300,400,64,64}, {0,0,32,32}, false, false, (float)elapsed);//,{300,600,64,64},{0,0,32,32},true);
        GL_DrawTexture(text_drawable.gl_texture,text_drawable.get_draw_irect());

        glBindFramebuffer(GL_FRAMEBUFFER,shadow_fb);
        glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(sh_colorProgram);
        glUniform4f(glGetUniformLocation(sh_colorProgram,"color"),1.0f,1.0f,1.0f,1.0f);
        
        for (i32 n=0;n<dests.size();n++) {
            v2i pt=dests[n];
            v2i prev_point=n==0?dests.back():dests[n-1];
            float vertices[] = {
                (float)mpos.x,(float)mpos.y, 0.0f,
                (float)pt.x,  (float)pt.y,   0.0f,
                (float)prev_point.x, (float)prev_point.y, 0.0f
            };
            glBindVertexArray(shadow_VAO);
            glBindBuffer(GL_ARRAY_BUFFER,shadow_VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3 * sizeof(float),0);
            glEnableVertexAttribArray(0);

            glDrawArrays(GL_TRIANGLES,0,3);
            glBindBuffer(GL_ARRAY_BUFFER,0);
            glBindVertexArray(0);
        }


        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glUseProgram(sh_textureProgram);
        GL_DrawTextureEx(test_world_texture,{0,0,0,0},{0,0,0,0},false,true);

        glUseProgram(sh_modProgram);
        GLuint tex1_loc = glGetUniformLocation(sh_modProgram, "_texture1");
        GLuint tex2_loc = glGetUniformLocation(sh_modProgram, "_texture2");
        
        glActiveTexture(GL_TEXTURE0); // Texture unit 0
        glBindTexture(GL_TEXTURE_2D, test_shadow_texture);
        glUniform1i(tex1_loc, 0);
        glActiveTexture(GL_TEXTURE1); // Texture unit 1
        glBindTexture(GL_TEXTURE_2D, test_objects_texture);
        glUniform1i(tex2_loc, 1);

        float vertices[] = {
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            1280.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            1280.0f, 720.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
        };
        glm::mat4 model = glm::mat4(1.0f);
        GLint transformLoc = glGetUniformLocation(sh_modProgram,"model");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(gl_varrays[TEXTURE_VAO]);
        glBindBuffer(GL_ARRAY_BUFFER, gl_vbuffers[TEXTURE_VBO]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glBindVertexArray(gl_varrays[TEXTURE_VAO]);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(0));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER,0);    
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);



        //GL_DrawTextureEx(gl_textures[GAME_WORLD_TEXTURE],{0,0,1280,720},{0,0,1280,720},false,true);
        //GL_DrawTexture(test_mask_texture);
        

        //glUniformMatrix4fv(glGetUniformLocation(sh_textureProgram, "transform"), 1, GL_FALSE, glm::value_ptr(transform));
        
        /*
        SDL_SetRenderDrawColor(sdl_renderer,255,0,0,255);
        for (i32 n=0; n<dests.size(); n++) {
            v2i pt = dests[n];
            SDL_Rect dot_dest = {pt.x-8,pt.y-8,16,16};
            SDL_RenderCopy(sdl_renderer,dot,NULL,&dot_dest);
            SDL_RenderDrawLine(sdl_renderer,dot_start.x+8,dot_start.y+8,dot_dest.x+8,dot_dest.y+8);
        }
        */
        SDL_GL_SwapWindow(window);
        //SDL_RenderPresent(sdl_renderer);

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
        /*if (argc < 2) {
            fprintf(stderr, "not enough args!");
            return -1;
        }
        */
        if (argc >= 3) {
            sscanf_s(argv[2], "%d", &port);
        } if (argc >= 4) {
            ip_addr = std::string(argv[3]);
        }
        client_connect(port,ip_addr);
    } else if (argc > 1 && !strcmp(argv[1],"demo")) {
        // demo
        demo();
    } else {
        if (argc > 1) {
            int port = DEFAULT_PORT;
            if (argc > 1) {
                sscanf_s(argv[1],"%d",&port);
            }
            server(port);
            goto cleanup;
        }
#ifdef RELEASE_BUILD
        std::string res;
        printf("Server or client? (leave empty for server) ");
        getline(std::cin,res);
        if (res == "") {
            int port = DEFAULT_PORT;
            printf("Port number? (leave empty for default) ");
            std::string p_num;
            getline(std::cin,p_num);
            if (p_num != "") {
                port = std::stoi(p_num);
            }
            server(port);
        } else {
            std::string ip_addr=DEFAULT_IP;
            printf("Ip address? (leave empty for 127.0.0.1) ");
            std::string ip_input;
            getline(std::cin,ip_input);
            if (ip_input != "") {
                ip_addr = ip_input;
            }
            int port = DEFAULT_PORT;
            printf("Port number? (leave empty for default) ");
            std::string p_num;
            getline(std::cin,p_num);
            if (p_num != "") {
                port = std::stoi(p_num);
            }
            client_connect(port,ip_addr);
        }
#else
        int port = DEFAULT_PORT;
        if (argc > 1) {
            sscanf_s(argv[1],"%d",&port);
        }
        server(port);
#endif
    }
cleanup:
    return 0;
}
