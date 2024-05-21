
enum {
    // Player commands
    CMD_LEFT,
    CMD_RIGHT,
    CMD_UP,
    CMD_DOWN,
    
    CMD_PUNCH,
    CMD_ACTION,
    
    // shoot is bullet
    CMD_RELOAD,
    
    // Server commands
    CMD_ADD_PLAYER,
    CMD_DIE,
    CMD_TAKE_DAMAGE,
    
    CMD_COUNT,
};

inline internal
bool get_command_state(u16 cmd_state,u16 cmd_to_test) {
    return cmd_state & (1 << cmd_to_test);
}

inline internal
void set_command_state(u16 &cmd_state,u16 cmd_to_set) {
    cmd_state |= (1 << cmd_to_set);
}

inline internal
void disable_command_state(u16 &cmd_state,u16 cmd_to_disable) {
    cmd_state &= ~(1 << cmd_to_disable);
}


void update_bullet(bullet_t &bullet,double delta) {
    bullet.position += bullet.vel*delta;
}


struct command_t {
    u8 code;
    // this is false if the command is being released instead of pressed
    bool press;
    i32 tick;
    // dont initialize until you send bc it doesnt matter
    entity_id sending_id=ID_DONT_EXIST;

    union {
        entity_id obj_id;
        float rot;
        i32 damage;
        i32 purchase;
        ITEM_TYPE selected_item;
    } props;
};

struct command_sort_by_time {
    bool operator()(command_t a, command_t b) const {
        return a.tick < b.tick;
    }
};


bool operator==(const command_t &left, const command_t &right) {
    return left.tick == right.tick && left.code == right.code && left.press == right.press && left.sending_id == right.sending_id;
}


// struct that acts as a non strict combination of commands and actions
// example: if you press the right key mid animiation and are not yet moving,
// instead of sending a right key command for others to hear the audio,
// you'd wait for them to start moving and then send the pseudo action instead

// NOTE: pretty much just for client info like sound and effects, but they're
// NOT triggered immediately by a command.


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
    v2 target_vel={0,0};
    i32 health=100;

    bool flip=false;
    bool visible=true;
    bool reloading=false;
    u8 curr_state=IDLE;

    int bullets_in_mag=5;
    float damage_timer=0.f;
    float reload_timer=0.f;
    float animation_timer=0.f;
    float state_timer=0.f;

    union {
        bool has_hit_something_yet;
    };

    enum {
        IDLE,
        MOVING,
        PUNCHING,
        TAKING_DAMAGE,
        DEAD,
        SHIELD,
        PLANTING,
        DEFUSING,
        CHARACTER_STATE_COUNT
    };

    // this info never changes
    Color color={255,255,255,255};
    entity_id id=ID_DONT_EXIST;

    // can condense this to like 2 bytes total with bitwise
    u16 command_state=0;

    // inventory
    static constexpr i32 INVENTORY_SIZE=5;
    inventory_item_t inventory[INVENTORY_SIZE]={};
    ITEM_TYPE current_equipped_item=IT_NONE;

    inline
    bool get_command_state(u16 cmd_to_test) {
        return command_state & (1 << cmd_to_test);
    }
};

character create_player(v2 pos,entity_id nid) {
    character p={};
    p.pos = pos;
    p.vel = {0,0};
    p.id = nid;
    return p;
}

void player_take_damage(character *player, int dmg) {
    player->health -= dmg;
    player->damage_timer=0.5;
}


fRect get_player_physics_hitbox(character *player) {
    float r_width = 32, r_height = 32.f;
    return {player->pos.x+32-(r_width/2),player->pos.y+64-r_height,r_width,r_height};
}


void update_player_controller(character *player, int tick, camera_t *game_camera=nullptr) {
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
    } if (input.mouse_just_pressed) {
        command_t n_cmd = {CMD_ACTION,true,tick,player->id};
        n_cmd.props.selected_item = player->inventory[inventory_ui.selected_slot].type;
        new_commands[cmd_count++] = n_cmd;
    } if (input.just_pressed[SDL_SCANCODE_R]) {
        new_commands[cmd_count++] = {CMD_RELOAD,true,tick,player->id};
    }

    for (u32 id=0; id < cmd_count; id++) {
        add_command_to_recent_commands(new_commands[id]);
    }
}

