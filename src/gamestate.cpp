
struct netstate_info_t;

enum TILETYPE {
    TT_NONE,
    TT_GROUND,
    TT_WALL,
    TT_BOMBSITE,
    TT_A,
    TT_AA,
    TT_ARROW_UPLEFT,
    TT_ARROW_UPRIGHT,
    TT_COUNT
};

enum ROUND_STATE {
    ROUND_WARMUP,
    ROUND_BUYTIME,
    ROUND_PLAYING,
    ROUND_ENDTIME,
    ROUND_STATE_COUNT
};

struct map_t {
    i32 wall_count=0;
    v2i walls[512];

    u8 tiles[MAX_MAP_SIZE][MAX_MAP_SIZE] = {{TT_NONE}};

    i32 bombsite_count=0;
    v2i bombsite[64];

    i32 spawn_counts[TEAM_COUNT];
    v2i spawns[TEAM_COUNT][32];
    bool static_texture_generated=false;
};

// should be called world state instead of game state
struct game_state {
    // this is map data, not game state data!
    // and can be removed completely    
    i32 player_count=0;
    character players[8];

    i32 bullet_count=0;
    bullet_t bullets[64];

    // should abstract these net variables out eventually
    i32 score[8] = {0};
    int tick=0;

    i32 bomb_planted_tick=0;
    entity_id bomb_planter = ID_DONT_EXIST;
    v2i bomb_plant_location={0,0};

    i32 bomb_defused_tick=0;
    entity_id bomb_defuser = ID_DONT_EXIST;

    bool bomb_defused=false;
    bool bomb_planted=false;

    i32 round_start_tick=0;
    i32 round_end_tick=0;
    i32 buytime_end_tick=0;
    i32 one_remaining_tick=0;
    ROUND_STATE round_state = ROUND_WARMUP;

    // let the player get into negative money
    // and give them a prompt that if they don't make their money back
    // the feds will come after them
    i32 money[MAX_PLAYERS]={0};

    void update(netstate_info_t &c, double delta);
};

constexpr i32 game_state_permanent_data_size = offsetof(game_state, player_count);
constexpr i32 game_state_transient_data_size = sizeof(game_state) - game_state_permanent_data_size;

struct snapshot_t {
    game_state gms;
    i32 last_processed_command_tick[16];
    i32 map;
};


enum GMS {
    GAME_HASNT_STARTED,
    PREGAME_SCREEN,
    GAME_PLAYING,
    GAMESTATE_COUNT
};

enum GAME_MODE {
    GAME_MODE_STRIKE,
    GAME_MODE_JOSHFARE,
    GAME_MODE_COUNT,
};

// the reason this struct is 100% SEPARATE from game_state is because we don't want
// to worry about these details when writing generic update code for the game.
// we need an interface or something for interaction between the two
struct overall_game_manager {
    i32 connected_players=0;
    GMS state=GAME_HASNT_STARTED;
    GAME_MODE gmode= GAME_MODE_STRIKE;
    
    int round_end_tick=-1;

    LARGE_INTEGER game_start_time;
    bool counting_down_to_game_start=false;
};


struct command_node {
    command_t cmd;
    bool verified;
};

// bridge between gamestate and network
struct netstate_info_t {
    // fuck it
    std::vector<command_node> command_stack;    
    std::vector<snapshot_t> snapshots;
    
    bool authoritative=false;
    int interp_delay=0;

    overall_game_manager gms;

    bool do_charas_interp[16]={true};
};


global_variable game_state gs;
global_variable map_t mp;


static void add_player() {
    entity_id id = (entity_id)(gs.player_count++);
    gs.players[id] = create_player({0,0},id);
}

static void add_wall(v2i wall) {
    mp.walls[mp.wall_count++] = wall;
}

static void add_bullet(v2 pos, float rot, entity_id shooter_id) {
    bullet_t &bullet = gs.bullets[gs.bullet_count++];
    bullet.position = pos;
    bullet.vel = convert_angle_to_vec(rot)*600.f;
    bullet.shooter_id = shooter_id;
}


enum {
    MAP_DE_DUST2,
    MAP_COUNT
};

