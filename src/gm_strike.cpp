
struct strike_map_t {
    i32 wall_count=0;
    v2i walls[512];

    u8 tiles[MAX_MAP_SIZE][MAX_MAP_SIZE] = {{TT_NONE}};

    i32 bombsite_count=0;
    v2i bombsite[64];

    i32 spawn_counts[TEAM_COUNT];
    v2i spawns[TEAM_COUNT][32];

    void add_wall(v2i pos) {
        walls[wall_count++] = pos;
    }

    enum {
        MAP_DE_DUST2,
        MAP_COUNT
    };

    void load_permanent_data_from_map(i32 map);
    void load_render_geometry();
};


void strike_map_t::load_permanent_data_from_map(i32 map) {
    SDL_Surface *level_surface = IMG_Load("res/map.png");

    *this = {};

    for (i32 x=0; x<32; x++) {
        for (i32 y=0; y<32; y++) {
            Color col = {0,0,0,0};
            Uint32 data = getpixel(level_surface, x,y);
            SDL_GetRGBA(data, level_surface->format, &col.r, &col.g, &col.b, &col.a);
            if (col == Color(0,0,0,255)) {
                add_wall({x,y});
                tiles[x][y] = TT_WALL;
            } else if (col == Color(255,0,0,255)) {
                spawns[TERRORIST][spawn_counts[TERRORIST]++] = {x,y};
            } else if (col == Color(255,255,0,255)) {
                spawns[COUNTER_TERRORIST][spawn_counts[COUNTER_TERRORIST]++] = {x,y};
            } else if (col == Color(0,0,255,255)) {
                bombsite[bombsite_count++] = {x,y};
                tiles[x][y] = TT_BOMBSITE;
            } else if (col.a == 0) {
                tiles[x][y] = TT_GROUND;
            } else if (col == Color(241,0,255)) {
                tiles[x][y] = TT_AA;
            } else if (col == Color(0,255,0)) {
                tiles[x][y] = TT_A;
            } else if (col == Color(248,130,255)) {
                tiles[x][y] = TT_ARROW_UPLEFT;
            } else if (col == Color(120,255,120)) {
                tiles[x][y] = TT_ARROW_UPRIGHT;
            } else {
                tiles[x][y] = TT_GROUND;
            }
        }
    }
    SDL_FreeSurface(level_surface);
}

void strike_map_t::load_render_geometry() {
    // regenerate raycast points
    auto &rps = client_sided_render_geometry.raycast_points;
    rps.clear();
    client_sided_render_geometry.segments.clear();
    for (i32 n=0; n<wall_count; n++) {
        v2i wall=walls[n];
        rps.push_back(v2(wall.x,wall.y)*64);
        rps.push_back(v2(wall.x+1,wall.y)*64);
        rps.push_back(v2(wall.x,wall.y+1)*64);
        rps.push_back(v2(wall.x+1,wall.y+1)*64);
    }
    printf("Points before: %d\n",(i32)rps.size());
    std::sort( rps.begin(), rps.end(), [](auto&left,auto&right){
        return *(double*)&left < *(double*)&right;
    });
    std::unordered_map<double,int> p_set;
    for (auto &p:rps) p_set[*(double*)&p]++;

    // removes all points surrounded by tiles
    for (auto pt=rps.begin();pt<rps.end();pt++) {
        i32 dupes=0;
        if (pt!=rps.end()-1 && *(pt+1) != *pt) continue;
        auto it=pt;
        while (it < rps.end()) {
            it++;
            if (*it == *pt) {
                dupes++;
            } else {
                break;
            }
        }
                    
        if (dupes==1 || dupes == 3) {
            rps.erase(pt,it);
            pt--;
        } else {
            pt=it-1;
        }
                    
    }
                
    for (i32 ind=0; ind<wall_count; ind++) {
        v2 wall=walls[ind];
        v2 p1 = v2(wall.x,wall.y)*64;
        v2 p2 = v2(wall.x+1,wall.y)*64;
        v2 p3 = v2(wall.x,wall.y+1)*64;
        v2 p4 = v2(wall.x+1,wall.y+1)*64;

        bool fp1 = p_set[*(double*)&p1] != 4;
        bool fp2 = p_set[*(double*)&p2] != 4;
        bool fp3 = p_set[*(double*)&p3] != 4;
        bool fp4 = p_set[*(double*)&p4] != 4;

        if (fp1) {
            if (fp2)
                client_sided_render_geometry.segments.push_back({p1,p2});
            if (fp3)
                client_sided_render_geometry.segments.push_back({p1,p3});
        } if (fp4) {
            if (fp2)
                client_sided_render_geometry.segments.push_back({p2,p4});
            if (fp3)
                client_sided_render_geometry.segments.push_back({p3,p4});
        }
    }
                
    std::sort( rps.begin(), rps.end(), [](auto&left,auto&right){
        return *(double*)&left < *(double*)&right;
    });

    rps.erase(unique(rps.begin(),rps.end()),rps.end());
                
    printf("Points after: %d\n",(i32)rps.size());

    std::sort( client_sided_render_geometry.segments.begin(), client_sided_render_geometry.segments.end(), [](auto&left,auto&right){
        return (*(double*)&left.p1 + *(double*)&left.p2) < (*(double*)&right.p1 + *(double*)&right.p2);
    });
    i32 s = (i32)client_sided_render_geometry.segments.size();
    client_sided_render_geometry.segments.erase(unique(client_sided_render_geometry.segments.begin(),client_sided_render_geometry.segments.end()),client_sided_render_geometry.segments.end());
    printf("Removed %d segments\n",(i32)client_sided_render_geometry.segments.size()-s);
}