int process_command(character *player, command_t cmd) {
    if (player->curr_state == character::DEAD) return 0;
    
    if (cmd.code == CMD_DIE) {
        player->curr_state = character::DEAD;
    } else if (cmd.code == CMD_TAKE_DAMAGE) {
        player_take_damage(player,cmd.props.damage);
        player->curr_state = character::TAKING_DAMAGE;
    } else if (cmd.code == CMD_RELOAD) {
        if (player->reloading) {
            // cancel reload
            player->reloading=false;
            return 0; // TODO: because this is a return 0, the return statement doesn't indicate whether
            // the command was processed, but instead whether a sound should be made with it
        } else if (player->bullets_in_mag != 5 && player->reloading == false) {
            player->reloading = true;
            player->reload_timer=2.0;
            queue_sound(SfxType::FLINTLOCK_RELOAD_SFX,player->id,cmd.tick);
        }
    } else if (cmd.code == CMD_ACTION) {
        if (cmd.props.selected_item != IT_WOODEN_FENCE) {
            player->curr_state = character::PUNCHING;
            player->state_timer = 0.2f;
            player->has_hit_something_yet=false;
            player->current_equipped_item=cmd.props.selected_item;
        }
        /*
    } else if (cmd.code == CMD_PUNCH) {
        player->curr_state = character::PUNCHING;
        player->state_timer = 0.2f;
        player->has_hit_something_yet=false;
        */
    } else {
        if (cmd.press) {
            set_command_state(player->command_state,cmd.code);
        } else {
            disable_command_state(player->command_state,cmd.code);
        }
    }
    return 1;
}


// passing tick as a param is dangerous!! ruh roh!!!
void update_player(character *player, double delta, i32 wall_count, v2i *walls, i32 player_count, character* players, i32 tick) {
    if (player->curr_state == character::DEAD) {
        return;
    }    

    float move_speed = 300;
    if (player->curr_state != character::TAKING_DAMAGE && player->curr_state != character::PUNCHING) {
        // movement code
        if (player->get_command_state(CMD_LEFT)) {
            player->target_vel.x = -1;
            player->flip = true;
        } else if (player->get_command_state(CMD_RIGHT)) {
            player->target_vel.x = 1;
            player->flip = false;
        } else {
            player->target_vel.x = 0;
        } if (player->get_command_state(CMD_UP)) {
            player->target_vel.y = -1;
        } else if (player->get_command_state(CMD_DOWN)) {
            player->target_vel.y = 1;
        } else {
            player->target_vel.y = 0;
        }
    } else if (player->curr_state == character::TAKING_DAMAGE) {
        player->target_vel = {0,0};
    }

    if (player->target_vel == v2(0,0) && player->curr_state == character::MOVING) {
        player->curr_state = character::IDLE;
    } else if (player->target_vel != v2(0,0) && player->curr_state == character::IDLE) {
        player->curr_state = character::MOVING;
        player->animation_timer = 0.25;
    }
    
    if (player->target_vel.x == 0) {
        player->vel.x *= player->curr_state == character::TAKING_DAMAGE ? 0.85f : 0.75f;
    } else {
        player->vel.x = player->target_vel.x*move_speed;
    } if (player->target_vel.y == 0) {
        player->vel.y *= player->curr_state == character::TAKING_DAMAGE ? 0.85f : 0.75f;
    } else {
        player->vel.y = player->target_vel.y*move_speed;
    }
    
    if (player->reloading) {
        player->vel *= 0.5f;
    }
    
    player->pos.x += player->vel.x * (float)delta;
    
    FOR(walls,wall_count) {
        fRect p_hitbox = get_player_physics_hitbox(player);
        fRect wall_rect = {obj->x*64.f,obj->y*64.f,64.f,64.f};
        if (rects_collide(p_hitbox,wall_rect)) {
            // adjust
            if (player->vel.x > 0) {
                player->pos.x = wall_rect.x - p_hitbox.w - (p_hitbox.x-player->pos.x);
            } else {
                player->pos.x = wall_rect.x + wall_rect.w - (p_hitbox.x-player->pos.x);
            }
            break;
        }
    }
    
    player->pos.y += player->vel.y * (float)delta;
    FOR(walls,wall_count) {
        fRect p_hitbox = get_player_physics_hitbox(player);
        fRect wall_rect = {obj->x*64.f,obj->y*64.f,64.f,64.f};
        if (rects_collide(p_hitbox,wall_rect)) {
            // adjust
            if (player->vel.y > 0) {
                player->pos.y = wall_rect.y - p_hitbox.h - (p_hitbox.y-player->pos.y);
            } else {
                player->pos.y = wall_rect.y + wall_rect.h - (p_hitbox.y-player->pos.y);
            }
            break;
        }
    }

    

    if (player->curr_state == character::MOVING) {
        player->animation_timer -= (float)delta;
        if (player->animation_timer <= 0) {
            player->animation_timer += 0.2f;
        }
    } else if (player->curr_state == character::PUNCHING) {
        player->state_timer -= (float)delta;
        if (player->state_timer <= 0) {
            if (player->target_vel != v2(0,0))
                player->curr_state = character::MOVING;
            else
                player->curr_state = character::IDLE;
            player->state_timer = 0.f;
        }
    }
    
    // anim
    if (player->damage_timer) {
        player->damage_timer -= (float)delta;
        if (player->damage_timer <= 0) {
            player->damage_timer = 0;
        }
    } if (player->reloading && player->reload_timer) {
        player->reload_timer -= (float)delta;
        if (player->reload_timer <= 0) {
            player->reload_timer = 0;
            player->reloading = false;
            player->bullets_in_mag=5;
        }
    }
}