void load_permanent_data_from_map(i32 map) {
    SDL_Surface *level_surface = IMG_Load("res/map.png");

    mp = {};
    
    i32 &ct_spawn_count=mp.spawn_counts[COUNTER_TERRORIST];
    v2i *ct_spawns=mp.spawns[COUNTER_TERRORIST];
    i32 &t_spawn_count=mp.spawn_counts[TERRORIST];
    v2i *t_spawns = mp.spawns[TERRORIST];

    ct_spawn_count=0;
    t_spawn_count=0;


    /*glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_GAME_WORLD]);
    glClearColor(1.f, 1.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(sh_textureProgram);
    */
    for (i32 x=0; x<32; x++) {
        for (i32 y=0; y<32; y++) {
            Color col = {0,0,0,0};
            Uint32 data = getpixel(level_surface, x,y);
            SDL_GetRGBA(data, level_surface->format, &col.r, &col.g, &col.b, &col.a);
            if (col == Color(0,0,0,255)) {
                add_wall({x,y});
                mp.tiles[x][y] = TT_WALL;
            } else if (col == Color(255,0,0,255)) {
                t_spawns[t_spawn_count++] = {x,y};
            } else if (col == Color(255,255,0,255)) {
                ct_spawns[ct_spawn_count++] = {x,y};
            } else if (col == Color(0,0,255,255)) {
                mp.bombsite[mp.bombsite_count++] = {x,y};
                mp.tiles[x][y] = TT_BOMBSITE;
            } else if (col.a == 0) {
                mp.tiles[x][y] = TT_GROUND;
            } else if (col == Color(241,0,255)) {
                mp.tiles[x][y] = TT_AA;
            } else if (col == Color(0,255,0)) {
                mp.tiles[x][y] = TT_A;
            } else if (col == Color(248,130,255)) {
                mp.tiles[x][y] = TT_ARROW_UPLEFT;
            } else if (col == Color(120,255,120)) {
                mp.tiles[x][y] = TT_ARROW_UPRIGHT;
            } else {
                mp.tiles[x][y] = TT_GROUND;
            }
        }
    }
    SDL_FreeSurface(level_surface);
    mp.static_texture_generated=false;    
}

void gamestate_load_map(overall_game_manager &gms, i32 map) {
    gms.state = GAME_PLAYING;

    gs.bullet_count=0;
    gs.one_remaining_tick=0;
    gs.bomb_planted_tick=0;
    gs.bomb_defused_tick=0;
    gs.bomb_planted=false;
    gs.bomb_defused=false;
    gs.round_state = ROUND_BUYTIME;

    load_permanent_data_from_map(map);
    
    bool create_all_charas = gs.player_count == gms.connected_players ? false : true;
    gs.player_count=0;

    i32 side_count[TEAM_COUNT]={0};    
    i32 unused_count[TEAM_COUNT]={mp.spawn_counts[TERRORIST],mp.spawn_counts[COUNTER_TERRORIST]};
    v2i unused_spawns[TEAM_COUNT][32];
    memcpy(unused_spawns[TERRORIST],mp.spawns[TERRORIST],32*sizeof(v2i));
    memcpy(unused_spawns[COUNTER_TERRORIST],mp.spawns[COUNTER_TERRORIST],32*sizeof(v2i));

    for (i32 ind=0; ind<gms.connected_players; ind++) {
        gs.money[ind] = FIRST_ROUND_START_MONEY;
        if (create_all_charas == false) {
            character old_chara = gs.players[ind];
            
            add_player();
            gs.players[ind] = create_player({0,0},old_chara.id);
            gs.players[ind].color = old_chara.color;
        } else {
            add_player();
        
            int total_color_points=rand() % 255 + (255+200);
            gs.players[gs.player_count-1].color.r = (u8)(rand()%MIN(255,total_color_points));
            total_color_points-=gs.players[gs.player_count-1].color.r;
            gs.players[gs.player_count-1].color.g = (u8)(rand()%MIN(255,total_color_points));
            total_color_points-=gs.players[gs.player_count-1].color.g;
            gs.players[gs.player_count-1].color.b = (u8)total_color_points;
        }

        TEAM n_team;
        if (side_count[TERRORIST] == side_count[COUNTER_TERRORIST]) {
            n_team = (TEAM)(rand() % (i32)TEAM_COUNT);
        } else if (side_count[TERRORIST] < side_count[COUNTER_TERRORIST]) {
            n_team = TERRORIST;
        } else {
            n_team = COUNTER_TERRORIST;
        }

        i32 rand_spawn = rand() % unused_count[n_team];
        gs.players[ind].pos = unused_spawns[n_team][rand_spawn]*64;
        unused_spawns[n_team][rand_spawn] = unused_spawns[n_team][rand_spawn-1];
        unused_count[n_team]--;
        side_count[n_team]++;
        gs.players[ind].team = n_team;
    }

    // start in 5 seconds
    gs.round_start_tick = gs.tick + 1;
    gs.buytime_end_tick = gs.round_start_tick + (BUYTIME_LENGTH * 60);
}

