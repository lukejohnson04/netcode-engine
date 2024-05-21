
struct perlin {
    // influence vectors at the corners of each chunk
    std::vector<std::vector<double>> influence;
    i32 octaves=1;
    i32 map_size;
    i32 chunk_size;
    double persistance=0.5;
    double noise(i32 x, i32 y);

    SDL_Surface *generate_surface();
    void generate_texture(GLuint *texture);
};


void generate_white_noise(double *arr, int n, u32 seed) {
    std::mt19937 rand(seed);
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    for (i32 ind=0;ind<n;ind++) {
        arr[ind] = distribution(rand);
    }
}

void generate_height_noisemap(double *arr, int map_size, u32 seed) {
    std::mt19937 rand(seed);
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    
    for (i32 x=0;x<map_size;x++) {
        for (i32 y=0;y<map_size;y++) {
            v2d normalized_coords = {(double)x/(double)map_size,(double)y/(double)map_size};
            double dist = ddistance_between_points(normalized_coords,{0.5,0.5}) * 2.0;
            double val = -(pow(abs(0.85*dist),3)) + 1;
            double white_val = distribution(rand);
            val += (white_val-0.5) / 8.0;
            
            val = MAX(MIN(val,1.0),0.0);

            arr[x*map_size+y] = val;
        }
    }
}

SDL_Surface *generate_surface_from_height_noisemap(double *arr, int map_size) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,map_size,map_size,32,SDL_PIXELFORMAT_ARGB8888);
    for (i32 x=0; x<map_size; x++) {
        for (i32 y=0; y<map_size; y++) {
            double noise_val = arr[x*map_size+y];
            u8 val = (u8)(noise_val * 255.0);
            Color col = {val,val,val,255};
            setpixel(surface,x,y,col);
        }
    }
    return surface;
}


perlin create_perlin(i32 n_map_size, i32 n_chunk_size, i32 n_octaves, double n_persistance) {
    perlin p;
    p.octaves = n_octaves;
    p.map_size = n_map_size;
    p.chunk_size = n_chunk_size;
    p.persistance = n_persistance;
    //p.influence.resize((p.map_size+1)*(p.map_size+1)*p.octaves);
    for (i32 octave=0;octave<p.octaves;octave++) {
        std::vector<double> gradients;
        for (i32 x=0;x<(p.map_size*(octave+1))+1;x++) {
            for (i32 y=0;y<(p.map_size*(octave+1))+1;y++) {
                gradients.push_back(Random::Double(0,PI*2));
            }
        }
        p.influence.push_back(gradients);
    }
    return p;
}

double perlin::noise(i32 x, i32 y) {
    double total=0.0;
    double frequency=1.0;
    double amplitude=1.0;
    double maxAmplitude=0.0;

    for (i32 octave=0; octave<octaves; octave++) {
        double period = chunk_size / frequency;
        // get cx and cy, the current chunk coordinates
        i32 chunk_x = (i32)floor(x / period);
        i32 chunk_y = (i32)floor(y / period);
        // normalized coordinates within the chunk, from 0 - 1
        double p_in_chunk_x = (x - (chunk_x*period)) / period;
        double p_in_chunk_y = (y - (chunk_y*period)) / period;

        // the index of the beginning of the current octave in the vector of influence vectors
        i32 t1_ind = (chunk_y*(map_size+1)) + chunk_x;
        i32 t2_ind = t1_ind+1;
        i32 t3_ind = t1_ind + (map_size+1);
        i32 t4_ind = t3_ind+1;

        v2d t1_norm = {cos(influence[octave][t1_ind]),sin(influence[octave][t1_ind])};
        v2d t2_norm = {cos(influence[octave][t2_ind]),sin(influence[octave][t2_ind])};
        v2d t3_norm = {cos(influence[octave][t3_ind]),sin(influence[octave][t3_ind])};
        v2d t4_norm = {cos(influence[octave][t4_ind]),sin(influence[octave][t4_ind])};
        
        v2d t1_offset = {p_in_chunk_x,p_in_chunk_y};
        v2d t2_offset = {p_in_chunk_x-1,p_in_chunk_y};
        v2d t3_offset = {p_in_chunk_x,p_in_chunk_y-1};
        v2d t4_offset = {p_in_chunk_x-1,p_in_chunk_y-1};
        
        double scalar1 = (t1_offset.x * t1_norm.x) + (t1_offset.y * t1_norm.y);
        double scalar2 = (t2_offset.x * t2_norm.x) + (t2_offset.y * t2_norm.y);
        double scalar3 = (t3_offset.x * t3_norm.x) + (t3_offset.y * t3_norm.y);
        double scalar4 = (t4_offset.x * t4_norm.x) + (t4_offset.y * t4_norm.y);

        double u = perlin_fade_d(p_in_chunk_x);
        double v = perlin_fade_d(p_in_chunk_y);

        // lerp horizontally, then vertically
        // then lerp the two results
        double smooth1 = lerp_d(scalar1, scalar2, u);
        double smooth2 = lerp_d(scalar3, scalar4, u);
        double final_val = lerp_d(smooth1, smooth2, v);
        total += final_val * amplitude;

        maxAmplitude += amplitude;
        amplitude *= persistance;
        frequency *= 2.0;
    }
    
    // clamp
    return total;
}

SDL_Surface *perlin::generate_surface() {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,chunk_size*map_size,chunk_size*map_size,32,SDL_PIXELFORMAT_ARGB8888);
    for (i32 x=0;x<map_size*chunk_size;x++) {
        for (i32 y=0;y<map_size*chunk_size;y++) {
            double noise_val = MAX(MIN(noise(x,y),1.0),-1.0);
            u8 val = (u8)(((noise_val+1)/2)*255.f);
            Color col = {val,val,val,255};
            setpixel(surface,x,y,col);
        }
    }
    return surface;
}

void perlin::generate_texture(GLuint *texture) {
    if (*texture) {
        glDeleteTextures(1,texture);
    }
    glGenTextures(1,texture);

    SDL_Surface *perlin_surface = generate_surface();
    GL_load_texture_from_surface(*texture,perlin_surface);
    SDL_FreeSurface(perlin_surface);
}


void generate_texture_from_white_noise(GLuint *texture,double *noise,int n) {
    if (*texture) {
        glDeleteTextures(1,texture);
    }
    glGenTextures(1,texture);

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,n,n,32,SDL_PIXELFORMAT_ARGB8888);
    for (i32 x=0;x<n;x++) {
        for (i32 y=0;y<n;y++) {
            double noise_val = MAX(MIN(noise[x*n+y],1.0),-1.0);
            u8 val = (u8)((noise_val)*255.f);
            Color col = {val,val,val,255};
            setpixel(surface,x,y,col);
        }
    }
    GL_load_texture_from_surface(*texture,surface);
    SDL_FreeSurface(surface);
}
