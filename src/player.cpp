
enum {
    // Player commands
    CMD_LEFT,
    CMD_RIGHT,
    CMD_UP,
    CMD_DOWN,

    CMD_PUNCH,
    // shoot is bullet
    CMD_SHOOT,

    // Server commands
    CMD_ADD_PLAYER,
    CMD_DIE,
    
    CMD_COUNT,
};

void update_bullet(bullet_t &bullet,double delta) {
    bullet.position += bullet.vel*delta;
}


struct command_t {
    u8 code;
    // this is false if the command is being released instead of pressed
    bool press;
    int tick;
    // dont initialize until you send bc it doesnt matter
    u32 sending_id=ID_DONT_EXIST;
    double time;

    union {
        entity_id obj_id;
        float rot;
    } props;
};

struct command_sort_by_time {
    bool operator()(command_t a, command_t b) const {
        return a.tick < b.tick;
    }
};


bool operator==(const command_t &left, const command_t &right) {
    return left.code == right.code && left.time == right.time && left.press == right.press && left.sending_id == right.sending_id;
}

static void add_command_to_recent_commands(command_t cmd);


// this should be in network and entities shouldn't have any idea about ANY of this information!
// TODO: abstract this out, make entities stupid
struct entity_net_header_t {
    u32 id=ID_DONT_EXIST;
    double r_time;
    double creation_time;
    double interp_delay=0.1;
};


struct character {
    v2 pos;
    v2 vel;
    i32 health=100;

    bool flip=false;

    double damage_timer=0.0;
    double reload_timer=0.0;
    double animation_timer=0.0;

    enum {
        IDLE,
        MOVING,
        PUNCHING,
        TAKING_DAMAGE,
        DEAD,
        CHARACTER_STATE_COUNT
    } curr_state=IDLE;

    union {
        double timer=0.0;
    } state;
    
