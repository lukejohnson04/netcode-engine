
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <time.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>

#include <vector>

#include "common.h"

#define DEFAULT_PORT 1250
#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

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

    bool running=true;

    const float FRAME_TIME = (1.f/60.f);
    const int FRAME_TIME_MS = (1000/60);
    
    int TICK_TIME = (int)((1.f/(float)client_st.server_header.tickrate) * 1000.f);
    u64 last_tick_time = SDL_GetTicks64();
    u64 last_frame_time = last_tick_time;
    
    client_st.init_ok=true;
    gs.time = client_st.server_time_on_connection;

    while (running) {
        PollEvents(&input,&running);
        u64 local_time = SDL_GetTicks64();
        int delta = local_time - last_frame_time;
        int s_time = client_time_to_server_time(local_time,client_st);
        
        if (client_st.game_data.player_id != ID_DONT_EXIST) {
            character *player = &gs.players[client_st.game_data.player_id];
            update_player_controller(player,s_time);
            //update_player(&gs.players[client_st.game_data.player_id],(frame_time-last_frame_time)/1000.f);
        }
        //rewind_game_state(gs,client_st.NetState,frame_time-2000);
        update_game_state(gs,client_st.NetState,s_time);

        
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

        {
            u32 timer = local_time - last_tick_time;

            if (timer > TICK_TIME) {
                last_tick_time = local_time;
                client_on_tick();
            }

            u64 curr = SDL_GetTicks64();
            if (local_time+FRAME_TIME_MS > curr) {
                Sleep((local_time+FRAME_TIME_MS)-curr);
            }
        }

        last_frame_time = local_time;
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
            sscanf(argv[2], "%d", &port);
        }

        client_connect(port);
    } else {
        server();
    }
    return 0;
}
