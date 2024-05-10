
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


// the reason this struct is 100% SEPARATE from game_state is because we don't want
// to worry about these details when writing generic update code for the game.
// we need an interface or something for interaction between the two
struct overall_game_manager {
    i32 connected_players=0;
    GMS state=GAME_HASNT_STARTED;
    
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


/*static void rewind_game_state(game_state &gst, netstate_info_t &c, double target_time) {
    for (i32 ind=(i32)c.snapshots.size()-1;ind>=0;ind--) {
        if (c.snapshots[ind].time<=target_time) {
            gst = c.snapshots[ind];
            gst.update(c,target_time);
            return;
        }
    }
    printf("ERROR: Cannot rewind game state!\n");
}
*/
/*
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
*/

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

/*
void load_gamestate_for_round(overall_game_manager &gms) {
    gms.state = GMS::GAME_PLAYING;
    SDL_Surface *level_surface = IMG_Load("res/map.png");

    gs.bullet_count=0;
    mp.wall_count=0;
    gs.one_remaining_tick=0;
    gs.bombsite_count=0;
    gs.bomb_planted_tick=0;

    i32 spawn_count=0;
    v2i spawns[8]={{0,0}};
    
    for (i32 x=0; x<32; x++) {
        for (i32 y=0; y<32; y++) {
            Color col = {0,0,0,0};
            Uint32 data = getpixel(level_surface, x,y);
            SDL_GetRGBA(data, level_surface->format, &col.r, &col.g, &col.b, &col.a);
            if (col == Color(0,0,0,255)) {
                add_wall({x,y});
            } else if (col == Color(255,0,0,255)) {
                spawns[spawn_count++] = {x,y};
            } else if (col == Color(0,0,255,255)) {
                gs.bombsite[gs.bombsite_count++] = {x,y};
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
*/

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

    /*
#ifdef GAME_CAN_END
    if (c.authoritative) {
        if (one_remaining_tick==0) {
            i32 living_player_count=0;
            character *last_alive=nullptr;
            FORn(players,gs.player_count,player) {
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
            }
        } else if (tick >= one_remaining_tick) {
            if (tick >= one_remaining_tick+90+180) {
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
                    gamestate_load_map(c.gms,MAP_DE_DUST2);
                    //load_gamestate_for_round(c.gms);
                }
            }
        }
    }
#endif
    */

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
    glUseProgram(sh_testProgram);
    
    // Render start
    SDL_SetRenderDrawColor(sdl_renderer,255,255,0,255);
    SDL_RenderClear(sdl_renderer);

    v2i cam_mod = {0,0};
    if (game_camera) cam_mod = v2i(1280/2,720/2) - v2i(game_camera->pos);
    SDL_Rect map_rect = {cam_mod.x,cam_mod.y,MAX_MAP_SIZE*64,MAX_MAP_SIZE*64};
    SDL_Rect cam_rect = {(i32)game_camera->pos.x - (1280/2),(i32)game_camera->pos.y - (720/2),1280,720};

    if (mp.static_texture_generated == false) {
        
        SDL_SetRenderTarget(sdl_renderer,textures[STATIC_MAP_TEXTURE]);
        SDL_SetRenderDrawColor(sdl_renderer,255,255,0,255);
        SDL_RenderClear(sdl_renderer);
    
        for (i32 x=0; x<MAX_MAP_SIZE; x++) {
            for (i32 y=0; y<MAX_MAP_SIZE; y++) {
                i32 type = mp.tiles[x][y];
                SDL_Rect dest = {x*64,y*64,64,64};
                SDL_Rect src={0,0,16,16};
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
                SDL_RenderCopy(sdl_renderer,textures[TILE_TEXTURE],&src,&dest);
            }
        }
        SDL_SetRenderTarget(sdl_renderer,NULL);
        mp.static_texture_generated=true;
    }
    SDL_RenderCopy(sdl_renderer,textures[STATIC_MAP_TEXTURE],NULL,&map_rect);
    
    // lol is this objectively horrible?? its a massive texture so its a great idea but poor execution
    // i mean who rly cares tho!

    /*
    for (i32 x=0; x<MAX_MAP_SIZE; x++) {
        for (i32 y=0; y<MAX_MAP_SIZE; y++) {
            i32 type = mp.tiles[x][y];
            SDL_Rect dest = {x*64+cam_mod.x,y*64+cam_mod.y,64,64};
            SDL_Rect src={0,0,16,16};
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
            SDL_RenderCopy(sdl_renderer,textures[TILE_TEXTURE],&src,&dest);
        }
    }
    */
    //SDL_SetRenderTarget(sdl_renderer,textures[WORLD_OBJECTS_TEXTURE]);
    //SDL_SetRenderDrawColor(sdl_renderer,0,0,0,0);
    //SDL_RenderClear(sdl_renderer);
    
