
struct netstate_info_t;

enum ROUND_STATE {
    ROUND_WARMUP,
    ROUND_BUYTIME,
    ROUND_PLAYING,
    ROUND_ENDTIME,
    ROUND_STATE_COUNT
};


struct world_object_t {
    v2 pos;
    WORLD_OBJECT_TYPE type;

    union {
        i32 took_damage_tick;
    };
    i16 health=100;
};


struct world_chunk_t {
    TILE_TYPE tiles[CHUNK_SIZE][CHUNK_SIZE] = {{TT_NONE}};
    static constexpr i32 world_object_limit = (CHUNK_SIZE*CHUNK_SIZE)/2;

    i32 world_object_count=0;
    world_object_t world_objects[world_object_limit];

    inline
    void add_world_object(world_object_t n_object) {
        assert(world_object_count<world_object_limit);
        world_objects[world_object_count++] = n_object;
    }
};


struct generic_map_t {
    i32 wall_count=0;
    v2i walls[WORLD_SIZE*CHUNK_SIZE*WORLD_SIZE*CHUNK_SIZE];

    world_chunk_t chunks[WORLD_SIZE][WORLD_SIZE]={};
    //TILE_TYPE tiles[WORLD_SIZE*CHUNK_SIZE][WORLD_SIZE*CHUNK_SIZE] = {{TT_NONE}};

    void add_wall(v2i pos) {
        walls[wall_count++] = pos;
    }
};


// should be called world state instead of game state
struct game_state {
    // this is map data, not game state data!
    // and can be removed completely    
    i32 player_count=0;
    character players[8];

    static constexpr i32 max_world_items_count=128;
    i32 world_item_count=0;
    world_item_t world_items[max_world_items_count];

    // should abstract these net variables out eventually
    int tick=0;
    i32 round_start_tick=0;

    // let the player get into negative money
    // and give them a prompt that if they don't make their money back
    // the feds will come after them
    void update(netstate_info_t &c, double delta);
};

// snapshots are for sending just the local data
struct snapshot_t {
    char data[sizeof(game_state)];
    //game_state gms;
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
    GM_STRIKE,
    GM_JOSHFARE,
    GM_JOSHUAGAME,
    GM_COUNT,
};

// the reason this struct is 100% SEPARATE from game_state is because we don't want
// to worry about these details when writing generic update code for the game.
// we need an interface or something for interaction between the two
struct overall_game_manager {
    i32 connected_players=0;
    GMS state=GAME_HASNT_STARTED;
    GAME_MODE gmode= GM_STRIKE;

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
    std::vector<command_node> command_stack;    
    std::vector<snapshot_t> snapshots;
    
    bool authoritative=false;
    int interp_delay=0;

    overall_game_manager gms;

    bool do_charas_interp[16]={true};
};


global_variable void* transient_game_state;
global_variable void* permanent_game_state;

global_variable game_state gs;
global_variable generic_map_t _mp;