struct gm_strike : game_state {
    i32 bullet_count=0;
    bullet_t bullets[64];

    // should abstract these net variables out eventually
    i32 score[8] = {0};

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

    enum ROUND_STATE {
        ROUND_WARMUP,
        ROUND_BUYTIME,
        ROUND_PLAYING,
        ROUND_ENDTIME,
        ROUND_STATE_COUNT
    };
    ROUND_STATE round_state = ROUND_WARMUP;

    // let the player get into negative money
    // and give them a prompt that if they don't make their money back
    // the feds will come after them
    i32 money[MAX_PLAYERS]={0};

    void add_bullet(v2 pos, float rot, entity_id shooter_id);
    void add_player();

    void load_map(overall_game_manager &gms, i32 map);
    void on_bomb_plant_finished(entity_id id, i32 fin_tick);
    void on_bomb_defuse_finished(entity_id id, i32 fin_tick);
    void end_round();

    void load_up_to_tick(netstate_info_t &c, int target_tick, bool overwrite_snapshots=true);
    void find_and_load_snapshot(netstate_info_t &c, int start_tick);
    void update(netstate_info_t &c, double delta);
    void render(character *render_from_perspective_of=nullptr, camera_t *game_camera=nullptr);
};

void gm_strike::add_bullet(v2 pos, float rot, entity_id shooter_id) {
    bullet_t &bullet = bullets[bullet_count++];
    bullet.position = pos;
    bullet.vel = convert_angle_to_vec(rot)*600.f;
    bullet.shooter_id = shooter_id;
}

void gm_strike::add_player() {
    entity_id id = (entity_id)(player_count++);
    players[id] = create_player({0,0},id);
}

