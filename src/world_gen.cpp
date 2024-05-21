
#define WORLD_SIZE 8
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
    WO_STONE,
    WO_WOODEN_FENCE,
    WO_COUNT
};

struct world_generation_props {
    u32 seed_terrain;
    u32 seed_height_noise;
    u32 seed_tree_noise;
    u32 seed_stone_noise;
};

world_generation_props generate_generation_props(u32 seed) {
    world_generation_props props;
    props.seed_terrain = seed;
    props.seed_height_noise = seed+1;
    props.seed_tree_noise = seed+2;
    props.seed_stone_noise = seed+3;

    return props;
}


inline internal
TILE_TYPE determine_tile(i32 x, i32 y, perlin &p_noise, double *white_noise, double *stone_noise, double *height_noise=nullptr) {
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
    } else if (altitude < 0.25) {
        tt = TILE_TYPE::TT_SAND;
    } else {
        double stone_chance = (altitude + 0.35) * stone_noise[x*p_noise.chunk_size*p_noise.map_size+y];
        if (stone_chance >= 0.5) {
            tt = TILE_TYPE::TT_STONE;
            return tt;
        }
        if (altitude < 0.32) {
            tt = TILE_TYPE::TT_DIRT;
        } else if (altitude < 0.6) {
            tt = TILE_TYPE::TT_GRASS;
        } else {
            tt = TILE_TYPE::TT_DIRT;
        }
    }
    return tt;
}

inline internal
WORLD_OBJECT_TYPE determine_world_object(i32 x, i32 y, perlin &p_noise, double *tree_noise, double *stone_noise, double *height_noise) {
    double noise_val = MAX(MIN(p_noise.noise(x,y),1.0),-1.0);
    double altitude = (noise_val+1.0)/2.0;
    if (height_noise) {
        altitude *= height_noise[x*p_noise.chunk_size*p_noise.map_size + y];
    }
    double tree_spawn_chance = pow(altitude,2)+0.1;
    iRect src={0,0,16,16};

    WORLD_OBJECT_TYPE wo_tt=WO_NONE;

    if (altitude >= 0.225) {
        double stone_spawn_chance = stone_noise[x*p_noise.chunk_size*p_noise.map_size+y];
        stone_spawn_chance *= (altitude+0.45);
        if (stone_spawn_chance >= 0.4) {
            wo_tt = WORLD_OBJECT_TYPE::WO_STONE;
            return wo_tt;
        }
    }
    
    if (altitude >= 0.32) {
        i32 ind = (x*p_noise.map_size*p_noise.chunk_size) + y;
        double tree_roll = tree_noise[ind]/1.25;
        if (tree_roll > 1.0-tree_spawn_chance) {
            wo_tt = WORLD_OBJECT_TYPE::WO_TREE;
        }
    }
    return wo_tt;
}
