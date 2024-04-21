

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

    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (sdl_renderer == nullptr) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    screenSurface = SDL_GetWindowSurface(window);
    init_textures(sdl_renderer);

    bool running=true;

    gs.time = client_st.server_time_on_connection;
    double start_time = client_st.sync_timer.get();
    // fix this time shit lol
    
    client_st.init_ok=true;

    int input_send_hz=30;
    int tick_hz=60;
    int frame_hz=0; // uncapped framerate
    double tick_delta=(1.0/tick_hz);
    double input_send_delta = (1.0/input_send_hz);

    // have server send next tick time
    // currently we have the server time and the last tick
    // but we aren't sure how long ago the last tick was

    timer_t global_timer;
    global_timer.Start();

    r_clock_t tick_clock(global_timer);
    r_clock_t input_send_clock(global_timer);
    tick_clock.start_time -= client_st.sync_timer.get() - client_st.time_of_last_tick_on_connection;

    int target_tick=client_st.last_tick;
    
    add_wall({5,3});
    add_wall({6,3});
    add_wall({7,3});
    add_wall({8,3});
    add_wall({5,5});
    add_wall({6,7});

    gs.players[client_st.client_id].cmd_interp = 0;
    // delay it by 10 ticks, so 1/6 of a second

    while (running) {
        // input
        PollEvents(&input,&running);

        character *player=nullptr;
        if (gs.players[client_st.client_id].id != ID_DONT_EXIST) {
            player = &gs.players[client_st.client_id];
        }

        int o_num=target_tick;
        target_tick = client_st.get_exact_current_server_tick();
        if (o_num != target_tick) {
            printf("Tick %d\n",target_tick);
        }
        
        update_player_controller(player,target_tick);
        //if (gs.tick < target_tick)
        //load_game_state_up_to_tick(gs,client_st.NetState,target_tick,false);
        while(gs.tick<target_tick) {
            gs.update(client_st.NetState,tick_delta);
            gs.tick++;
            tick_clock.start_time += tick_delta;
        }

        if (input_send_clock.getElapsedTime() > input_send_delta) {
            client_send_input();
            input_send_clock.Restart();
        }

        render_game_state(sdl_renderer);
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