// TODO: replace _mp with mp
void gm_strike::load_map(overall_game_manager &gms, i32 map) {
    gms.state = GAME_PLAYING;

    bullet_count=0;
    one_remaining_tick=0;
    bomb_planted_tick=0;
    bomb_defused_tick=0;
    bomb_planted=false;
    bomb_defused=false;
    round_state = ROUND_BUYTIME;

    strike_map_t &mp = *(strike_map_t*)permanent_game_state;
    mp = {};

    mp.load_permanent_data_from_map(map);
    
    bool create_all_charas = player_count == gms.connected_players ? false : true;
    player_count=0;

    i32 side_count[TEAM_COUNT]={0};
    i32 unused_count[TEAM_COUNT]={mp.spawn_counts[TERRORIST],mp.spawn_counts[COUNTER_TERRORIST]};
    v2i unused_spawns[TEAM_COUNT][32];
    memcpy(unused_spawns[TERRORIST],mp.spawns[TERRORIST],32*sizeof(v2i));
    memcpy(unused_spawns[COUNTER_TERRORIST],mp.spawns[COUNTER_TERRORIST],32*sizeof(v2i));

    for (i32 ind=0; ind<gms.connected_players; ind++) {
        money[ind] = FIRST_ROUND_START_MONEY;
        if (create_all_charas == false) {
            character old_chara = players[ind];
            
            add_player();
            players[ind] = create_player({0,0},old_chara.id);
            players[ind].color = old_chara.color;
        } else {
            add_player();
        
            int total_color_points=rand() % 255 + (255+200);
            players[player_count-1].color.r = (u8)(rand()%MIN(255,total_color_points));
            total_color_points-=players[player_count-1].color.r;
            players[player_count-1].color.g = (u8)(rand()%MIN(255,total_color_points));
            total_color_points-=players[player_count-1].color.g;
            players[player_count-1].color.b = (u8)total_color_points;
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
        players[ind].pos = unused_spawns[n_team][rand_spawn]*64;
        unused_spawns[n_team][rand_spawn] = unused_spawns[n_team][rand_spawn-1];
        unused_count[n_team]--;
        side_count[n_team]++;
        players[ind].team = n_team;
    }

    // start in 5 seconds
    round_start_tick = tick + 1;
    buytime_end_tick = round_start_tick + (BUYTIME_LENGTH * 60);
}

void gm_strike::on_bomb_plant_finished(entity_id id, i32 fin_tick) {
    bomb_planted_tick = fin_tick;
    bomb_planter = id;
    bomb_plant_location = players[id].pos + v2(32,40);
    bomb_planted=true;
}

void gm_strike::on_bomb_defuse_finished(entity_id id, i32 fin_tick) {
    bomb_defused_tick = fin_tick;
    bomb_defuser = id;
    bomb_defused=true;
    bomb_planted=false;
}

void gm_strike::end_round() {
    round_state = ROUND_ENDTIME;
    round_end_tick = tick;
}


internal void on_bomb_plant_finished(entity_id id, i32 tick) {
    gm_strike &gs = *(gm_strike*)transient_game_state;
    gs.on_bomb_plant_finished(id,tick);
}

internal void on_bomb_defuse_finished(entity_id id, i32 tick) {
    gm_strike &gs = *(gm_strike*)transient_game_state;
    gs.on_bomb_defuse_finished(id,tick);
}

internal void add_bullet(v2 pos, float rot, entity_id shooter_id) {
    gm_strike &gs = *(gm_strike*)transient_game_state;
    gs.add_bullet(pos,rot,shooter_id);
}



void gm_strike::update(netstate_info_t &c, double delta) {
    strike_map_t &mp = *(strike_map_t*)permanent_game_state;
    if (tick < round_start_tick) {
        return;
    } if (round_state == ROUND_BUYTIME) {
        if (tick >= buytime_end_tick) {
            round_state = ROUND_PLAYING;
        }
    }
    if (c.authoritative && round_state == ROUND_ENDTIME) {
        if (tick >= round_end_tick + ROUND_ENDTIME_LENGTH*60) {
            load_map(c.gms,strike_map_t::MAP_DE_DUST2);
        }
    }

    if (bomb_planted && tick == bomb_planted_tick+BOMB_TIME_TO_DETONATE*60) {
        // explode bomb
        bomb_planted=false;
        end_round();
        
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
                end_round();
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
            end_round();
        }
    }
}

void gm_strike::render(character *render_from_perspective_of, camera_t *game_camera) {
    // Render start
    glClearColor(1.f, 0.0f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    strike_map_t &mp = *(strike_map_t*)permanent_game_state;


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
    
    for (i32 id=0; id<player_count; id++) {
        character &p = players[id];
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

    for (i32 ind=0; ind<bullet_count; ind++) {
        iRect rect = {(int)bullets[ind].position.x+cam_mod.x,(int)bullets[ind].position.y+cam_mod.y,16,16};
        iRect center = {8,8};
        float rad_rot = convert_vec_to_angle(bullets[ind].vel)+(float)PI;

        GL_DrawTextureEx(gl_textures[BULLET_TEXTURE],rect,{0,0,0,0},false,false,rad_rot);
    }

    // if the bomb has been planted draw the bomb
    if (bomb_planted) {
        iRect dest = {(int)bomb_plant_location.x-12+cam_mod.x,(int)bomb_plant_location.y-12+cam_mod.y,24,24};
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
    if (round_state == ROUND_BUYTIME) {
        double time_until_start = (buytime_end_tick-tick) * (1.0/60.0);
        std::cout << time_until_start << std::endl;
        std::string timer_str=std::to_string((int)ceil(time_until_start));;
        local_persist generic_drawable round_start_timer={};
        round_start_timer = generate_text(m5x7,timer_str,{255,0,0,255},round_start_timer.gl_texture);
        round_start_timer.scale = {2,2};
        round_start_timer.position = {1280/2-round_start_timer.get_draw_irect().w/2,24};
        GL_DrawTexture(round_start_timer.gl_texture,round_start_timer.get_draw_irect());

    } else if (round_state == ROUND_PLAYING && bomb_planted) {
        double time_until_detonation = (bomb_planted_tick+(BOMB_TIME_TO_DETONATE*60) - tick) * (1.0/60.0);
        std::string timer_str=std::to_string((int)ceil(time_until_detonation));
        local_persist generic_drawable detonation_timer={};
        detonation_timer = generate_text(m5x7,timer_str,{255,0,0,255},detonation_timer.gl_texture);
        detonation_timer.scale = {2,2};
        detonation_timer.position = {1280/2-detonation_timer.get_draw_irect().w/2,24};
        GL_DrawTexture(detonation_timer.gl_texture,detonation_timer.get_draw_irect());
    }

}