    for (i32 id=0; id<gs.player_count; id++) {
        character &p = gs.players[id];
        if (p.visible == false) continue;

        SDL_Rect src_rect = {p.curr_state == character::PUNCHING ? 64 : p.curr_state == character::SHIELD ? 96 : 0,p.curr_state == character::TAKING_DAMAGE ? 32 : 0,32,32};
        SDL_Rect rect = {(int)p.pos.x+cam_mod.x,(int)p.pos.y+cam_mod.y,64,64};
        if (p.curr_state == character::MOVING) {
            src_rect.x = p.animation_timer > 0.2/2.0? 32 : 0;
        }
        if (p.damage_timer) {
            u8 g = (u8)lerp(p.color.g-75.f,(float)p.color.g,(0.5f-(float)p.damage_timer)*(1.f/0.5f));
            u8 b = (u8)lerp(p.color.b-105.f,(float)p.color.b,(0.5f-(float)p.damage_timer)*(1.f/0.5f));
            SDL_SetTextureColorMod(textures[PLAYER_TEXTURE],p.color.r,g,b);
        } else {
            SDL_SetTextureColorMod(textures[PLAYER_TEXTURE],p.color.r,p.color.g,p.color.b);
        }
        SDL_RenderCopyEx(sdl_renderer,textures[PLAYER_TEXTURE],&src_rect,&rect,NULL,NULL,p.flip?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);
    }

    for (i32 ind=0; ind<gs.bullet_count; ind++) {
        SDL_Rect rect = {(int)gs.bullets[ind].position.x+cam_mod.x,(int)gs.bullets[ind].position.y+cam_mod.y,16,16};
        SDL_Point center = {8,8};
        float rad_rot = convert_vec_to_angle(gs.bullets[ind].vel)+PI;
        SDL_RenderCopyEx(sdl_renderer,textures[BULLET_TEXTURE],NULL,&rect,rad_2_deg(rad_rot),&center,SDL_FLIP_NONE);
    }

    // if the bomb has been planted draw the bomb
    if (gs.bomb_planted) {
        SDL_Rect rect = {16,16,16,16};
        SDL_Rect dest = {(int)gs.bomb_plant_location.x-12+cam_mod.x,(int)gs.bomb_plant_location.y-12+cam_mod.y,24,24};
        SDL_RenderCopy(sdl_renderer,textures[TexType::ITEM_TEXTURE],&rect,&dest);
    }
    
    if (render_from_perspective_of != nullptr && client_sided_render_geometry.raycast_points.size()>0 && draw_shadows) {
        const SDL_Color black = {0,0,0,255};
        const SDL_Color white = {255,255,255,255};
        const SDL_Color invisible={0,0,0,0};
        const u8 shadow_visibility = 80;

        SDL_SetRenderTarget(sdl_renderer,textures[SHADOW_TEXTURE]);
        SDL_RenderClear(sdl_renderer);
        SDL_SetRenderDrawColor(sdl_renderer,80,80,80,255);
        SDL_RenderFillRect(sdl_renderer,NULL);
        
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

        SDL_SetRenderDrawBlendMode(sdl_renderer,SDL_BLENDMODE_NONE);
        for (i32 n=0;n<dests.size()+1;n++) {
            bool fin=n==dests.size();
            if (fin) n=0;
            v2 pt=dests[n].pt;
            v2 prev_point=n==0?dests.back().pt:dests[n-1].pt;
            SDL_Vertex vertex_1 = {{(float)p_pos.x+cam_mod.x,(float)p_pos.y+cam_mod.y}, white, {1, 1}};
            SDL_Vertex vertex_2 = {{(float)pt.x+cam_mod.x,(float)pt.y+cam_mod.y}, white, {1, 1}};
            SDL_Vertex vertex_3 = {{(float)prev_point.x+cam_mod.x,(float)prev_point.y+cam_mod.y}, white, {1, 1}};
            SDL_Vertex vertices[3] = {vertex_1,vertex_2,vertex_3};
            SDL_RenderGeometry(sdl_renderer, NULL, vertices, 3, NULL, 0);
            if (fin) break;
        }

        //SDL_SetRenderTarget(sdl_renderer,textures[WORLD_OBJECTS_TEXTURE]);
        //SDL_RenderCopy(sdl_renderer,textures[SHADOW_TEXTURE],NULL,NULL);

        SDL_SetRenderTarget(sdl_renderer,NULL);
        //SDL_RenderCopy(sdl_renderer,textures[STATIC_MAP_TEXTURE],NULL,&map_rect);
        //SDL_RenderCopy(sdl_renderer,textures[WORLD_OBJECTS_TEXTURE],NULL,NULL);
        //SDL_RenderCopy(sdl_renderer,textures[SHADOW_TEXTURE],NULL,NULL);
        //SDL_RenderCopy(sdl_renderer,textures[WORLD_OBJECTS_TEXTURE],NULL,NULL);
        //SDL_RenderCopy(sdl_renderer,textures[WORLD_OBJECTS_TEXTURE],NULL,NULL);

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
    } else {
        SDL_SetRenderTarget(sdl_renderer,NULL);
        SDL_RenderCopy(sdl_renderer,textures[WORLD_OBJECTS_TEXTURE],NULL,NULL);
    }
    glUseProgram(NULL);