void load_new_game() {
    timer_t perfCounter;
    perfCounter.Start();
    
    gs = {};
    _mp = {};

    perlin p_noise = create_perlin(WORLD_SIZE,CHUNK_SIZE,3,0.65);
    constexpr i32 tiles_in_world = WORLD_SIZE*CHUNK_SIZE*WORLD_SIZE*CHUNK_SIZE;

    double white_noise[tiles_in_world];
    generate_white_noise(white_noise,tiles_in_world,Random::seed);
    double height_noise[tiles_in_world];
    generate_height_noisemap(height_noise,white_noise,WORLD_SIZE*CHUNK_SIZE,Random::seed);
    
    for (i32 cy=0;cy<WORLD_SIZE;cy++) {
        for (i32 cx=0;cx<WORLD_SIZE;cx++) {
            world_chunk_t chunk = {};

            for (i32 y=0;y<CHUNK_SIZE;y++) {
                for (i32 x=0;x<CHUNK_SIZE;x++) {
                    i32 global_x = cx*CHUNK_SIZE+x;
                    i32 global_y = cy*CHUNK_SIZE+y;
                    
                    TILE_TYPE tt = determine_tile(global_x,global_y,p_noise,white_noise,height_noise);
                    chunk.tiles[x][y] = tt;
                    WORLD_OBJECT_TYPE wo_tt = determine_world_object(global_x,global_y,p_noise,white_noise,height_noise);
                    if (wo_tt != WO_NONE) {
                        world_object_t n_obj = {};
                        n_obj.type = wo_tt;
                        n_obj.took_damage_tick=0;
                        n_obj.pos = {(float)global_x*64.f,(float)global_y*64.f};
                        chunk.add_world_object(n_obj);
                    }
                }
            }
            _mp.chunks[cx][cy] = chunk;
        }
    }

    character &player = gs.players[gs.player_count];
    player = create_player({200,200},(entity_id)(gs.player_count));
    player.color = {(u8)(rand() % 255), (u8)(rand() % 255), (u8)(rand() % 255), 255};
    gs.player_count++;

    double elapsed = perfCounter.get();
    std::cout << "World generation took " << elapsed << " seconds" << std::endl;
}


