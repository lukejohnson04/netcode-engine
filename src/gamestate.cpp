
struct netstate_info_t;

struct game_state {
    i32 player_count=0;
    character players[8];

    i32 wall_count=0;
    v2i walls[64];

    i32 bullet_count=0;
    bullet_t bullets[128];

    i32 score[8] = {0};

    double time=0;
    int tick=0;

    int round_end_tick=-1;

    void update(netstate_info_t &c, double delta);
};


// bridge between gamestate and network
struct netstate_info_t {
    // fuck it
    std::vector<command_t> command_stack;
    std::vector<command_t> command_buffer;

    std::vector<game_state> snapshots;

    bool authoritative=false;
    int interp_delay=0;

    void add_snapshot(game_state gs) {
        for (game_state &snap:snapshots) {
            if (snap.tick == gs.tick) {
                printf("WARNING: Overwriting snapshot at tick %d",gs.tick);
                snap = gs;
                return;
            }
        }
        snapshots.push_back(gs);
    }

    bool do_charas_interp[16]={true};
};


global_variable game_state gs;

static void add_player() {
    u32 id = gs.player_count++;
    gs.players[id] = create_player({0,0},id);
    gs.players[id].creation_time = gs.time;
    gs.players[id].r_time = gs.time;
}

static void add_wall(v2i wall) {
    gs.walls[gs.wall_count++] = wall;
}

static void add_bullet(v2 pos, float rot, entity_id shooter_id) {
    bullet_t &bullet = gs.bullets[gs.bullet_count++];
    bullet.position = pos;
    bullet.vel = convert_angle_to_vec(rot)*600.f;
    bullet.shooter_id = shooter_id;
}


static void rewind_game_state(game_state &gst, netstate_info_t &c, double target_time) {
    for (i32 ind=(i32)c.snapshots.size()-1;ind>=0;ind--) {
        if (c.snapshots[ind].time<=target_time) {
            gst = c.snapshots[ind];
            gst.update(c,target_time);
            return;
        }
    }
    printf("ERROR: Cannot rewind game state!\n");
}

void game_state::update(netstate_info_t &c, double delta) {
    // add commands as the server
    if (c.authoritative) {
        FORn(players,player_count,player) {
            if (player->health <= 0) {
                command_t kill_command = {CMD_DIE,false,gs.tick,player->id};
                c.command_stack.push_back(kill_command);
                round_end_tick = tick;
            }
        }
    }
    
    // process commands for this tick
    for (command_t &cmd: c.command_stack) {
        if (cmd.tick == tick) {
            process_command(&players[cmd.sending_id],cmd);
        }
    }
    for (command_t &cmd: c.command_buffer) {
        if (cmd.tick == tick) {
            process_command(&players[cmd.sending_id],cmd);
        }
    }
    

    // interp delay in ticks
    int interp_delay = c.interp_delay;
    int interp_players=0;
    int interp_tick=tick-interp_delay;
    FOR(players,player_count) {
        if (obj->curr_state == character::DEAD) continue;
        if (c.do_charas_interp[obj->id] && !c.authoritative) {
            interp_players++;
            game_state *prev_snap=nullptr;
            game_state *next_snap=nullptr;

            for (auto &snap:c.snapshots) {
                if (snap.tick<interp_tick) {
                    prev_snap = &snap;
                } else if (snap.tick>=interp_tick) {
                    next_snap = &snap;
                    break;
                }
            }

            if (next_snap && prev_snap) {
                v2 prev_pos = prev_snap->players[obj->id].pos;
                v2 next_pos = next_snap->players[obj->id].pos;
                float f = (float)(interp_tick-prev_snap->tick) / (float)(next_snap->tick-prev_snap->tick);
                obj->pos = lerp(prev_pos,next_pos,f);
                
            } else {
                printf("No snapshots to interpolate between for %d\n",obj->id);
            }
        } else {
            update_player(obj,delta,wall_count,walls,player_count,players);
        }
    }
    if (!c.authoritative && player_count > 1 && interp_players == 0) {
        printf("Not interpolating at all!\n");
    }

    FOR(bullets,bullet_count) {
        obj->position += obj->vel*delta;
        // if a bullet hits a wall delete it
        FORn(walls,wall_count,wall) {
            fRect b_hitbox = {obj->position.x,obj->position.y,16.f,16.f};
            fRect wall_rect = {wall->x*64.f,wall->y*64.f,64.f,64.f};
            if (rects_collide(b_hitbox,wall_rect)) {
                *obj = bullets[--bullet_count];
                break;
            }
        }
        FORn(players,wall_count,player) {
            if (player->id==obj->shooter_id) continue;
            fRect b_hitbox = {obj->position.x,obj->position.y,16.f,16.f};
            fRect p_hitbox = {player->pos.x,player->pos.y,64.f,64.f};
            if (rects_collide(b_hitbox,p_hitbox)) {
                *obj = bullets[--bullet_count];
                player_take_damage(player,10);
                break;
            }
        }
        if (obj->position.x > 2000 || obj->position.x < -2000 || obj->position.y > 2000 || obj->position.y < -2000) {
            *obj = bullets[--bullet_count];
            obj--;
            continue;
        }
    }
}


