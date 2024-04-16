

struct game_state {
    i32 player_count=0;
    character players[8];
    
    i32 bullet_count=0;
    bullet_t bullets[64];
    double time=0;
};

global_variable game_state gs;

static void add_player() {
    u32 id = gs.player_count++;
    gs.players[id] = create_player({0,0},id);
    gs.players[id].creation_time = gs.time;
    gs.players[id].r_time = gs.time;
}

static void add_bullet(bullet_t n_bullet) {
    gs.bullets[gs.bullet_count++] = n_bullet;
}

static void pop_bullet() {
    gs.bullet_count--;
}


// bridge between gamestate and network
struct netstate_info_t {
    // fuck it
    std::vector<command_t> command_stack;
    std::vector<command_t> command_buffer;
    
    game_state snapshots[256];
    int snapshot_count=0;
};


static void rewind_game_state(game_state &gst, netstate_info_t &c, double target_time) {
    if (target_time > gst.time) {
        printf("ERROR: Rewinding into the future!\n");
    }
    for (i32 i=(i32)c.command_stack.size()-1; i>=0; i--) {
        command_t &cmd = c.command_stack[i];
        if (cmd.time < target_time) {
            break;
        }
        for (i32 n=0; n<gst.player_count; n++) {
            character &player=gst.players[n];
            update_player(&player,(cmd.time-player.r_time));
        }
        unprocess_command(&gst.players[cmd.sending_id],cmd);
        gst.time = cmd.time;
    }
    for (i32 ind=0; ind<gst.player_count; ind++) {
        character &player=gst.players[ind];
        update_player(&player,(target_time-player.r_time));
    }
    gst.time = target_time;
}


static void commandless_fast_forward(game_state &gst, double curr_time, double target_time) {
    for (i32 ind=0;ind<gst.player_count;ind++) {
        character &player = gst.players[ind];
        update_player(&player,(target_time - curr_time));
    }

    for (i32 ind=0; ind<gst.bullet_count; ind++) {
        bullet_t &bullet = gst.bullets[ind];
        update_bullet(bullet,(target_time-curr_time));
    }
}


// assumes we're only going into the future
static void update_game_state(game_state &gst, netstate_info_t &c, double target_time) {
    printf("Updating game state by %f",target_time-gst.time);
    if (target_time < gst.time) {
        printf("ERROR: Updating into the past!\n");
    }

    std::vector<command_t> full_stack;// = c.command_stack;
    full_stack.insert(full_stack.begin(),c.command_stack.begin(),c.command_stack.end());
    full_stack.insert(full_stack.end(),c.command_buffer.begin(),c.command_buffer.end());
    std::sort(full_stack.begin(),full_stack.end(),command_sort_by_time());
    printf(" with %zd commands\n",full_stack.size());
    

    command_t *curr=nullptr;
    for (auto &cmd: full_stack) {
        if (cmd.time >= gst.time) {
            if (cmd.time > target_time) {
                break;
            }
            curr = &cmd;//&c.command_stack[ind];
            break;
        }
    }
    
    while (curr) {
        commandless_fast_forward(gst,gst.time,curr->time);
        process_command(&gst.players[curr->sending_id],*curr);
        gst.time = curr->time;
        curr++;
        if (curr >= &full_stack[0] + full_stack.size() || curr->time > target_time) {
            curr=nullptr;
        }
    }

    commandless_fast_forward(gst,gst.time,target_time);
    gst.time = target_time;
}
