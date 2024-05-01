
enum {
    // Player commands
    CMD_LEFT,
    CMD_RIGHT,
    CMD_UP,
    CMD_DOWN,

    CMD_PUNCH,
    // shoot is bullet
    CMD_SHOOT,
    CMD_RELOAD,

    // special abilities
    CMD_FIREBURST,
    CMD_SHIELD,
    CMD_INVISIBLE,

    // Server commands
    CMD_ADD_PLAYER,
    CMD_DIE,
    CMD_TAKE_DAMAGE,
    
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
        i32 damage;
    } props;
};

struct command_sort_by_time {
    bool operator()(command_t a, command_t b) const {
        return a.tick < b.tick;
    }
};



bool operator==(const command_t &left, const command_t &right) {
    return left.props.rot == right.props.rot && left.tick == right.tick && left.code == right.code && left.time == right.time && left.press == right.press && left.sending_id == right.sending_id;
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


double shield_cooldown_length=6.0;
double invisibility_cooldown_length=12.0;
double fireburst_cooldown_length = 8.0;

struct character {
    v2 pos;
    v2 vel;
    v2 target_vel={0,0};
    i32 health=100;

    enum {
        FLINTLOCK,
        FIREBURST,
        KNIFE,
        FISTS
    } selected_weapon=FLINTLOCK;

    bool flip=false;
    bool visible=true;

    int bullets_in_mag=5;
    bool reloading=false;
    double damage_timer=0.0;
    double reload_timer=0.0;
    double shield_timer=0.0;
    double animation_timer=0.0;
    double invisibility_timer=0.0;
    double invisibility_cooldown=0.0;
    double shield_cooldown=0.0;
    double fireburst_cooldown=0.0;

    enum {
        IDLE,
        MOVING,
        PUNCHING,
        TAKING_DAMAGE,
        DEAD,
        FIREBURSTING,
        SHIELD,
        CHARACTER_STATE_COUNT
    } curr_state=IDLE;

    union {
        double timer=0.0;
    } state;

    Color color={255,255,255,255};
    u32 id=ID_DONT_EXIST;

    bool command_state[CMD_COUNT];
};

character create_player(v2 pos,u32 nid) {
    character p={};
    p.pos = pos;
    p.vel = {0,0};
    p.id = nid;
    memset(p.command_state,0,sizeof(bool)*CMD_COUNT);
    return p;
}

static void add_bullet(v2 pos, float rot, entity_id shooter_id);
static void command_callback(character *player, command_t cmd);
static void no_bullets_fire_effects();

void player_take_damage(character *player, int dmg) {
    player->health -= dmg;
    player->damage_timer=0.5;
}

int process_command(character *player, command_t cmd) {
    //if (player->curr_state == character::DEAD) return 0;
    
    if (cmd.code == CMD_PUNCH) {
        player->state.timer = 0.25;
        player->curr_state = character::PUNCHING;
    } else if (cmd.code == CMD_SHOOT) {
        add_bullet(player->pos+v2(16,16), cmd.props.rot, player->id);
        player->bullets_in_mag--;
    } else if (cmd.code == CMD_DIE) {
        player->curr_state = character::DEAD;
    } else if (cmd.code == CMD_FIREBURST) {
        if (player->fireburst_cooldown > 0) {
            return 0;
        }
    } else if (cmd.code == CMD_SHIELD) {
        if (player->shield_cooldown > 0) {
            return 0;
        }
        player->curr_state = character::SHIELD;
        player->shield_timer = 1.0;
        player->shield_cooldown = shield_cooldown_length;
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
        }
    } else if (cmd.code == CMD_INVISIBLE) {
        if (player->invisibility_cooldown > 0) {
            return 0;
        } if (player->curr_state == character::SHIELD) {
            return 0;
        }
        player->visible = false;
        player->invisibility_timer=2.5;
        player->invisibility_cooldown = invisibility_cooldown_length;
    } else if (cmd.code == CMD_FIREBURST) {
        // fuck this one actually lol
        return 0;
    } else {
        player->command_state[cmd.code] = cmd.press;
    }
    return 1;
    //command_callback(player,cmd);
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
    } if (input.just_pressed[SDL_SCANCODE_SPACE]) {
        if (player->bullets_in_mag==0) {
            no_bullets_fire_effects();
            // make clicking sound effect
        } else {
            v2i mPos = get_mouse_position();
            if (game_camera) {
                mPos = v2i((int)(mPos.x + (game_camera->pos.x-1280/2)),(int)(mPos.y + (game_camera->pos.y-720/2)));
            }
            float rot = get_angle_to_point(player->pos+v2(16,16)+v2(8,8),mPos);

            new_commands[cmd_count++] = {CMD_SHOOT,true,tick,player->id};
            new_commands[cmd_count-1].props.rot = rot;
        }
    } if (input.mouse_just_pressed) {
        new_commands[cmd_count++] = {CMD_PUNCH,true,tick,player->id};
    } if (input.just_pressed[SDL_SCANCODE_R]) {
        new_commands[cmd_count++] = {CMD_RELOAD,true,tick,player->id};
    } if (input.just_pressed[SDL_SCANCODE_F]) {
        new_commands[cmd_count++] = {CMD_INVISIBLE,true,tick,player->id};
    }

    if (input.just_pressed[SDL_SCANCODE_LCTRL]) {
        new_commands[cmd_count++] = {CMD_SHIELD,true,tick,player->id};
    } if (input.just_pressed[SDL_SCANCODE_Q]) {
        new_commands[cmd_count++] = {CMD_FIREBURST,true,tick,player->id};
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
    if (player->curr_state != character::TAKING_DAMAGE && player->curr_state != character::SHIELD) {
        if (player->command_state[CMD_LEFT]) {
            player->target_vel.x = -1;
            player->flip = true;
        } else if (player->command_state[CMD_RIGHT]) {
            player->target_vel.x = 1;
            player->flip = false;
        } else {
            player->target_vel.x = 0;
        } if (player->command_state[CMD_UP]) {
            player->target_vel.y = -1;
        } else if (player->command_state[CMD_DOWN]) {
            player->target_vel.y = 1;
        } else {
            player->target_vel.y = 0;
        }
    } else {
        player->target_vel = {0,0};
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
    // so it'll take a quarter of a second to get to maximum speed

    if (player->reloading) {
        player->vel *= 0.5f;
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

    if (player->curr_state != character::TAKING_DAMAGE && player->curr_state != character::SHIELD) {
        fRect p_hitbox = {player->pos.x,player->pos.y,64.f,64.f};
        // see if you're getting punched
        FOR(players,player_count) {
            if (obj->id == player->id) {
                continue;
            }
            if (obj->curr_state != character::PUNCHING) {
                continue;
            }
            fRect punch_hitbox = {obj->pos.x + 32,obj->pos.y + 16,48,32};
            if (obj->flip) {
                punch_hitbox.x -= punch_hitbox.w;
            }
        
            if (rects_collide(p_hitbox,punch_hitbox)) {
                // get punched!
                player->curr_state = character::TAKING_DAMAGE;
                if (player->reloading) {
                    player->reloading=false;
                }
                player->state.timer = 0.5;
                player->vel.x = (float)(obj->flip ? -1200 : 1200);
                player_take_damage(player,10);
                break;
            }
        }
    }

    // anim
    if (player->state.timer) {
        player->state.timer -= delta;
        if (player->state.timer <= 0) {
            player->state.timer = 0.0;
            player->curr_state = character::IDLE;
        }
    } if (player->damage_timer) {
        player->damage_timer -= delta;
        if (player->damage_timer <= 0) {
            player->damage_timer = 0;
        }
    } if (player->reloading && player->reload_timer) {
        player->reload_timer -= delta;
        if (player->reload_timer <= 0) {
            player->reload_timer = 0;
            player->reloading = false;
            player->bullets_in_mag=5;
        }
    } if (player->curr_state == character::SHIELD) {
        player->shield_timer -= delta;
        if (player->shield_timer <= 0) {
            player->curr_state = character::IDLE;
        }
    } if (player->visible == false && player->invisibility_timer > 0) {
        player->invisibility_timer -= delta;
        if (player->invisibility_timer <= 0) {
            player->invisibility_timer = 0;
            player->visible = true;
        }
    }
    player->invisibility_cooldown-=delta;
    player->shield_cooldown-=delta;
    player->fireburst_cooldown-=delta;
}