void render_game_state(SDL_Renderer *sdl_renderer) {
    // Render start
    SDL_SetRenderDrawColor(sdl_renderer,255,255,0,255);
    SDL_RenderClear(sdl_renderer);
    
    for (i32 id=0; id<gs.player_count; id++) {
        character &p = gs.players[id];

        SDL_Rect src_rect = {p.curr_state == character::PUNCHING ? 64 : 0,p.curr_state == character::TAKING_DAMAGE ? 32 : 0,32,32};
        SDL_Rect rect = {(int)p.pos.x,(int)p.pos.y,64,64};
        if (p.damage_timer) {
            u8 g = (u8)lerp(180,255,(0.5f-(float)p.damage_timer)*(1.f/0.5f));
            u8 b = (u8)lerp(150,255,(0.5f-(float)p.damage_timer)*(1.f/0.5f));
            SDL_SetTextureColorMod(textures[PLAYER_TEXTURE],255,g,b);
        } else {
            SDL_SetTextureColorMod(textures[PLAYER_TEXTURE],255,255,255);
        }
        SDL_RenderCopyEx(sdl_renderer,textures[PLAYER_TEXTURE],&src_rect,&rect,NULL,NULL,p.flip?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);
    }

    for (i32 ind=0; ind<gs.wall_count; ind++) {
        SDL_SetRenderDrawColor(sdl_renderer,0,0,0,255);

        SDL_Rect rect = {(int)gs.walls[ind].x*64,(int)gs.walls[ind].y*64,64,64};
        SDL_RenderFillRect(sdl_renderer, &rect);
    }

    for (i32 ind=0; ind<gs.bullet_count; ind++) {
        SDL_Rect rect = {(int)gs.bullets[ind].position.x,(int)gs.bullets[ind].position.y,16,16};
        SDL_Point center = {8,8};
        float rad_rot = convert_vec_to_angle(gs.bullets[ind].vel)+PI;
        SDL_RenderCopyEx(sdl_renderer,textures[BULLET_TEXTURE],NULL,&rect,rad_2_deg(rad_rot),&center,SDL_FLIP_NONE);
    }

}

// loads the first snapshot found before the input time
void find_and_load_gamestate_snapshot(game_state &gst, netstate_info_t &c, int start_tick) {
    for (i32 ind=((i32)c.snapshots.size())-1; ind>=0;ind--) {
        if (c.snapshots[ind].tick<=start_tick) {
            gst = c.snapshots[ind];
            return;
        }
    }
    printf("ERROR: Couldn't find snapshot to rewind to %d!\n",start_tick);
}

void load_game_state_up_to_tick(game_state &gst, netstate_info_t &c, int target_tick, bool overwrite_snapshots=true) {
    while(gs.tick<target_tick) {
        // crazy inefficient lol
        if (overwrite_snapshots) {
            for (game_state &snap: c.snapshots) {
                if (snap.tick==gs.tick) {
                    snap = gs;
                    break;
                }
            }
        }
        gs.update(c,1.0/60.0);
        gs.tick++;
    }
}

