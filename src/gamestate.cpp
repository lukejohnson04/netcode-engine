
struct game_state {
    u8 player_count=0;
    character players[8];
    int time=0;
};

global_variable game_state gs;

void add_player() {
    u32 id = gs.player_count++;
    gs.players[id] = create_player({0,0},id);
    gs.players[id].creation_time = gs.time;
    gs.players[id].r_time = gs.time;
}


// bridge between gamestate and network
struct netstate_info_t {
    
    command_t command_stack[1024];
    u32 stack_count=0;
    command_t command_buffer[1024];
    u32 buffer_count=0;
    
    // fuck it
    //std::vector<command_t> command_stack;
    //std::vector<command_t> command_buffer;
    
    game_state snapshots[256];
    int snapshot_count=0;
};


static void rewind_game_state(game_state &gst, netstate_info_t &c, int target_time) {
    for (i32 i=c.stack_count-1; i>=0; i--) {
        command_t &cmd = c.command_stack[i];
        for (i32 n=0; n<gst.player_count; n++) {
            character &player=gst.players[n];
            update_player(&player,(float)(cmd.time-player.r_time)/1000.f);
        }
        unprocess_command(&gst.players[cmd.sending_id],cmd);
    }
    for (i32 ind=0; ind<gst.player_count; ind++) {
        character &player=gst.players[ind];
        update_player(&player,(float)(target_time-player.r_time)/1000.f);
    }
    gst.time = target_time;
}

// assumes we're only going into the future
static void update_game_state(game_state &gst, netstate_info_t &c, int target_time) {
    if (target_time < gst.time) {
        printf("massive error\n");
    }
    command_t *curr=nullptr;
    for (u32 ind=0; ind<c.stack_count; ind++) {
        command_t &cmd = c.command_stack[ind];
        if (cmd.time >= gst.time) {
            if (cmd.time > target_time) {
                break;
            }
            curr = &c.command_stack[ind];
            break;
        }
    }
    
    while (curr) {
        for (i32 ind=0;ind<gst.player_count;ind++) {
            character &player = gst.players[ind];
            update_player(&player,(curr->time - gst.time)/1000.f);
        }
        process_command(&gst.players[curr->sending_id],*curr);
        gst.time = curr->time;
        curr++;
        if (curr >= c.command_stack+c.stack_count || curr->time > target_time) {
            curr=nullptr;
        }
    }

    for (u32 ind=0;ind<gst.player_count;ind++) {
        character &player = gst.players[ind];
        update_player(&player,(target_time - gst.time)/1000.f);
        player.r_time = target_time;
    }
    gst.time = target_time;
}