internal void on_bomb_plant_finished(entity_id id, i32 tick) {
    gs.bomb_planted_tick = tick;
    gs.bomb_planter = id;
    gs.bomb_plant_location = gs.players[id].pos + v2(32,40);
    gs.bomb_planted=true;
}

internal void on_bomb_defuse_finished(entity_id id, i32 tick) {
    gs.bomb_defused_tick = tick;
    gs.bomb_defuser = id;
    gs.bomb_defused=true;
    gs.bomb_planted=false;
}

void game_state_end_round() {
    gs.round_state = ROUND_ENDTIME;
    gs.round_end_tick = gs.tick;
}


#define GAME_CAN_END
void game_state::update(netstate_info_t &c, double delta) {
    if (tick < round_start_tick) {
        return;
    } if (round_state == ROUND_BUYTIME) {
        if (tick >= buytime_end_tick) {
            round_state = ROUND_PLAYING;
        }
    }
    if (c.authoritative && round_state == ROUND_ENDTIME) {
        if (tick >= round_end_tick + ROUND_ENDTIME_LENGTH*60) {
            gamestate_load_map(c.gms,MAP_DE_DUST2);
        }
    }

    if (bomb_planted && tick == bomb_planted_tick+BOMB_TIME_TO_DETONATE*60) {
        // explode bomb
        bomb_planted=false;
        game_state_end_round();
        
        FORn(players,player_count,player) {
            double dist_to_bomb = distance_between_points(player->pos,bomb_plant_location);
            if (dist_to_bomb < BOMB_RADIUS) {
                if (dist_to_bomb < BOMB_KILL_RADIUS) {
                    player->health = 0;
                } else {
                    int damage = (int)((dist_to_bomb-BOMB_KILL_RADIUS) / (BOMB_RADIUS-BOMB_KILL_RADIUS) * 100.0);
                    player->health -= damage;
                }
            }
        }
        if (!c.authoritative) {
            queue_sound(SfxType::BOMB_DETONATE_SFX,bomb_planter,tick);
        }
    }

    // add commands as the server
    if (c.authoritative) {        
        FORn(players,player_count,player) {
            if (player->health <= 0 && player->curr_state != character::DEAD) {
                command_t kill_command = {CMD_DIE,false,tick,player->id};
                command_node node = {kill_command,true};
                c.command_stack.push_back(node);
            }
        }
    }
    
    // process commands for this tick
    for (command_node &node: c.command_stack) {
        if (node.cmd.tick == tick) {
            if (round_state == ROUND_BUYTIME) {
                if (node.cmd.code != CMD_BUY) continue;
            }
            process_command(&players[node.cmd.sending_id],node.cmd);
        }
    }

    i32 planted_before_tick = bomb_planted;
    i32 defused_before_tick = bomb_defused;
    // interp delay in ticks
    FOR(players,player_count) {
        if (obj->curr_state == character::DEAD) continue;
        bool planting_before_update = obj->curr_state == character::PLANTING;
        update_player(obj,delta,mp.wall_count,mp.walls,player_count,players,mp.bombsite_count,mp.bombsite,tick,planted_before_tick,bomb_plant_location);
    }

    FOR(bullets,bullet_count) {
        obj->position += obj->vel*delta;
        // if a bullet hits a wall delete it
        FORn(mp.walls,mp.wall_count,wall) {
            fRect b_hitbox = {obj->position.x,obj->position.y,16.f,16.f};
            fRect wall_rect = {wall->x*64.f,wall->y*64.f,64.f,64.f};
            if (rects_collide(b_hitbox,wall_rect)) {
                *obj = bullets[--bullet_count];
                break;
            }
        }
        FORn(players,player_count,player) {
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

    // if we want to make bomb plant/explosion completely authoritative we just put the code in this
    // c.authoritative check
    if (bomb_planted) {
        if (!planted_before_tick) {
            if (c.authoritative) {
                queue_sound(SfxType::PLANT_FINISHED_SFX,bomb_planter,tick);
            }
        }
    } if (bomb_defused) {
        if (!defused_before_tick) {
            if (c.authoritative) {
                queue_sound(SfxType::BOMB_DEFUSE_FINISH_SFX,ID_SERVER,tick);
                game_state_end_round();
            }
        }
    }
    
    if (c.authoritative && round_state != ROUND_ENDTIME) {
        // check how many players are alive
        i32 living[TEAM_COUNT]={0};
        FORn(players,player_count,player) {
            if (player->curr_state != character::DEAD) {
                living[player->team]++;
            }
        }
        if ((!bomb_planted && living[TERRORIST]==0) || living[COUNTER_TERRORIST]==0) {
            game_state_end_round();
        }
    }
}


v2 pp;
int collision_point_sort(const void *v1, const void*v2) {
    v2i p1 = *(v2i*)v1;
    v2i p2 = *(v2i*)v2;

    return (int)((get_angle_to_point(pp,p1)-get_angle_to_point(pp,p2))*1000.f);
}


struct col_target {
    v2 pt;
    double angle;
};


struct {
    std::vector<v2> raycast_points;
    std::vector<segment> segments;
} client_sided_render_geometry;

bool draw_shadows=true;

void render_game_state(character *render_from_perspective_of=nullptr, camera_t *game_camera=nullptr) {
    // Render start
    glClearColor(1.f, 0.0f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);


    v2i cam_mod = {0,0};
    if (game_camera) cam_mod = v2i(1280/2,720/2) - v2i(game_camera->pos);
    iRect map_rect = {cam_mod.x,cam_mod.y,MAX_MAP_SIZE*64,MAX_MAP_SIZE*64};
    iRect cam_rect = {(i32)game_camera->pos.x - (1280/2),(i32)game_camera->pos.y - (720/2),1280,720};

    glUseProgram(sh_textureProgram);
    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_GAME_WORLD]);
    glClearColor(1.f, 1.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    for (i32 x=0; x<MAX_MAP_SIZE; x++) {
        for (i32 y=0; y<MAX_MAP_SIZE; y++) {
            i32 type = mp.tiles[x][y];
            iRect dest = {x*64,y*64,64,64};
            if (rects_collide(dest,cam_rect) == false) continue;
            dest.x += cam_mod.x;
            dest.y += cam_mod.y;
            
            iRect src={0,0,16,16};
            if (type == TT_GROUND) {
                src={16,16,16,16};
            } else if (type == TT_WALL) {
                src={16,0,16,16};
            } else if (type == TT_BOMBSITE) {
                src={32,0,16,16};
            } else if (type == TT_A) {
                src={32,16,16,16};
            } else if (type == TT_AA) {
                src={32,32,16,16};
            } else if (type == TT_ARROW_UPLEFT) {
                src={48,16,16,16};
            } else if (type == TT_ARROW_UPRIGHT) {
                src={48,32,16,16};
            }
            GL_DrawTexture(gl_textures[TILE_TEXTURE],dest,src);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    
    // lol is this objectively horrible?? its a massive texture so its a great idea but poor execution
    // i mean who rly cares tho!
    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_GAME_OBJECTS]);
    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLuint tintLoc_col = glGetUniformLocation(sh_textureProgram, "colorMod");
    
    for (i32 id=0; id<gs.player_count; id++) {
        character &p = gs.players[id];
        if (p.visible == false) continue;

        iRect src_rect = {p.curr_state == character::PUNCHING ? 64 : p.curr_state == character::SHIELD ? 96 : 0,p.curr_state == character::TAKING_DAMAGE ? 32 : 0,32,32};
        iRect dest_rect = {(int)p.pos.x+cam_mod.x,(int)p.pos.y+cam_mod.y,64,64};
        if (p.curr_state == character::MOVING) {
            src_rect.x = p.animation_timer > 0.2/2.0? 32 : 0;
        }
        if (p.damage_timer) {
            u8 g = (u8)lerp(p.color.g-75.f,(float)p.color.g,(0.5f-(float)p.damage_timer)*(1.f/0.5f));
            u8 b = (u8)lerp(p.color.b-105.f,(float)p.color.b,(0.5f-(float)p.damage_timer)*(1.f/0.5f));
            glUniform4f(tintLoc_col, p.color.r,g,b,1.0f);
        } else {
            glUniform4f(tintLoc_col, p.color.r,p.color.g,p.color.b,1.0f);
        }
        GL_DrawTextureEx(gl_textures[PLAYER_TEXTURE],dest_rect,src_rect,p.flip);
    }
    glUniform4f(tintLoc_col, 1.0f,1.0f,1.0f,1.0f);

    for (i32 ind=0; ind<gs.bullet_count; ind++) {
        iRect rect = {(int)gs.bullets[ind].position.x+cam_mod.x,(int)gs.bullets[ind].position.y+cam_mod.y,16,16};
        iRect center = {8,8};
        float rad_rot = convert_vec_to_angle(gs.bullets[ind].vel)+PI;

        GL_DrawTextureEx(gl_textures[BULLET_TEXTURE],rect,{0,0,0,0},false,false,rad_rot);
    }

    // if the bomb has been planted draw the bomb
    if (gs.bomb_planted) {
        iRect dest = {(int)gs.bomb_plant_location.x-12+cam_mod.x,(int)gs.bomb_plant_location.y-12+cam_mod.y,24,24};
        GL_DrawTexture(gl_textures[ITEM_TEXTURE],dest,{16,16,16,16});
    }
    
    if (render_from_perspective_of != nullptr && client_sided_render_geometry.raycast_points.size()>0 && draw_shadows) {
        const SDL_Color black = {0,0,0,255};
        const SDL_Color white = {255,255,255,255};
        const SDL_Color invisible={0,0,0,0};
        const u8 shadow_visibility = 80;

        //SDL_SetRenderTarget(sdl_renderer,textures[SHADOW_TEXTURE]);
        //SDL_RenderClear(sdl_renderer);
        //SDL_SetRenderDrawColor(sdl_renderer,80,80,80,255);
        //SDL_RenderFillRect(sdl_renderer,NULL);
        
        v2i p_pos = render_from_perspective_of->pos + v2(32,32);
        std::vector<col_target> dests;

        for (auto t: client_sided_render_geometry.raycast_points) {
            col_target c1,c2,c3;

            c1.pt = t;
            c1.angle = get_angle_to_point(p_pos,t);
            intersect_props col = get_collision(p_pos,t,client_sided_render_geometry.segments);

            if (!col.collides) {
                printf("ERROR\n");
                c1.pt = v2(c1.pt-p_pos).normalize() * 1500.f + p_pos;
            } else {
                c1.pt = v2(col.collision_point.x,col.collision_point.y);
                if (col.collision_point != t) {
                    dests.push_back(c1);
                    continue;
                }
            }

            c2.angle = c1.angle + 0.005f;
            c3.angle = c1.angle - 0.005f;
            // NOTE: the length needs to be AT LEAST the screen width!
            c2.pt = (dconvert_angle_to_vec(c2.angle) * 2000.0) + p_pos;
            c3.pt = (dconvert_angle_to_vec(c3.angle) * 2000.0) + p_pos;
            intersect_props col_2 = get_collision(p_pos,c2.pt,client_sided_render_geometry.segments);
            intersect_props col_3 = get_collision(p_pos,c3.pt,client_sided_render_geometry.segments);
            if (col_2.collides) {
                c2.pt = col_2.collision_point;
            }
            if (col_3.collides) {
                c3.pt = col_3.collision_point;
            }

            dests.push_back(c3);
            dests.push_back(c1);
            dests.push_back(c2);
        }


        // if we sort beforehand and then just add inorder we can reduce the sort size by
        // a factor of 3
        std::sort(dests.begin(),dests.end(),[](auto &left, auto &right) {
            return left.angle < right.angle;
        });

        glUseProgram(sh_colorProgram);
        GLuint colorLoc = glGetUniformLocation(sh_colorProgram,"color");
        glUniform4f(colorLoc,1.0f,1.0f,1.0f,1.0f);
        
        glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffers[FB_SHADOW]);
        glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(sh_colorProgram);
        glUniform4f(glGetUniformLocation(sh_colorProgram,"color"),1.0f,1.0f,1.0f,1.0f);

        // NOTE: this will be much much faster if we only populate the vertices in the loop
        // and then send it all at once to the GPU
        for (i32 n=0;n<dests.size();n++) {
            v2i pt=dests[n].pt;
            v2i prev_point=n==0?dests.back().pt:dests[n-1].pt;
            float vertices[] = {
                (float)p_pos.x+cam_mod.x,(float)p_pos.y+cam_mod.y, 0.0f,
                (float)pt.x+cam_mod.x,  (float)pt.y+cam_mod.y,   0.0f,
                (float)prev_point.x+cam_mod.x, (float)prev_point.y+cam_mod.y, 0.0f
            };
            glBindVertexArray(gl_varrays[SHADOW_VAO]);
            glBindBuffer(GL_ARRAY_BUFFER,gl_vbuffers[SHADOW_VBO]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3 * sizeof(float),0);
            glEnableVertexAttribArray(0);

            glDrawArrays(GL_TRIANGLES,0,3);
            glBindBuffer(GL_ARRAY_BUFFER,0);
            glBindVertexArray(0);
        }
        
        /*
          for (auto &dest: dests) {
          v2i pt = dest.pt;
          SDL_Rect dest = {(i32)pt.x-8,(i32)pt.y-8,16,16};
          SDL_SetRenderDrawColor(sdl_renderer,255,0,0,255);
          SDL_RenderDrawLine(sdl_renderer,(int)p_pos.x,(int)p_pos.y,(int)pt.x,(int)pt.y);
          SDL_RenderCopy(sdl_renderer,textures[RAYCAST_DOT_TEXTURE],NULL,&dest);
          }
        */
        
        dests.clear();
    }

    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glUseProgram(sh_textureProgram);
    GL_DrawTextureEx(gl_textures[TX_GAME_WORLD],{0,0,1280,720},{0,0,0,0},false,true);

    if (draw_shadows) {
        glUseProgram(sh_modProgram);
        GLuint tex1_loc = glGetUniformLocation(sh_modProgram, "_texture1");
        GLuint tex2_loc = glGetUniformLocation(sh_modProgram, "_texture2");
        
        glActiveTexture(GL_TEXTURE0); // Texture unit 0
        glBindTexture(GL_TEXTURE_2D, gl_textures[TX_SHADOW]);
        glUniform1i(tex1_loc, 0);
        glActiveTexture(GL_TEXTURE1); // Texture unit 1
        glBindTexture(GL_TEXTURE_2D, gl_textures[TX_GAME_OBJECTS]);
        glUniform1i(tex2_loc, 1);

        float vertices[] = {
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            1280.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            1280.0f, 720.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
        };
        glm::mat4 model = glm::mat4(1.0f);
        GLint transformLoc = glGetUniformLocation(sh_modProgram,"model");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(gl_varrays[TEXTURE_VAO]);
        glBindBuffer(GL_ARRAY_BUFFER, gl_vbuffers[TEXTURE_VBO]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
        glBindVertexArray(gl_varrays[TEXTURE_VAO]);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(0));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER,0);    
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
    } else {
        GL_DrawTextureEx(gl_textures[TX_GAME_OBJECTS],{0,0,0,0},{0,0,0,0},false,true);
    }

    glUseProgram(sh_textureProgram);
    // gui elements
    if (gs.round_state == ROUND_BUYTIME) {
        double time_until_start = (gs.buytime_end_tick-gs.tick) * (1.0/60.0);
        std::string timer_str=std::to_string((int)ceil(time_until_start));;
        local_persist generic_drawable round_start_timer={};
        round_start_timer = generate_text(m5x7,timer_str,{255,0,0,255},round_start_timer.gl_texture);
        round_start_timer.scale = {2,2};
        round_start_timer.position = {1280/2-round_start_timer.get_draw_irect().w/2,24};
        GL_DrawTexture(round_start_timer.gl_texture,round_start_timer.get_draw_irect());
        std::cout << timer_str << std::endl;

    } else if (gs.round_state == ROUND_PLAYING && gs.bomb_planted) {
        double time_until_detonation = (gs.bomb_planted_tick+(BOMB_TIME_TO_DETONATE*60) - gs.tick) * (1.0/60.0);
        std::string timer_str=std::to_string((int)ceil(time_until_detonation));
        local_persist generic_drawable detonation_timer={};
        detonation_timer = generate_text(m5x7,timer_str,{255,0,0,255},detonation_timer.gl_texture);
        detonation_timer.scale = {2,2};
        detonation_timer.position = {1280/2-detonation_timer.get_draw_irect().w/2,24};
        GL_DrawTexture(detonation_timer.gl_texture,detonation_timer.get_draw_irect());
    }

}

