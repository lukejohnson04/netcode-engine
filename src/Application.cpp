
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <time.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#include <vector>
#include <algorithm>
#include <iostream>

#include "common.h"

#define DEFAULT_PORT 1250
#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include "render.cpp"
#include "input.cpp"
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

    window = SDL_CreateWindow("Main",
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

    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (sdl_renderer == nullptr) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    screenSurface = SDL_GetWindowSurface(window);
    init_textures(sdl_renderer);

    bool running=true;

    gs.time = client_st.server_time_on_connection;
    const double FRAME_TIME = (1.0/60.0);
    const int FRAME_TIME_MS = (1000/60);
    
    const double TICK_TIME = (1.0/(double)client_st.server_header.tickrate);
    double start_time = client_st.sync_timer.get();
    double last_tick_time = start_time;
    double last_frame_time = start_time;
    // fix this time shit lol
    
    client_st.init_ok=true;

    double interp_delay = 0.05;

    while (running) {
        PollEvents(&input,&running);
        double local_time = client_st.sync_timer.get();
        double delta = local_time - last_frame_time;
        double s_time = client_time_to_server_time(client_st);
        double interp_time = s_time - interp_delay;

        character *player=nullptr;
        
        if (client_st.game_data.player_id != ID_DONT_EXIST) {
            player = &gs.players[client_st.game_data.player_id];
            update_player_controller(player,s_time);
            //update_player(player,delta);
        }

        // take the list of the whole command stack, including the unsent client commands
        // for each command, update the game state up to that point.
        
        // we want to always be showing the game state from interp_delay milliseconds in the past
        // the one exception is the player for the client who we want to be showing in the present,
        // predicting that the client input will go through without a hitch

        // rewind player to current gamestate time
        std::vector<command_t> full_stack;
        {
            std::vector<command_t> &cst=client_st.NetState.command_stack;
            std::vector<command_t> &cbf=client_st.NetState.command_buffer;
            full_stack.insert(full_stack.begin(),cst.begin(),cst.end());
            full_stack.insert(full_stack.end(),cbf.begin(),cbf.end());
            std::sort(full_stack.begin(),full_stack.end(),command_sort_by_time());
        }

        if (player) {
            rewind_player(player,gs.time,full_stack);
        }

        // bring gamestate to interpolation time
        update_game_state(gs,client_st.NetState,interp_time);
        // update player to prediction time
        if (player) {
            fast_forward_player(player,s_time,full_stack);
        }

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

        {
            double next_frame_time = last_frame_time + FRAME_TIME;
            double next_tick_time = last_tick_time + TICK_TIME;
            double curr = client_st.sync_timer.get();

            if (curr > next_tick_time) {
                last_tick_time = next_tick_time;
                client_on_tick();
                /*
                static int tick=0;
                tick++;
                printf("%d\n",tick);
                */
            }

            if (next_frame_time > curr) {
                Sleep((DWORD)((next_frame_time - curr)*1000));
            }
            last_frame_time = local_time;
        }
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
        if (argc < 2) {
            fprintf(stderr, "not enough args!");
            return -1;
        } if (argc == 3) {
            sscanf_s(argv[2], "%d", &port);
        }

        client_connect(port);
    } else {
        server();
    }
    return 0;
}
