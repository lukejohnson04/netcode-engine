
struct netstate_info_t;


// should be called world state instead of game state
struct game_state {
    i32 player_count=0;
    character players[8];

    i32 wall_count=0;
    v2i walls[240];

    i32 bullet_count=0;
    bullet_t bullets[128];

    // should abstract these net variables out eventually
    i32 score[8] = {0};
    double time=0;
    int tick=0;

    int round_start_tick=0;
    int one_remaining_tick=0;

    void update(netstate_info_t &c, double delta);
};

enum GMS {
    GAME_HASNT_STARTED,
    PREGAME_SCREEN,
    ROUND_PLAYING,
    ROUND_END,
    ROUND_COUNTDOWN,
};


// the reason this struct is 100% SEPARATE from game_state is because we don't want
// to worry about these details when writing generic update code for the game.
// we need an interface or something for interaction between the two
struct overall_game_manager {
    i32 connected_players=0;
    i32 state=GAME_HASNT_STARTED;
    
    int round_end_tick=-1;

    LARGE_INTEGER game_start_time;
    bool counting_down_to_game_start=false;
};


// bridge between gamestate and network
struct netstate_info_t {
    // fuck it
    std::vector<command_t> command_stack;
    std::vector<command_t> command_buffer;

    std::vector<game_state> snapshots;
    std::vector<command_t> new_commands_for_fx;

    bool authoritative=false;
    int interp_delay=0;

    overall_game_manager gms;

