
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
    int time;
    // dont initialize until you send bc it doesnt matter
    u32 sending_id=ID_DONT_EXIST;
};


struct command_stack_t {
    command_t stack[1024];
    u32 count=0;
};


bool operator==(const command_t &left, const command_t &right) {
    return left.code == right.code && left.time == right.time && left.press == right.press && left.sending_id == right.sending_id;
}

struct character {
    v2 pos;
    v2 vel;

    // net header
    int r_time;
    u32 ghost=ID_DONT_EXIST;
    u32 id=ID_DONT_EXIST;
    int ghost_delay = 2000;
    int creation_time;

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


void add_command_to_recent_commands(command_t cmd);

void update_player_controller(character *player,int time) {
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
        process_command(player,new_commands[id]);
        add_command_to_recent_commands(new_commands[id]);
    }
}

void update_player(character *player, float delta) {
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
    player->r_time += (int)(delta*1000.f);
}

void rewind_player(character *player, int target_time, command_t *cmd_stack, u32 cmd_count) {
    // Update up to the next command. Repeat until there are no more commands
    for (u32 ind=cmd_count-1; ind>=0; ind--) {
        if (cmd_stack[ind].time > player->r_time) {
            continue;
        }
        if ((player->ghost != ID_DONT_EXIST && player->ghost != cmd_stack[ind].sending_id) || (player->ghost == ID_DONT_EXIST && cmd_stack[ind].sending_id != player->id)) {
            continue;
        } if (cmd_stack[ind].time < target_time) {
            break;
        } else if (cmd_stack[ind].time <= player->r_time) { 
            update_player(player,(cmd_stack[ind].time-player->r_time)/1000.f);
            unprocess_command(player,cmd_stack[ind]);
            player->r_time = cmd_stack[ind].time;
        }
    }

    if (player->r_time > target_time) {
        update_player(player,(target_time-player->r_time)/1000.f);
    }
    player->r_time = target_time;
}


// fast forwards the player up to the given end time
void fast_forward_player(character *player, int curr_time, int end_time, command_t *cmd_stack, u32 cmd_count) {
    for (u32 ind=0; ind<cmd_count; ind++) {
        if ((player->ghost != ID_DONT_EXIST && player->ghost != cmd_stack[ind].sending_id) || (player->ghost == ID_DONT_EXIST && cmd_stack[ind].sending_id != player->id)) {
            continue;
        } if (cmd_stack[ind].time > end_time) {
            break;
        } else if (cmd_stack[ind].time >= curr_time) {
            update_player(player,(cmd_stack[ind].time-curr_time)/1000.f);
            process_command(player,cmd_stack[ind]);
            curr_time = cmd_stack[ind].time;
        }
    }

    if (curr_time < end_time) {
        update_player(player,(end_time-curr_time)/1000.f);
    }
    player->r_time = end_time;
}