void render_pregame_screen(overall_game_manager &gms, double time_to_start) {
    glUseProgram(sh_textureProgram);
    static int connected_last_tick=-1;
    static generic_drawable connection_text;
    
    if (connected_last_tick == -1 || connected_last_tick != gms.connected_players) {
        connection_text = generate_text(m5x7,"Players connected (" + std::to_string(gms.connected_players)+"/2)",{255,255,200});
        connected_last_tick=gms.connected_players;
        connection_text.scale = {1,1};
        connection_text.position = {1280/2-connection_text.get_draw_irect().w/2,380};
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glClearColor(0.f, 0.25f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(sh_textureProgram);
    GL_DrawTextureEx(gl_textures[PREGAME_TEXTURE]);
    GL_DrawTextureEx(connection_text.gl_texture,connection_text.get_draw_irect());

    if (gms.counting_down_to_game_start) {
        std::string str = std::to_string(time_to_start);
        if (str.size() > 4) {
            str.erase(str.begin()+4,str.end());
        }
        
        static generic_drawable countdown_clock;
        countdown_clock = generate_text(m5x7,str,{255,240,40},countdown_clock.gl_texture);
        countdown_clock.scale = {2,2};
        countdown_clock.position = {1280/2-countdown_clock.get_draw_irect().w/2,480};
        GL_DrawTexture(countdown_clock.gl_texture,countdown_clock.get_draw_irect());
    }
}

// loads the first snapshot found before the input time
void find_and_load_gamestate_snapshot(game_state &gst, netstate_info_t &c, int start_tick) {
    for (i32 ind=((i32)c.snapshots.size())-1; ind>=0;ind--) {
        if (c.snapshots[ind].gms.tick<=start_tick) {
            gst = c.snapshots[ind].gms;
            return;
        }
    }
    printf("ERROR: Couldn't find snapshot to rewind to %d!\n",start_tick);
}

void load_game_state_up_to_tick(game_state &gst, netstate_info_t &c, int target_tick, bool overwrite_snapshots=true) {
    while(gst.tick<target_tick) {
        // should this be before or after???
        // after i think because this function shouldn't have to worry about previous ticks
        // crazy inefficient lol
        // TODO: fix this
        if (overwrite_snapshots) {
            for (snapshot_t &snap: c.snapshots) {
                if (snap.gms.tick==gst.tick) {
                    snap.gms = gst;
                    break;
                }
            }
        }
        gst.update(c,1.0/60.0);
        gst.tick++;
    }
}
