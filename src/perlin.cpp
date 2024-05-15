
struct perlin {
    // influence vectors at the corners of each chunk
    std::vector<double> influence;
    i32 octaves=1;
    v2i map_size;
    v2i chunk_size;
    double noise(double x, double y);
};

perlin create_perlin(i32 map_width, i32 map_height, i32 chunk_width, i32 chunk_height, i32 n_octaves) {
    perlin p;
    p.octaves = n_octaves;
    p.map_size = {map_width, map_height};
    p.chunk_size = {chunk_width,chunk_height};
    p.influence.resize((map_width+1)*(map_height+1)*p.octaves);

    for (i32 octave=0;octave<p.octaves;octave++) {
        for (i32 x=0;x<map_width+1;x++) {
            for (i32 y=0;y<map_height+1;y++) {
                i32 ind = octave * (map_width+1)*(map_height+1) + (x*map_height+1) + y;
                p.influence[ind] = Random::Double(0,PI*2);
            }
        }
    }
    return p;
}

double perlin::noise(double x, double y) {
    double total=0.0;
    double frequency=1.0;
    double amplitude=1.0;
    double maxAmplitude=0.0;

    for (i32 octave=0; octave<octaves; octave++) {
        //v2i period = p.chunk_size / frequency;
        // get cx and cy, the current chunk coordinates
        i32 cx = (i32)floor(x / chunk_size.x);
        i32 cy = (i32)floor(y / chunk_size.y);

        // the index of the beginning of the current octave in the vector of influence vectors
        i32 influence_index_start = octave * (map_size.x * chunk_size.x+1)*(map_size.y * chunk_size.y+1);
        i32 t1_ind = influence_index_start + (cy*(map_size.y+1)) + cx;
        i32 t2_ind = t1_ind+1;
        i32 t3_ind = t1_ind + (map_size.y+1);
        i32 t4_ind = t3_ind+1;

        v2d t1_norm = {cos(influence[t1_ind]),sin(influence[t1_ind])};
        v2d t2_norm = {cos(influence[t2_ind]),sin(influence[t2_ind])};
        v2d t3_norm = {cos(influence[t3_ind]),sin(influence[t3_ind])};
        v2d t4_norm = {cos(influence[t4_ind]),sin(influence[t4_ind])};

        // coordinates within chunk
        i32 nx=(i32)(x-(cx*chunk_size.x));
        i32 ny=(i32)(y-(cy*chunk_size.y));
        
        v2 t1_offset = {nx,ny};
        v2 t2_offset = {nx-1,ny};
        v2 t3_offset = {nx,ny-1};
        v2 t4_offset = {nx-1,ny-1};
        
        double scalar1 = (t1_offset.x * t1_norm.x) + (t1_offset.y * t1_norm.y);
        double scalar2 = (t2_offset.x * t2_norm.x) + (t2_offset.y * t2_norm.y);
        double scalar3 = (t3_offset.x * t3_norm.x) + (t3_offset.y * t3_norm.y);
        double scalar4 = (t4_offset.x * t4_norm.x) + (t4_offset.y * t4_norm.y);

        double u = perlin_fade_d(nx);
        double v = perlin_fade_d(ny);

        // lerp horizontally, then vertically
        // then lerp the two results
        double smooth1 = lerp_d(scalar1, scalar2, u);
        double smooth2 = lerp_d(scalar3, scalar4, u);
        double final_val = lerp_d(smooth1, smooth2, v);
        total += final_val;
    }
    return total;
}
