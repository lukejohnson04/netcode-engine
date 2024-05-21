
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
    WO_BEACON,
    WO_COUNT
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

    void add_wall(v2i pos) {
        walls[wall_count++] = pos;
    }
};


iRect get_wobject_draw_rect(WORLD_OBJECT_TYPE type) {
    if (type == WO_TREE) {
        return {0,64,48,64};
    } else if (type == WO_STONE) {
        return {48,48,32,32};
    } else if (type == WO_WOODEN_FENCE) {
        return {48,96,32,32};
    } else if (type == WO_BEACON) {
        return {80,64,32,64};
    }
    return {0,0,0,0};
}


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
            return wo_tt;
        }
    }
    
    return wo_tt;
}

internal
void generate_world(generic_map_t *mp) {
    timer_t perfCounter;
    perfCounter.Start();
    
    *mp = {};

    world_generation_props gen_props = generate_generation_props(Random::seed);
    perlin p_noise = create_perlin(WORLD_SIZE,CHUNK_SIZE,3,0.65);
    constexpr i32 tiles_in_world = WORLD_SIZE*CHUNK_SIZE*WORLD_SIZE*CHUNK_SIZE;

    double tree_noise[tiles_in_world];
    generate_white_noise(tree_noise,tiles_in_world,gen_props.seed_tree_noise);
    double height_noise[tiles_in_world];
    generate_height_noisemap(height_noise,WORLD_SIZE*CHUNK_SIZE,gen_props.seed_height_noise);
    double stone_noise[WORLD_SIZE*CHUNK_SIZE][WORLD_SIZE*CHUNK_SIZE];
    {
        perlin p_stone = create_perlin(WORLD_SIZE,CHUNK_SIZE,4,0.65);
        for (i32 x=0;x<WORLD_SIZE*CHUNK_SIZE;x++) {
            for (i32 y=0;y<WORLD_SIZE*CHUNK_SIZE;y++) {
                stone_noise[x][y] = p_stone.noise(x,y);
            }
        }
    }
    i32 stone=0;

    for (i32 cy=0;cy<WORLD_SIZE;cy++) {
        for (i32 cx=0;cx<WORLD_SIZE;cx++) {
            world_chunk_t chunk = {};

            for (i32 y=0;y<CHUNK_SIZE;y++) {
                for (i32 x=0;x<CHUNK_SIZE;x++) {
                    i32 global_x = cx*CHUNK_SIZE+x;
                    i32 global_y = cy*CHUNK_SIZE+y;
                    
                    TILE_TYPE tt = determine_tile(global_x,global_y,p_noise,tree_noise,stone_noise[0],height_noise);
                    chunk.tiles[x][y] = tt;
                    WORLD_OBJECT_TYPE wo_tt = determine_world_object(global_x,global_y,p_noise,tree_noise,stone_noise[0],height_noise);
                    if (wo_tt != WO_NONE) {
                        if (wo_tt == WORLD_OBJECT_TYPE::WO_STONE) stone++;
                        world_object_t n_obj = {};
                        n_obj.type = wo_tt;
                        n_obj.took_damage_tick=0;
                        n_obj.pos = {(float)global_x*64.f,(float)global_y*64.f};
                        mp->add_wall({global_x,global_y});
                        chunk.add_world_object(n_obj);
                    }
                }
            }
            mp->chunks[cx][cy] = chunk;
        }
    }
    std::cout << "Generated " << stone << " stone\n";

    double elapsed = perfCounter.get();
    std::cout << "World generation took " << elapsed << " seconds" << std::endl;
    
}