    // gui elements
    if (gs.round_state == ROUND_BUYTIME) {
        double time_until_start = (gs.buytime_end_tick-gs.tick) * (1.0/60.0);

        std::string timer_str=std::to_string((int)ceil(time_until_start));;
        generic_drawable round_start_timer;
        v2 scale = {4,4};
        round_start_timer = generate_text(m5x7,timer_str,{255,0,0,255});
        round_start_timer.scale = scale;
        round_start_timer.position = {1280/2-round_start_timer.get_draw_rect().w/2,24};
        SDL_Rect st_timer_rect = round_start_timer.get_draw_rect();
        SDL_RenderCopy(sdl_renderer,round_start_timer.texture,NULL,&st_timer_rect);
    } else if (gs.round_state == ROUND_PLAYING && gs.bomb_planted) {
        double time_until_detonation = (gs.bomb_planted_tick+(BOMB_TIME_TO_DETONATE*60) - gs.tick) * (1.0/60.0);
        std::string timer_str=std::to_string((int)ceil(time_until_detonation));
        generic_drawable detonation_timer;
        v2 scale = {4,4};
        detonation_timer = generate_text(m5x7,timer_str,{255,0,0,255});
        detonation_timer.scale = scale;
        detonation_timer.position = {1280/2-detonation_timer.get_draw_rect().w/2,24};
        SDL_Rect dt_timer_rect = detonation_timer.get_draw_rect();
        SDL_RenderCopy(sdl_renderer,detonation_timer.texture,NULL,&dt_timer_rect);
    }
    
    /*
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
    */

}

void render_pregame_screen(overall_game_manager &gms, double time_to_start) {
    static int connected_last_tick=-1;
    static generic_drawable connection_text = generate_text(m5x7,"Players connected (" + std::to_string(connected_last_tick) + "/2)",{255,255,200});
    connection_text.scale = {2,2};
    
    if (connected_last_tick != gms.connected_players) {
        connection_text = generate_text(m5x7,"Players connected (" + std::to_string(gms.connected_players)+"/2)",{255,255,200});
        connected_last_tick=gms.connected_players;
        connection_text.scale = {2,2};
        connection_text.position = {1280/2-connection_text.get_draw_rect().w/2,380};
    }
    
    SDL_SetRenderDrawColor(sdl_renderer,255,255,0,255);
    SDL_RenderClear(sdl_renderer);
    SDL_Rect dest = {0,0,1280,720};
    SDL_RenderCopy(sdl_renderer,textures[PREGAME_TEXTURE],NULL,&dest);
    SDL_Rect connection_rect = connection_text.get_draw_rect();
    SDL_RenderCopy(sdl_renderer,connection_text.texture,NULL,&connection_rect);

    if (gms.counting_down_to_game_start) {
        std::string str = std::to_string(time_to_start);
        if (str.size() > 4) {
            str.erase(str.begin()+4,str.end());
        }
        
        generic_drawable countdown_clock = generate_text(m5x7,str,{255,240,40});
        countdown_clock.scale = {4,4};
        countdown_clock.position = {1280/2-countdown_clock.get_draw_rect().w/2,480};
        SDL_Rect countdown_rect = countdown_clock.get_draw_rect();
        SDL_RenderCopy(sdl_renderer,countdown_clock.texture,NULL,&countdown_rect);
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

