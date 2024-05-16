
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

struct game_state {
    int tick=0;
    i32 player_count=0;
    character players[8]; // characters are way too big lol
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

struct snapshot_t;

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


bool draw_shadows=true;

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
