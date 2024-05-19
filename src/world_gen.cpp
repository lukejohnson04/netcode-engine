
#define WORLD_SIZE 4
#define CHUNK_SIZE 16

enum TILE_TYPE : u8 {
    TT_NONE,
    TT_GROUND,
    TT_WALL,
    TT_BOMBSITE,
    TT_A,
    TT_AA,
    TT_ARROW_UPLEFT,
    TT_ARROW_UPRIGHT,
    
    TT_DIRT,
    TT_GRASS,
    TT_WATER,
    TT_STONE,
    TT_SAND,
    TT_COUNT
};

enum WORLD_OBJECT_TYPE : u8 {
    WO_NONE,
    WO_TREE,
    WO_COUNT
};


inline internal
TILE_TYPE determine_tile(i32 x, i32 y, perlin &p_noise, double *white_noise, double *height_noise=nullptr) {
    double noise_val = MAX(MIN(p_noise.noise(x,y),1.0),-1.0);
    double altitude = (noise_val+1.0)/2.0;
    if (height_noise) {
        altitude *= height_noise[x*p_noise.chunk_size*p_noise.map_size + y];
    }
    double tree_spawn_chance = pow(altitude*0.9,2)+0.1;
    iRect src={0,0,16,16};

    TILE_TYPE tt=TT_NONE;
    
    if (altitude < 0.2) {
        tt = TILE_TYPE::TT_WATER;
    } else if (altitude < 0.28) {
        tt = TILE_TYPE::TT_SAND;
    } else if (altitude < 0.32) {
        tt = TILE_TYPE::TT_DIRT;
    } else if (altitude < 0.55) {
        tt = TILE_TYPE::TT_GRASS;
    } else {
        tt = TILE_TYPE::TT_DIRT;
    }
    return tt;
}

inline internal
WORLD_OBJECT_TYPE determine_world_object(i32 x, i32 y, perlin &p_noise, double *white_noise, double *height_noise=nullptr) {
    double noise_val = MAX(MIN(p_noise.noise(x,y),1.0),-1.0);
    double altitude = (noise_val+1.0)/2.0;
    if (height_noise) {
        altitude *= height_noise[x*p_noise.chunk_size*p_noise.map_size + y];
    }
    double tree_spawn_chance = pow(altitude*0.9,2)+0.1;
    iRect src={0,0,16,16};

    WORLD_OBJECT_TYPE wo_tt=WO_NONE;
    
    if (altitude >= 0.32) {
        i32 ind = (x*p_noise.map_size*p_noise.chunk_size) + y;
        double tree_roll = white_noise[ind]/1.25;
        if (tree_roll > 1.0-tree_spawn_chance) {
            wo_tt = WORLD_OBJECT_TYPE::WO_TREE;
        }
    }
    return wo_tt;
}



/*
internal
void generate_map(TILE_TYPE **tiles, int nx, int ny, u32 seed) {
    perlin p_noise = create_perlin(WORLD_SIZE,CHUNK_SIZE,3,0.65);
    constexpr i32 tiles_in_world = WORLD_SIZE*CHUNK_SIZE*WORLD_SIZE*CHUNK_SIZE;

    double white_noise[tiles_in_world];
    generate_white_noise(white_noise,tiles_in_world,Random::seed);
    double height_noise[tiles_in_world];
    generate_height_noisemap(height_noise,white_noise,WORLD_SIZE*CHUNK_SIZE,Random::seed);

    for (i32 x=0;x<WORLD_SIZE*CHUNK_SIZE;x++) {
        for (i32 y=0;y<WORLD_SIZE*CHUNK_SIZE;y++) {
            TILE_TYPE tt = determine_tile(x,y,p_noise,white_noise,height_noise);
            tiles[x][y] = tt;
            if (tt == TILE_TYPE::TT_TREE) {
                _mp.add_wall({x,y});
            }
        }
    }
}
*/
