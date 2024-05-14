
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


struct gm_strike {

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

    void gamestate_load_map(overall_game_manager &gms, strike_map_t &mp, i32 map);
    void on_bomb_plant_finished(entity_id id, i32 fin_tick);
    void on_bomb_defuse_finished(entity_id id, i32 fin_tick);
    void game_state_end_round();

    void update(netstate_info_t &c, double delta);
};

void gm_strike::add_bullet(v2 pos, float rot, entity_id shooter_id) {
    bullet_t &bullet = bullets[bullet_count++];
    bullet.position = pos;
    bullet.vel = convert_angle_to_vec(rot)*600.f;
    bullet.shooter_id = shooter_id;
}


// TODO: replace _mp with mp
void gm_strike::gamestate_load_map(overall_game_manager &gms, strike_map_t &_mp, i32 map) {
    gms.state = GAME_PLAYING;

    bullet_count=0;
    one_remaining_tick=0;
    bomb_planted_tick=0;
    bomb_defused_tick=0;
    bomb_planted=false;
    bomb_defused=false;
    round_state = ROUND_BUYTIME;

    load_permanent_data_from_map(map);
    
    bool create_all_charas = player_count == gms.connected_players ? false : true;
    player_count=0;

    i32 side_count[TEAM_COUNT]={0};
    i32 unused_count[TEAM_COUNT]={_mp.spawn_counts[TERRORIST],_mp.spawn_counts[COUNTER_TERRORIST]};
    v2i unused_spawns[TEAM_COUNT][32];
    memcpy(unused_spawns[TERRORIST],_mp.spawns[TERRORIST],32*sizeof(v2i));
    memcpy(unused_spawns[COUNTER_TERRORIST],_mp.spawns[COUNTER_TERRORIST],32*sizeof(v2i));

    for (i32 ind=0; ind<gms.connected_players; ind++) {
        money[ind] = FIRST_ROUND_START_MONEY;
        if (create_all_charas == false) {
            character old_chara = players[ind];
            
            add_player(players,&player_count);
            players[ind] = create_player({0,0},old_chara.id);
            players[ind].color = old_chara.color;
        } else {
            add_player(players,&player_count);
        
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

void gm_strike::game_state_end_round() {
    round_state = ROUND_ENDTIME;
    round_end_tick = tick;
}