    u32 id=ID_DONT_EXIST;
    double r_time;
    double creation_time;
    int cmd_interp=10;
    bool interp=false;

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

static void add_bullet(v2 pos, float rot, entity_id shooter_id);

void process_command(character *player, command_t cmd) {
    if (cmd.code == CMD_PUNCH) {
        player->state.timer = 0.25;
        player->curr_state = character::PUNCHING;
    } else if (cmd.code == CMD_SHOOT) {
        add_bullet(player->pos+v2(16,16), cmd.props.rot, player->id);
        player->reload_timer=3.0;
    } else if (cmd.code == CMD_DIE) {
        player->curr_state = character::DEAD;
    } else {
        player->command_state[cmd.code] = cmd.press;
    }
    // if (cmd.code == CMD_LEFT || cmd.code == CMD_RIGHT || cmd.code == CMD_UP || cmd.code == CMD_DOWN) {
}


// NOTE: this doesn't save the previous state!!! SO if you're holding right,
// and then somehow a +right gets fired, when reversing it it will always reset
// to a -right instead of maintaining the previous state of moving right!!!

// can't undo animation like this because we don't know where we were in the
// animation when the command was called
/*
void unprocess_command(character *player, command_t cmd) {
    if (cmd.code == CMD_SHOOT) {
        pop_bullet();
    } else if (cmd.code == CMD_PUNCH) {
        player->state.timer = 0;
        player->curr_state = character::IDLE;
    } else {
        player->command_state[cmd.code] = !cmd.press;
    }
}
*/

void update_player_controller(character *player, int tick) {
    // 16 max commands a tick
    command_t new_commands[16];
    u32 cmd_count=0;

    if (input.just_pressed[SDL_SCANCODE_D]) {
        new_commands[cmd_count++] = {CMD_RIGHT,true,tick,player->id};
    } else if (input.just_released[SDL_SCANCODE_D]) {
        new_commands[cmd_count++] = {CMD_RIGHT,false,tick,player->id};
    } if (input.just_pressed[SDL_SCANCODE_A]) {
        new_commands[cmd_count++] = {CMD_LEFT,true,tick,player->id};
    } else if (input.just_released[SDL_SCANCODE_A]) {
        new_commands[cmd_count++] = {CMD_LEFT,false,tick,player->id};
    } if (input.just_pressed[SDL_SCANCODE_W]) {
        new_commands[cmd_count++] = {CMD_UP,true,tick,player->id};
    } else if (input.just_released[SDL_SCANCODE_W]) {
        new_commands[cmd_count++] = {CMD_UP,false,tick,player->id};
    } if (input.just_pressed[SDL_SCANCODE_S]) {
        new_commands[cmd_count++] = {CMD_DOWN,true,tick,player->id};
    } else if (input.just_released[SDL_SCANCODE_S]) {
        new_commands[cmd_count++] = {CMD_DOWN,false,tick,player->id};
    } if (input.just_pressed[SDL_SCANCODE_SPACE] && player->reload_timer == 0) {
        v2i mPos = get_mouse_position();
        float rot = get_angle_to_point(player->pos+v2(16,16)+v2(8,8),mPos);

        new_commands[cmd_count++] = {CMD_SHOOT,true,tick,player->id};
        new_commands[cmd_count-1].props.rot = rot;
    } if (input.mouse_just_pressed) {
        new_commands[cmd_count++] = {CMD_PUNCH,true,tick,player->id};
    }

    for (u32 id=0; id < cmd_count; id++) {
        add_command_to_recent_commands(new_commands[id]);
    }
}

void update_player(character *player, double delta, i32 wall_count, v2i *walls, i32 player_count, character* players) {
    if (player->curr_state == character::DEAD) {
        return;
    }
    
    float move_speed = 300;
    if (player->command_state[CMD_LEFT]) {
        player->vel.x = -move_speed;
        player->flip = true;
    } else if (player->command_state[CMD_RIGHT]) {
        player->vel.x = move_speed;
        player->flip = false;
    } else {
        player->vel.x = 0;
    } if (player->command_state[CMD_UP]) {
        player->vel.y = -move_speed;
    } else if (player->command_state[CMD_DOWN]) {
        player->vel.y = move_speed;
    } else {
        player->vel.y = 0;
    }

    if (player->curr_state == character::TAKING_DAMAGE) {
        player->vel = {0,0};
    }
    
    player->pos.x += player->vel.x * (float)delta;
    FOR(walls,wall_count) {
        fRect p_hitbox = {player->pos.x,player->pos.y,64.f,64.f};
        fRect wall_rect = {obj->x*64.f,obj->y*64.f,64.f,64.f};
        if (rects_collide(p_hitbox,wall_rect)) {
            // adjust
            if (player->vel.x > 0) {
                player->pos.x = wall_rect.x - p_hitbox.w;
            } else {
                player->pos.x = wall_rect.x + wall_rect.w;
            }
            break;
        }
    }
    
    player->pos.y += player->vel.y * (float)delta;
    FOR(walls,wall_count) {
        fRect p_hitbox = {player->pos.x,player->pos.y,64.f,64.f};
        fRect wall_rect = {obj->x*64.f,obj->y*64.f,64.f,64.f};
        if (rects_collide(p_hitbox,wall_rect)) {
            // adjust
            if (player->vel.y > 0) {
                player->pos.y = wall_rect.y - p_hitbox.h;
            } else {
                player->pos.y = wall_rect.y + wall_rect.h;
            }
            break;            
        }
    }

    // see if you're getting punched
    FOR(players,player_count) {
        if (obj->id == player->id) {
            continue;
        }
        if (obj->curr_state != character::PUNCHING) {
            continue;
        }
        if (player->pos.x > obj->pos.x && player->pos.x < obj->pos.x + 50 && abs(player->pos.y - obj->pos.y) < 32) {
            // get punched!
            player->curr_state = character::TAKING_DAMAGE;
            player->state.timer = 0.5;
            break;
        }
    }

    // anim
    if (player->state.timer) {
        player->state.timer -= delta;
        if (player->state.timer <= 0) {
            player->state.timer = 0.0;
            //if (player->curr_state == character::PUNCHING) {
                player->curr_state = character::IDLE;
                //}
        }
    } if (player->damage_timer) {
        player->damage_timer -= delta;
        if (player->damage_timer <= 0) {
            player->damage_timer = 0;
        }
    } if (player->reload_timer) {
        player->reload_timer -= delta;
        if (player->reload_timer <= 0) {
            player->reload_timer = 0;
        }
    }

    player->r_time += delta;
}


void rewind_player(character *player, double target_time, std::vector<command_t> &cmd_stack) {}
/*
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
*/


// fast forwards the player up to the given end time
void fast_forward_player(character *player, double end_time, std::vector<command_t> &cmd_stack) {}
/*
    if (player->r_time > end_time) {
        printf("fast forwarding into the past\n");
    }
    for (i32 ind=0; ind<cmd_stack.size(); ind++) {
        if (cmd_stack[ind].sending_id != player->id) {
            continue;
        } if (cmd_stack[ind].time > end_time) {
            break;
        } else if (cmd_stack[ind].time >= player->r_time) {
            update_player(player,(cmd_stack[ind].time-player->r_time,gs.wall_count,gs.walls));
            process_command(player,cmd_stack[ind]);
        }
    }

    if (player->r_time < end_time) {
        update_player(player,(end_time-player->r_time));
    }
    player->r_time = end_time;
}

*/

void player_take_damage(character *player, int dmg) {
    player->health -= dmg;
    player->damage_timer=0.5;
}