    void add_snapshot(game_state gs, bool authoritative=false, int curr_tick=0, int snapshot_buffer=0) {
        if (authoritative) {
            for (game_state &snap: snapshots) {
                if (snap.tick == gs.tick) {
                    if (gs.tick < curr_tick-snapshot_buffer) {
                        printf("WARNING: Overwriting snapshot at tick %d",gs.tick);
                    }
                    snap = gs;
                    return;
                }
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

void load_gamestate_for_round(overall_game_manager &gms) {
    gms.state = ROUND_PLAYING;
    i32 rand_level = rand() % 4;    
    SDL_Surface *level_surface = IMG_Load("res/levels.png");

    gs.bullet_count=0;
    gs.wall_count=0;
    gs.one_remaining_tick=0;


    i32 spawn_count=0;
    v2i spawns[8]={{0,0}};
    
    for (i32 x=0; x<20; x++) {
        for (i32 y=0; y<12; y++) {
            Color col = {0,0,0,0};
            Uint32 data = getpixel(level_surface, x+(20*rand_level),y);
            SDL_GetRGBA(data, level_surface->format, &col.r, &col.g, &col.b, &col.a);
            if (col == Color(0,0,0,255)) {
                add_wall({x,y});
            } else if (col == Color(255,0,0,255)) {
                spawns[spawn_count++] = {x,y};
            }
        }
    }

    bool create_all_charas = gs.player_count == gms.connected_players ? false : true;
    gs.player_count=0;

    for (i32 ind=0; ind<gms.connected_players; ind++) {
        if (create_all_charas == false) {
            character old_chara = gs.players[ind];
            
            add_player();
            gs.players[ind] = create_player({0,0},old_chara.id);
            gs.players[ind].color = old_chara.color;
        } else {
            add_player();
        
            int total_color_points=rand() % 255 + (255+200);
            gs.players[gs.player_count-1].color.r = rand()%MIN(255,total_color_points);
            total_color_points-=gs.players[gs.player_count-1].color.r;
            gs.players[gs.player_count-1].color.g = rand()%MIN(255,total_color_points);
            total_color_points-=gs.players[gs.player_count-1].color.g;
            gs.players[gs.player_count-1].color.b = total_color_points;
        }
        i32 rand_spawn = rand() % spawn_count;
        gs.players[ind].pos = spawns[rand_spawn]*64;
        spawns[rand_spawn] = spawns[spawn_count-1];
        spawn_count--;
    }

    SDL_FreeSurface(level_surface);

    // start in 5 seconds
    gs.round_start_tick = gs.tick + 300;
}


void game_state::update(netstate_info_t &c, double delta) {
    if (tick < round_start_tick) {
        return;
    }
    // add commands as the server
    if (c.authoritative) {
        FORn(players,player_count,player) {
            if (player->health <= 0) {
                command_t kill_command = {CMD_DIE,false,gs.tick,player->id};
                c.command_stack.push_back(kill_command);
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

    if (c.authoritative) {
        if (gs.one_remaining_tick==0) {
            i32 living_player_count=0;
            character *last_alive=nullptr;
            FORn(gs.players,gs.player_count,player){
                if (player->curr_state!=character::DEAD) {
                    living_player_count++;
                    last_alive=player;
                    if (living_player_count>1) {
                        break;
                    }
                }
            }
            if (living_player_count==1) {
                one_remaining_tick=tick;
                score[last_alive->id]++;
                printf("Player became last alive on tick %d\n",tick);
            }
        } else if (tick >= gs.one_remaining_tick) {
            if (tick >= gs.one_remaining_tick+90+180) {
                // gamestate win
                character *last_alive=nullptr;
                FORn(players,player_count,player){
                    if (player->curr_state!=character::DEAD) {
                        last_alive=player;
                        break;
                    }
                }
                if (last_alive==nullptr) {
                    // error
                    printf("ERROR: Game ended in a tie!\n");
                } else {
                    load_gamestate_for_round(c.gms);
                }
            }
        }
    }

    // interp delay in ticks
    int interp_delay = 6;//c.interp_delay;
    int interp_players=0;
    int interp_tick=tick-interp_delay;
    FOR(players,player_count) {
        if (obj->curr_state == character::DEAD) continue;
        //if (c.do_charas_interp[obj->id] && !c.authoritative && c.snapshots.size() > 0) {
            
        //} else {
        update_player(obj,delta,wall_count,walls,player_count,players);
            //}
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
                if (player->curr_state != character::SHIELD) {
                    player_take_damage(player,20);
                }
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

v2 pp;
int collision_point_sort(const void *v1, const void*v2) {
    v2i p1 = *(v2i*)v1;
    v2i p2 = *(v2i*)v2;

    return (int)((get_angle_to_point(pp,p1)-get_angle_to_point(pp,p2))*1000.f);
}

struct {
    std::vector<v2i> raycast_points;
} client_sided_render_geometry;


void render_game_state(SDL_Renderer *sdl_renderer, character *render_from_perspective_of=nullptr) {
    // Render start
    SDL_SetRenderDrawColor(sdl_renderer,255,255,0,255);
    SDL_RenderClear(sdl_renderer);
    
    for (i32 id=0; id<gs.player_count; id++) {
        character &p = gs.players[id];
        if (p.visible == false) continue;

        SDL_Rect src_rect = {p.curr_state == character::PUNCHING ? 64 : p.curr_state == character::SHIELD ? 96 : 0,p.curr_state == character::TAKING_DAMAGE ? 32 : 0,32,32};
        SDL_Rect rect = {(int)p.pos.x,(int)p.pos.y,64,64};
        if (p.damage_timer) {
            u8 g = (u8)lerp(p.color.g-75.f,(float)p.color.g,(0.5f-(float)p.damage_timer)*(1.f/0.5f));
            u8 b = (u8)lerp(p.color.b-105.f,(float)p.color.b,(0.5f-(float)p.damage_timer)*(1.f/0.5f));
            SDL_SetTextureColorMod(textures[PLAYER_TEXTURE],p.color.r,g,b);
        } else {
            SDL_SetTextureColorMod(textures[PLAYER_TEXTURE],p.color.r,p.color.g,p.color.b);
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
    if (render_from_perspective_of != nullptr && client_sided_render_geometry.raycast_points.size()>0) {
        const SDL_Color black = {0,0,0,255};
        const SDL_Color white = {255,255,255,255};

        SDL_SetRenderTarget(sdl_renderer,textures[SHADOW_TEXTURE]);
        SDL_RenderClear(sdl_renderer);
        SDL_SetRenderDrawColor(sdl_renderer,0,0,0,255);        
        SDL_RenderFillRect(sdl_renderer,NULL);
        
        v2i p_pos = render_from_perspective_of->pos + v2(32,32);
        
        std::vector<v2i> dests;
        
        for (v2i temp_t: client_sided_render_geometry.raycast_points) {
            v2 t=temp_t;
            //i32 p = n%4;
            //i32 wall_num = (i32)floor((float)n/4.f);
            //t = p==0?v2(wall.x,wall.y):p==1?v2(wall.x+1,wall.y):p==2?v2(wall.x,wall.y+1):v2(wall.x+1,wall.y+1);
            
            // if the corner is shared continue
            // TODO: Bake the walls when the level is first loaded
            // FPS goes back up to constant 144 if you just remove
            // unecessary walls, i.e. we don't need to send 12 rays
            // to a block that's in the edge.
            /*
            bool shared=false;
            for (i32 j=0;j<gs.wall_count;j++) {
                if (j==wall_num) continue;
                if (gs.walls[j].x != wall.x && gs.walls[j].y != wall.y) continue;
                v2 tp_1 = v2(gs.walls[j].x,  gs.walls[j].y);
                v2 tp_2 = v2(gs.walls[j].x+1,gs.walls[j].y);
                v2 tp_3 = v2(gs.walls[j].x,  gs.walls[j].y+1);
                v2 tp_4 = v2(gs.walls[j].x+1,gs.walls[j].y+1);
                if (t==tp_1||t==tp_2||t==tp_3||t==tp_4) {
                    shared=true;
                    break;
                }
            }
            if (shared) continue;
            t*=64;
            */
            
            intersect_props col = get_collision(p_pos,t,gs.walls,gs.wall_count);
            v2i pt;
            if (!col.collides) {
                printf("ERROR\n");
            } else {
                pt = v2(col.collision_point.x,col.collision_point.y);
            }

            v2 pt_2 = (convert_angle_to_vec(get_angle_to_point(p_pos,t) + 0.005f) * 2000.0f) + p_pos;
            v2 pt_3 = (convert_angle_to_vec(get_angle_to_point(p_pos,t) - 0.005f) * 2000.0f) + p_pos;
            intersect_props col_2 = get_collision(p_pos,pt_2,gs.walls,gs.wall_count);
            intersect_props col_3 = get_collision(p_pos,pt_3,gs.walls,gs.wall_count);
            if (col_2.collides==false) {
                printf("ERROR\n");
                dests.push_back(v2(pt_2-p_pos).normalize() * 1500.f + p_pos);
            } else {
                dests.push_back(col_2.collision_point);
            }
            if (col_3.collides==false) {
                printf("ERROR\n");
                dests.push_back(v2(pt_3-p_pos).normalize() * 1500.f + p_pos);
            } else {
                dests.push_back(col_3.collision_point);
            }
            
            dests.push_back(pt);

            /*
            // two slightly off points
            v2 pt_2 = (convert_angle_to_vec(get_angle_to_point(p_pos,t) + 0.005f) * 2000.f) + p_pos;
            v2 pt_3 = (convert_angle_to_vec(get_angle_to_point(p_pos,t) - 0.005f) * 2000.f) + p_pos;
            intersect_props col_2 = get_collision(p_pos,pt_2,gs.walls,gs.wall_count);
            intersect_props col_3 = get_collision(p_pos,pt_3,gs.walls,gs.wall_count);
            
            if (col_2.collides==false) {
                dests.push_back(v2(pt_2-p_pos).normalize() * 1500.f + p_pos);
            } else {
                dests.push_back(col_2.collision_point);
            }
            if (col_3.collides==false) {
                dests.push_back((pt_3-p_pos).normalize() * 1500.f + p_pos);
            } else {
                dests.push_back(col_3.collision_point);
            }

            dests.push_back(pt);
            */
        }


        // if we sort beforehand and then just add inorder we can reduce the sort size by
        // a factor of 3
        std::sort(dests.begin(),dests.end(),[p_pos](auto &left, auto &right) {
            return get_angle_to_point(p_pos,left)<get_angle_to_point(p_pos,right);
        });


        SDL_SetRenderDrawBlendMode(sdl_renderer,SDL_BLENDMODE_ADD);
        for (i32 n=0;n<dests.size()+1;n++) {
            bool fin=n==dests.size();
            if (fin) n=0;
            v2i pt=dests[n];
            v2i prev_point=n==0?dests.back():dests[n-1];
            SDL_Vertex vertex_1 = {{(float)p_pos.x,(float)p_pos.y}, white, {1, 1}};
            SDL_Vertex vertex_2 = {{(float)pt.x,(float)pt.y}, white, {1, 1}};
            SDL_Vertex vertex_3 = {{(float)prev_point.x,(float)prev_point.y}, white, {1, 1}};
            SDL_Vertex vertices[] = {vertex_1,vertex_2,vertex_3};
            SDL_RenderGeometry(sdl_renderer, NULL, vertices, 3, NULL, 0);
            if (fin) break;
        }        

        SDL_SetRenderTarget(sdl_renderer,NULL);
        SDL_RenderCopy(sdl_renderer,textures[SHADOW_TEXTURE],NULL,NULL);
        SDL_SetRenderDrawBlendMode(sdl_renderer,SDL_BLENDMODE_NONE);
        /*
        for (auto &pt: dests) {
            SDL_Rect dest = {(i32)pt.x-8,(i32)pt.y-8,16,16};
            SDL_SetRenderDrawColor(sdl_renderer,255,0,0,255);
            SDL_RenderDrawLine(sdl_renderer,(int)p_pos.x,(int)p_pos.y,(int)pt.x,(int)pt.y);
            SDL_RenderCopy(sdl_renderer,textures[RAYCAST_DOT_TEXTURE],NULL,&dest);
        }
        */
        dests.clear();
    }

    // gui elements
    if (gs.tick<gs.round_start_tick+90) {
        // display a countdown
        double time_until_start = (gs.round_start_tick-gs.tick) * (1.0/60.0);

        std::string timer_str;
        generic_drawable round_start_timer;
        v2 scale = {16,16};
        if (time_until_start > 0) {
            timer_str = std::to_string((int)ceil(time_until_start));
            scale = v2(12,12)*(time_until_start-floor(time_until_start)) + v2(4,4);
        } else {
            timer_str = "GO!";
        }
        round_start_timer = generate_text(sdl_renderer,m5x7,timer_str,{255,0,0,255});
        round_start_timer.scale = scale;
        round_start_timer.position = {1280/2-round_start_timer.get_draw_rect().w/2,720/2-round_start_timer.get_draw_rect().h/2};
        SDL_RenderCopy(sdl_renderer,round_start_timer.texture,NULL,&round_start_timer.get_draw_rect());
    }


}

void render_pregame_screen(SDL_Renderer *sdl_renderer, overall_game_manager &gms, double time_to_start) {
    static int connected_last_tick=-1;
    static generic_drawable connection_text = generate_text(sdl_renderer,m5x7,"Players connected (" + std::to_string(connected_last_tick) + "/2)",{255,255,200});
    connection_text.scale = {2,2};
    
    if (connected_last_tick != gms.connected_players) {
        connection_text = generate_text(sdl_renderer,m5x7,"Players connected (" + std::to_string(gms.connected_players)+"/2)",{255,255,200});
        connected_last_tick=gms.connected_players;
        connection_text.scale = {2,2};
        connection_text.position = {1280/2-connection_text.get_draw_rect().w/2,380};
    }
            
    SDL_SetRenderDrawColor(sdl_renderer,255,255,0,255);
    SDL_RenderClear(sdl_renderer);
    SDL_Rect dest = {0,0,1280,720};
    SDL_RenderCopy(sdl_renderer,textures[PREGAME_TEXTURE],NULL,&dest);
    SDL_RenderCopy(sdl_renderer,connection_text.texture,NULL,&connection_text.get_draw_rect());

    if (gms.counting_down_to_game_start) {
        std::string str = std::to_string(time_to_start);
        if (str.size() > 4) {
            str.erase(str.begin()+4,str.end());
        }
        
        generic_drawable countdown_clock = generate_text(sdl_renderer,m5x7,str,{255,240,40});
        countdown_clock.scale = {4,4};
        countdown_clock.position = {1280/2-countdown_clock.get_draw_rect().w/2,480};
        SDL_RenderCopy(sdl_renderer,countdown_clock.texture,NULL,&countdown_clock.get_draw_rect());
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
        // should this be before or after???
        // after i think because this function shouldn't have to worry about previous ticks
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