#define GAME_CAN_END
void game_state::update(netstate_info_t &c, double delta) {
    if (tick < round_start_tick) {
        return;
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
            process_command(&players[node.cmd.sending_id],node.cmd);
        }
    }

    // interp delay in ticks
    i32 punching_players_count=0;
    character *punching_players[8]={nullptr};
    FOR(players,player_count) {
        if (obj->curr_state == character::DEAD) continue;
        bool planting_before_update = obj->curr_state == character::PLANTING;
        update_player(obj,delta,_mp.wall_count,_mp.walls,player_count,players,tick);
        if (obj->curr_state == character::PUNCHING && obj->has_hit_something_yet==false) {
            punching_players[punching_players_count++] = obj;
        }
    }

    for (i32 cx=0;cx<WORLD_SIZE;cx++) {
        for (i32 cy=0;cy<WORLD_SIZE;cy++) {
            world_chunk_t *chunk = &_mp.chunks[cx][cy];
            for (i32 obj_ind=0;obj_ind<chunk->world_object_count;obj_ind++) {
                world_object_t *wo_tt = &chunk->world_objects[obj_ind];
                if (wo_tt->type != WO_TREE) continue;
                if (tick - wo_tt->took_damage_tick < 6) continue;
                iRect tree_rect = {(i32)wo_tt->pos.x,(i32)wo_tt->pos.y,64,64};
                for (i32 p_ind=0;p_ind<punching_players_count;p_ind++) {
                    character *player = punching_players[p_ind];
                    iRect p_rect = {(i32)player->pos.x,(i32)player->pos.y,64,64};

                    if (rects_collide(p_rect,tree_rect)) {
                        player->has_hit_something_yet=true;
                        wo_tt->took_damage_tick = tick;
                        wo_tt->health -= 15;
                        // tree destroyed
                        if (wo_tt->health <= 0) {
                            world_items[world_item_count++] = create_world_item(ITEM_TYPE::IT_WOOD,wo_tt->pos,player->flip?-1.0f:1.0f);
                            chunk->world_objects[obj_ind] = chunk->world_objects[--chunk->world_object_count];
                            obj_ind--;
                        }
                        punching_players[p_ind] = punching_players[--punching_players_count];
                        break;
                    }
                }
            }
        }
    }
    for (i32 item_ind=0;item_ind<world_item_count;item_ind++) {
        world_item_t *item = &world_items[item_ind];
        iRect item_pickup_rect = {(i32)item->pos.x,(i32)item->pos.y,32,32};
        bool picked_up=false;
        for (i32 p_ind=0;p_ind<player_count;p_ind++) {
            character *player=&players[p_ind];
            iRect p_pickup_rect = {(i32)player->pos.x+8,(i32)player->pos.y+24,64-(8*2),64-(24)};
            
            if (rects_collide(p_pickup_rect,item_pickup_rect)) {
                if (add_to_inventory(player->inventory,character::INVENTORY_SIZE,{item->type,item->count})) {
                    world_items[item_ind--] = world_items[--world_item_count];
                    picked_up=true;
                    break;
                }
            }
        } if (picked_up) continue;
        
        if (item->vel == v3::Zero) continue;
        item->pos += {item->vel.x,item->vel.y};
        item->z_pos += item->vel.z;

        item->vel.x *= 0.85f;
        item->vel.y *= 0.85f;
        item->vel.z += 120.f*(1.0f/60.0f);
        
        if (item->z_pos >= 0) {
            item->z_pos = 0;
            item->vel = {0,0,0};
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

bool draw_shadows=false;

void render_game_state(character *render_from_perspective_of=nullptr, camera_t *game_camera=nullptr) {
    // Render start
    glClearColor(1.f, 0.0f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    v2i cam_mod = {0,0};
    if (game_camera) cam_mod = v2i(1280/2,720/2) - v2i(game_camera->pos);
    iRect map_rect = {cam_mod.x,cam_mod.y,WORLD_SIZE*CHUNK_SIZE*64,WORLD_SIZE*CHUNK_SIZE*64};
    iRect cam_rect = {(i32)game_camera->pos.x - (1280/2),(i32)game_camera->pos.y - (720/2),1280,720};

    glUseProgram(sh_textureProgram);
    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_GAME_WORLD]);
    glClearColor(1.f, 1.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_MINIMAP]);
    glClearColor(1.f, 1.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_GAME_WORLD]);
    for (i32 cy=0; cy<WORLD_SIZE; cy++) {
        for (i32 cx=0; cx<WORLD_SIZE; cx++) {
            // see if chunk is onscreen
            const i32 chunk_size_pixels = CHUNK_SIZE*64;
            world_chunk_t chunk = _mp.chunks[cx][cy];
            iRect chunk_rect = {chunk_size_pixels*cx,chunk_size_pixels*cy,chunk_size_pixels,chunk_size_pixels};
            if (rects_collide(chunk_rect,cam_rect) == false) continue;

            for (i32 y=0;y<CHUNK_SIZE;y++) {
                for (i32 x=0;x<CHUNK_SIZE;x++) {
                    TILE_TYPE type = chunk.tiles[x][y];
                    iRect dest = {x*64+chunk_rect.x,y*64+chunk_rect.y,64,64};
                    if (rects_collide(dest,cam_rect)==false) continue;
            
                    iRect src={0,0,16,16};
                    if (type == TILE_TYPE::TT_DIRT) {
                        src = {16,32,16,16};
                    } else if (type == TILE_TYPE::TT_GRASS) {
                        src = {0,16,16,16};
                    } else if (type == TILE_TYPE::TT_WATER) {
                        src = {32,0,16,16};
                    } else if (type == TILE_TYPE::TT_STONE) {
                        src = {0,32,16,16};
                    } else if (type == TILE_TYPE::TT_SAND) {
                        src = {16,16,16,16};
                    }
            
                    dest.x += cam_mod.x;
                    dest.y += cam_mod.y;
                
                    GL_DrawTexture(gl_textures[TILE_TEXTURE],dest,src);
                }
            }
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_GAME_WORLD]);

    // this drawing is broken because it only draws inorder within chunks
    // e.g. it goes left to right, top to bottom. But then next it draws
    // the neighbor chunk, which starts back at the top right
    for (i32 cy=0; cy<WORLD_SIZE; cy++) {
        for (i32 cx=0; cx<WORLD_SIZE; cx++) {
            // see if chunk is onscreen
            const i32 chunk_size_pixels = CHUNK_SIZE*64;
            world_chunk_t chunk = _mp.chunks[cx][cy];
            iRect chunk_rect = {chunk_size_pixels*cx,chunk_size_pixels*cy,chunk_size_pixels,chunk_size_pixels};

            // need to make the chunk size a little bigger because trees are massive and take up more
            // screenspace than just the tiles of the chunk themselves
            if (rects_collide(iRect({chunk_rect.x-192,chunk_rect.y-192,chunk_rect.w+384,chunk_rect.h+384}),cam_rect) == false) continue;

            for (i32 obj_ind=0;obj_ind<chunk.world_object_count;obj_ind++) {
                world_object_t *tree = &chunk.world_objects[obj_ind];
                if (tree->type != WORLD_OBJECT_TYPE::WO_TREE) {
                    continue;
                }
                bool mod=false;
                if (gs.tick - tree->took_damage_tick < 12) {
                    mod = true;
                    glUniform4f(glGetUniformLocation(sh_textureProgram,"colorMod"),1.0f,0.5f,0.75f,1.0f);
                }
                iRect tree_dest = {(i32)tree->pos.x-64,(i32)tree->pos.y-(64*3),3*64,4*64};
                tree_dest.x += cam_mod.x;
                tree_dest.y += cam_mod.y;
                GL_DrawTexture(gl_textures[TILE_TEXTURE],tree_dest,{0,64,48,64});
                if (mod) {
                    glUniform4f(glGetUniformLocation(sh_textureProgram,"colorMod"),1.0f,1.0f,1.0f,1.0f);
                }
            }
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
            std::cout << "damage timer\n";
        } else {
            glUniform4f(tintLoc_col, p.color.r/255.f,p.color.g/255.f,p.color.b/255.f,1.0f);
        }
        GL_DrawTextureEx(gl_textures[PLAYER_TEXTURE],dest_rect,src_rect,p.flip);
    }

    for (i32 item_ind=0; item_ind<gs.world_item_count; item_ind++) {
        world_item_t *item = &gs.world_items[item_ind];
        iRect rect = {(i32)item->pos.x,(i32)item->pos.y,32,32};
        if (rects_collide(rect,cam_rect)==false) continue;
        rect.x+=cam_mod.x;
        rect.y+=cam_mod.y;
        rect.y += (i32)item->z_pos;
        GL_DrawTexture(gl_textures[ITEM_TEXTURE],rect,{32,0,16,16});
    }
    glUniform4f(tintLoc_col, 1.0f,1.0f,1.0f,1.0f);
    
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

    // minimap
    glUseProgram(sh_textureProgram);
    iRect minimap_dest = {1280-8-128,8,128,128};
    // border
    GL_DrawTexture(gl_textures[MINIMAP_TEXTURE],{minimap_dest.x-8,minimap_dest.y-8,minimap_dest.w+16,minimap_dest.h+16});
    //GL_DrawTextureEx(gl_textures[TX_MINIMAP],minimap_dest,{0,0,0,0},false,true);
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
internal void find_and_load_gamestate_snapshot(game_state &gst, netstate_info_t &c, int start_tick) {
    for (i32 ind=((i32)c.snapshots.size())-1; ind>=0;ind--) {
        game_state &gms = *(game_state*)c.snapshots[ind].data;
        if (gms.tick<=start_tick) {
            gst = gms;
            return;
        }
    }
    printf("ERROR: Couldn't find snapshot to rewind to %d!\n",start_tick);
}

internal void load_game_state_up_to_tick(void* temp_game_state_data, netstate_info_t &c, int target_tick, bool overwrite_snapshots=true) {
    game_state &gst = *(game_state*)temp_game_state_data;
    while(gst.tick<target_tick) {
        // should this be before or after???
        // after i think because this function shouldn't have to worry about previous ticks
        // crazy inefficient lol
        // TODO: fix this
        if (overwrite_snapshots) {
            for (snapshot_t &snap: c.snapshots) {
                game_state &gms = *(game_state*)snap.data;
                if (gms.tick==gst.tick) {
                    gms = gst;
                    break;
                }
            }
        }
        gst.update(c,1.0/60.0);
        gst.tick++;
    }
}
