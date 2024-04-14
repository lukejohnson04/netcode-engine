
enum {
    // Player commands
    CMD_LEFT,
    CMD_RIGHT,
    CMD_UP,
    CMD_DOWN,
    CMD_COUNT,

    // Server commands
    CMD_ADD_PLAYER
};

struct command_t {
    u8 code;
    // this is false if the command is being released instead of pressed
    bool press;
    double time;
    // dont initialize until you send bc it doesnt matter
    u32 sending_id=ID_DONT_EXIST;
};

struct command_sort_by_time {
    bool operator()(command_t a, command_t b) const {
        return a.time < b.time;
    }
};


bool operator==(const command_t &left, const command_t &right) {
    return left.code == right.code && left.time == right.time && left.press == right.press && left.sending_id == right.sending_id;
}

struct character {
    v2 pos;
    v2 vel;

    // net header
    u32 id=ID_DONT_EXIST;
    double r_time;
    double creation_time;
    double interp_delay=0.1;

    bool command_state[CMD_COUNT];
};

character create_player(v2 pos,u32 nid) {
    character p;
    p.pos = pos;
    p.vel = {0,0};
    p.id = nid;
    memset(p.command_state,0,sizeof(bool)*CMD_COUNT);
    return p;
}


void process_command(character *player, command_t cmd) {
    player->command_state[cmd.code] = cmd.press;
}


// NOTE: this doesn't save the previous state!!! SO if you're holding right,
// and then somehow a +right gets fired, when reversing it it will always reset
// to a -right instead of maintaining the previous state of moving right!!!
void unprocess_command(character *player, command_t cmd) {
    player->command_state[cmd.code] = !cmd.press;
}

static void add_command_to_recent_commands(command_t cmd);


void update_player_controller(character *player,double time) {
    // 16 max commands a tick
    command_t new_commands[16];
    u32 cmd_count=0;

    if (input.just_pressed[SDL_SCANCODE_D]) {
        new_commands[cmd_count++] = {CMD_RIGHT,true,time,player->id};
    } else if (input.just_released[SDL_SCANCODE_D]) {
        new_commands[cmd_count++] = {CMD_RIGHT,false,time,player->id};
    } if (input.just_pressed[SDL_SCANCODE_A]) {
        new_commands[cmd_count++] = {CMD_LEFT,true,time,player->id};
    } else if (input.just_released[SDL_SCANCODE_A]) {
        new_commands[cmd_count++] = {CMD_LEFT,false,time,player->id};
    } if (input.just_pressed[SDL_SCANCODE_W]) {
        new_commands[cmd_count++] = {CMD_UP,true,time,player->id};
    } else if (input.just_released[SDL_SCANCODE_W]) {
        new_commands[cmd_count++] = {CMD_UP,false,time,player->id};
    } if (input.just_pressed[SDL_SCANCODE_S]) {
        new_commands[cmd_count++] = {CMD_DOWN,true,time,player->id};
    } else if (input.just_released[SDL_SCANCODE_S]) {
        new_commands[cmd_count++] = {CMD_DOWN,false,time,player->id};
    }

    for (u32 id=0; id < cmd_count; id++) {
        //process_command(player,new_commands[id]);
        add_command_to_recent_commands(new_commands[id]);
    }
}

void update_player(character *player, double delta) {
    if (player->r_time + delta < player->creation_time) {
        delta = player->r_time - player->creation_time;
    }
    
    float move_speed = 300;
    if (player->command_state[CMD_LEFT]) {
        player->vel.x = -move_speed;
    } else if (player->command_state[CMD_RIGHT]) {
        player->vel.x = move_speed;
    } else {
        player->vel.x = 0;
    } if (player->command_state[CMD_UP]) {
        player->vel.y = -move_speed;
    } else if (player->command_state[CMD_DOWN]) {
        player->vel.y = move_speed;
    } else {
        player->vel.y = 0;
    }
    player->pos += player->vel * delta;
    player->r_time += delta;
}

void rewind_player(character *player, double target_time, std::vector<command_t> &cmd_stack) {
    if (player->r_time < target_time) {
        printf("rewinding into the future\n");
    }
    
    // Update up to the next command. Repeat until there are no more commands
    for (i32 ind=(i32)cmd_stack.size()-1; ind>=0; ind--) {
        if (cmd_stack[ind].time > player->r_time) {
            continue;
        }
        if (cmd_stack[ind].sending_id != player->id) {
            continue;
        } if (cmd_stack[ind].time < target_time) {
            break;
        } else if (cmd_stack[ind].time <= player->r_time) { 
            update_player(player,(cmd_stack[ind].time-player->r_time));
            unprocess_command(player,cmd_stack[ind]);
            player->r_time = cmd_stack[ind].time;
        }
    }

    if (player->r_time > target_time) {
        update_player(player,(target_time-player->r_time));
    }
    player->r_time = target_time;
}


// fast forwards the player up to the given end time
void fast_forward_player(character *player, double end_time, std::vector<command_t> &cmd_stack) {
    if (player->r_time > end_time) {
        printf("fast forwarding into the past\n");
    }
    for (i32 ind=0; ind<cmd_stack.size(); ind++) {
        if (cmd_stack[ind].sending_id != player->id) {
            continue;
        } if (cmd_stack[ind].time > end_time) {
            break;
        } else if (cmd_stack[ind].time >= player->r_time) {
            update_player(player,(cmd_stack[ind].time-player->r_time));
            process_command(player,cmd_stack[ind]);
        }
    }

    if (player->r_time < end_time) {
        update_player(player,(end_time-player->r_time));
    }
    player->r_time = end_time;
}

