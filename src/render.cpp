
enum TexType {
    PLAYER_TEXTURE,
    BULLET_TEXTURE,
    TILE_TEXTURE,

    // gui
    ITEM_TEXTURE,
    PREGAME_TEXTURE,
    STARTGAME_BUTTON_TEXTURE,
    ABILITIES_TEXTURE,
    UI_TEXTURE,
    BUY_MENU_WEAPONS_TEXTURE,
    BUY_MENU_TEXTURE,

    // metagame
    LEVEL_TEXTURE,
    STATIC_MAP_TEXTURE,
    RAYCAST_DOT_TEXTURE,
    TX_GAME_WORLD,
    TX_GAME_OBJECTS,
    TX_SHADOW,
    
    TEXTURE_COUNT
};

enum VaoType {
    TEXTURE_VAO,
    SHADOW_VAO,
    VAO_COUNT
};

enum VboType {
    TEXTURE_VBO,
    SHADOW_VBO,
    VBO_COUNT
};

enum FrameBufferType {
    FB_GAME_WORLD,
    FB_GAME_OBJECTS,
    FB_SHADOW,
    FB_COUNT
};


GLuint sh_textureProgram, sh_colorProgram, sh_modProgram;
TTF_Font *m5x7=nullptr;

SDL_Texture *textures[TEXTURE_COUNT] = {nullptr};
GLuint gl_textures[TEXTURE_COUNT] = {NULL};
GLuint gl_varrays[VAO_COUNT] = {NULL};
GLuint gl_vbuffers[VBO_COUNT] = {NULL};
GLuint gl_framebuffers[FB_COUNT] = {NULL};

std::string readShaderFile(const std::string &shaderPath) {
    std::ifstream shaderFile;
    std::stringstream shaderStream;

    // Open file
    shaderFile.open(shaderPath);
    if (!shaderFile.is_open()) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: Cannot open file " << shaderPath << std::endl;
        return "";
    }

    // Read file's buffer contents into streams
    shaderStream << shaderFile.rdbuf();
    shaderFile.close(); // Close file handlers

    // Convert stream into string
    return shaderStream.str();
}

GLuint compileShader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

GLuint createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexCode = readShaderFile(vertexPath);
    std::string fragmentCode = readShaderFile(fragmentPath);
    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    // Compile shaders
    GLuint vertexShader = compileShader(vShaderCode, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fShaderCode, GL_FRAGMENT_SHADER);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}


SDL_Texture *LoadTexture(SDL_Renderer *renderer,const char* path) {
    SDL_Texture *res = IMG_LoadTexture(renderer,path);
    if (res == NULL) {
        printf("ERROR: Could not load %s from file\n",path);
    }
    return res;
}




internal void GL_load_texture(GLuint tex, const char* path) {
    SDL_Surface *tex_surf = IMG_Load(path);
    if (tex_surf == NULL) {
        std::cout << "ERROR: Failed to load texture at path " << path << std::endl;
        return;
    }

    int Mode = GL_RGB;
    if(tex_surf->format->BytesPerPixel == 4) {
        Mode = GL_RGBA;
        printf("alpha");
    }

    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_surf->w, tex_surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_surf->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}



internal GLuint GL_create_framebuffer(GLuint texture) {
    GLuint fb;
    glGenFramebuffers(1,&fb);
    glBindFramebuffer(GL_FRAMEBUFFER,fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return fb;
}

internal void GL_load_texture_for_framebuffer(GLuint texture,i32 width=WINDOW_WIDTH,i32 height=WINDOW_HEIGHT) {
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D,0);
}

internal void GL_load_texture_from_surface(GLuint tex,SDL_Surface *tex_surf) {
    GLenum format = 0, type = GL_UNSIGNED_BYTE; // Default to unsigned byte for type

    switch (tex_surf->format->format) {
        case SDL_PIXELFORMAT_RGBA8888:
            format = GL_RGBA;
            break;
        case SDL_PIXELFORMAT_ABGR8888:
            format = GL_BGRA;
            break;
        case SDL_PIXELFORMAT_BGRA8888:
            format = GL_BGRA;
            break;
        case SDL_PIXELFORMAT_ARGB8888:
            format = GL_BGRA;
            break;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_RGB24:
            format = GL_RGB;
            break;
        case SDL_PIXELFORMAT_BGR888:
            format = GL_BGR;
            break;
        case SDL_PIXELFORMAT_RGB565:
            format = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
            break;
        case SDL_PIXELFORMAT_RGBA4444:
            format = GL_RGBA;
            type = GL_UNSIGNED_SHORT_4_4_4_4;
            break;
        case SDL_PIXELFORMAT_RGB332:
            format = GL_RGB;
            type = GL_UNSIGNED_BYTE_3_3_2;
            break;
        default:
            break;
    }

    if (format == 0) {
        std::cerr << "No valid OpenGL format found. Texture creation aborted." << std::endl;
        return;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, format == GL_RGB ? GL_RGB8 : GL_RGBA8, tex_surf->w, tex_surf->h, 0, format, type, tex_surf->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
}


internal GLuint GL_load_texture(const char* path) {
    GLuint res;
    glGenTextures(1,&res);
    GL_load_texture(res,path);
    return res;
}


static void init_textures() {
    sh_textureProgram = createShaderProgram("../src/sh_texture.vert","../src/sh_texture.frag");
    sh_colorProgram = createShaderProgram("../src/sh_color.vert","../src/sh_color.frag");
    sh_modProgram = createShaderProgram("../src/sh_mod.vert","../src/sh_mod.frag");

    glUseProgram(sh_textureProgram);
    GLuint projLoc = glGetUniformLocation(sh_textureProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    GLuint tintLoc_col = glGetUniformLocation(sh_textureProgram, "colorMod");
    glUniform4f(tintLoc_col, 1.0f,1.0f,1.0f,1.0f);

    glUseProgram(sh_colorProgram);
    GLuint projLoc_col = glGetUniformLocation(sh_colorProgram, "projection");
    glUniformMatrix4fv(projLoc_col, 1, GL_FALSE, glm::value_ptr(projection));
    
    glUseProgram(sh_modProgram);
    GLuint projLoc_mod = glGetUniformLocation(sh_modProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    GLuint modelLoc_mod = glGetUniformLocation(sh_modProgram, "model");
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLoc_mod, 1, GL_FALSE, glm::value_ptr(model));
    
    glGenTextures(TEXTURE_COUNT,gl_textures);
    glGenVertexArrays(VAO_COUNT,gl_varrays);
    glGenBuffers(VBO_COUNT,gl_vbuffers);
    glGenFramebuffers(FB_COUNT,gl_framebuffers);

    // shadow mask frame buffer
    GL_load_texture_for_framebuffer(gl_textures[TX_GAME_OBJECTS]);
    GL_load_texture_for_framebuffer(gl_textures[TX_GAME_WORLD]);
    GL_load_texture_for_framebuffer(gl_textures[TX_SHADOW]);
    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_SHADOW]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_textures[TX_SHADOW], 0);
    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_GAME_OBJECTS]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_textures[TX_GAME_OBJECTS], 0);
    glBindFramebuffer(GL_FRAMEBUFFER,gl_framebuffers[FB_GAME_WORLD]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_textures[TX_GAME_WORLD], 0);
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    GL_load_texture(gl_textures[PLAYER_TEXTURE],"res/charas.png");    
    GL_load_texture(gl_textures[BULLET_TEXTURE],"res/bullet.png");    
    GL_load_texture(gl_textures[PREGAME_TEXTURE],"res/pregame_screen.png");    
    GL_load_texture(gl_textures[STARTGAME_BUTTON_TEXTURE],"res/start_game_button.png");    
    GL_load_texture(gl_textures[LEVEL_TEXTURE],"res/levels.png");    
    GL_load_texture(gl_textures[ABILITIES_TEXTURE],"res/abilities.png");    
    GL_load_texture(gl_textures[UI_TEXTURE],"res/ui.png");
    GL_load_texture(gl_textures[BUY_MENU_WEAPONS_TEXTURE],"res/buy_menu_weapons.png");
    GL_load_texture(gl_textures[BUY_MENU_TEXTURE],"res/buy_menu.png");
    GL_load_texture(gl_textures[TILE_TEXTURE],"res/tiles.png");
    GL_load_texture(gl_textures[ITEM_TEXTURE],"res/items.png");
    GL_load_texture(gl_textures[RAYCAST_DOT_TEXTURE],"res/dot.png");

    //GL_load_texture(gl_textures[WORLD_OBJECTS_TEXTURE],"res/dot.png");
    // come back to this
    //gl_textures[WORLD_OBJECTS_TEXTURE] = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,1280,720);
    //textures[TexType::SHADOW_TEXTURE] = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,1280,720);
    //SDL_SetTextureBlendMode(textures[SHADOW_TEXTURE],SDL_BLENDMODE_MOD);
    //SDL_SetTextureBlendMode(textures[WORLD_OBJECTS_TEXTURE],SDL_BLENDMODE_BLEND);

    //textures[TexType::STATIC_MAP_TEXTURE] = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,32*64,32*64);

    /*
    SDL_SetRenderDrawColor(renderer,255,255,0,255);
    SDL_SetRenderTarget(renderer,textures[TexType::STATIC_MAP_TEXTURE]);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer,NULL);
    */

    // VAOs

    
    for(i32 ind=0;ind<TEXTURE_COUNT;ind++) {
        if (gl_textures[ind] == NULL) {
            printf("WARNING: texture with ID %d is not loaded\n",ind);
        }
    }
    m5x7 = TTF_OpenFont("res/m5x7.ttf",32);
    if (m5x7 == nullptr) {
        printf("ERROR: failed to load res/m5x7.ttf!\n");
    }
}


Uint32 getpixel(SDL_Surface *surface, int x, int y) {
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp)
    {
        case 1:
            return *p;
            break;

        case 2:
            return *(Uint16 *)p;
            break;

        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                return p[0] << 16 | p[1] << 8 | p[2];
            else
                return p[0] | p[1] << 8 | p[2] << 16;
            break;

        case 4:
            return *(Uint32 *)p;
            break;

        default:
            return 0;       /* shouldn't happen, but avoids warnings */
    }
}


struct character;
struct camera_t {
    v2 pos={0,0};
    v2 size = {1280,720};
    union {
        character *follow;
    };

    
    iRect get_draw_rect() {
        return {(int)(pos.x-size.x/2),(int)(pos.y-size.y/2),(int)size.x,(int)size.y};
    }
    

    v2 get_center() {
        return pos + (v2(size.x/2.f,size.y/2.f));
    }
};

camera_t *global_cam=nullptr;

struct generic_drawable {
    v2i position={0,0};
    SDL_Texture *texture=nullptr;
    GLuint gl_texture=NULL;
    v2 scale={1,1};
    iRect bound = {0,0,0,0};

    /*
    SDL_Rect get_draw_rect() {
        if (bound.w==0||bound.h==0) {
            glBindTexture(GL_TEXTURE_2D,gl_texture);
            // mipmap level is 0
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &bound.h);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &bound.h);

            //SDL_QueryTexture(texture,NULL,NULL,&w,&h);
        }
        int w=bound.w,h=bound.h;
        SDL_Rect res={position.x,position.y,(int)((float)w*scale.x),(int)((float)h*scale.y)};
        return res;
    }
    */

    iRect get_draw_irect() {
        if (bound.w==0||bound.h==0) {
            glBindTexture(GL_TEXTURE_2D,gl_texture);
            // mipmap level is 0
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &bound.w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &bound.h);

            //SDL_QueryTexture(texture,NULL,NULL,&w,&h);
        }
        int w=bound.w,h=bound.h;
        iRect res={position.x,position.y,(int)((float)w*scale.x),(int)((float)h*scale.y)};
        return res;
    }
};

internal generic_drawable generate_text(TTF_Font *font,std::string str,Color col={255,255,255,255},GLuint tex=NULL) {
    generic_drawable res;
    
    SDL_Surface* temp_surface =
        TTF_RenderText_Solid(font, str.c_str(),*(SDL_Color*)&col);
    temp_surface = SDL_ConvertSurfaceFormat(temp_surface, SDL_PIXELFORMAT_ARGB8888, 0);

    if (tex != NULL) {
        glDeleteTextures(1,&tex);
    }
    glGenTextures(1,&tex);

    res.gl_texture = tex;
    GL_load_texture_from_surface(res.gl_texture,temp_surface);
    
    SDL_FreeSurface(temp_surface);
    return res;
}


void render_ability_sprite(generic_drawable *sprite,double timer) {
    /*
    SDL_Rect draw_rect = sprite->get_draw_rect();
    SDL_RenderCopy(sdl_renderer,sprite->texture,(SDL_Rect*)&sprite->bound,&draw_rect);
    if (timer > 0) {
        std::string cooldown_str = std::to_string(timer);
        if (cooldown_str.size()>3) {
            for (i32 ind=0;ind<cooldown_str.size();ind++) {
                if (cooldown_str[ind]=='.') {
                    cooldown_str.erase(cooldown_str.begin()+ind+2,cooldown_str.end());
                    break;
                }
            }
        }
        generic_drawable cooldown_text = generate_text(m5x7,cooldown_str,{255,0,0,0});
        cooldown_text.position = {sprite->position.x + 40,sprite->position.y + sprite->get_draw_rect().h - 16};
        SDL_Rect cl_draw_rect = cooldown_text.get_draw_rect();
        SDL_RenderCopy(sdl_renderer,cooldown_text.texture,NULL,&cl_draw_rect);
    }
    */
}


/*

GLuint compileShader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader,1,&source,NULL);
    glCompileShader(shader);
    return shader;
}

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // Check for linking errors here
    return shaderProgram;
}

 */

void GL_DrawRect(iRect rect, Color color=COLOR_BLACK) {
    float vertices[] = {
        (float)rect.x+rect.w,    (float)rect.y,           0.0f,
        (float)rect.x+rect.w,    (float)rect.y+rect.h,    0.0f,
        (float)rect.x,           (float)rect.y+rect.h,    0.0f,
        (float)rect.x,           (float)rect.y,           0.0f
    };

    GLuint colUni = glGetUniformLocation(sh_colorProgram, "color");
    float colorF[] = {(float)color.r/255.f,(float)color.g/255.f,(float)color.b/255.f,(float)color.a/255.f};
    glUniform4f(colUni, colorF[0],colorF[1],colorF[2],colorF[3]);

    // we don't have to repass the projection every frame, unless it changes
    local_persist GLuint VAO,VBO;
    local_persist bool generated=false;
    if (!generated) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        generated=true;
    }
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER,0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);    
}


void GL_DrawTexture(GLuint texture, iRect dest={0,0,0,0}, iRect src={0,0,0,0}) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,texture);

    float u1 = 0.0f;
    float u2 = 1.0f;
    float v1 = 0.0f;
    float v2 = 1.0f;

    v2i tex_size = {0,0};
    if (dest.w==0 || src.w!=0) {
        // store this info in some data structure maybe?
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_size.x);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_size.y);
        if (src.w!=0) {
            u1 = (float)src.x / (float)tex_size.x;
            u2 = (float)(src.x + src.w) / (float)tex_size.x;
            v1 = (float)src.y / (float)tex_size.y;
            v2 = (float)(src.y + src.h) / (float)tex_size.y;
        }
    } if (dest.w==0) {
        dest.w = tex_size.x;
        dest.h = tex_size.y;
    }
    
    float vertices[] = {
        (float)dest.x,           (float)dest.y,        0.0f, u1, v1,
        (float)dest.x+dest.w,    (float)dest.y,        0.0f, u2, v1,
        (float)dest.x+dest.w,    (float)dest.y+dest.h, 0.0f, u2, v2,
        (float)dest.x,           (float)dest.y+dest.h, 0.0f, u1, v2,
    };
    
    // uniforms
    GLint textureLoc = glGetUniformLocation(sh_textureProgram,"_texture");
    glUniform1i(textureLoc,0);    
    glm::mat4 model = glm::mat4(1.0f);
    GLint transformLoc = glGetUniformLocation(sh_textureProgram,"model");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(model));    
    
    glBindVertexArray(gl_varrays[TEXTURE_VAO]);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbuffers[TEXTURE_VBO]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(gl_varrays[TEXTURE_VAO]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER,0);    
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void GL_DrawTextureEx(GLuint texture, iRect dest={0,0,0,0}, iRect src={0,0,0,0}, bool flip_x=false, bool flip_y=false, float rotation=0) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,texture);

    float u1= !flip_x ? 0.0f : 1.0f;
    float u2= !flip_x ? 1.0f : 0.0f;
    float v1= !flip_y ? 0.0f : 1.0f;
    float v2= !flip_y ? 1.0f : 0.0f;

    v2i tex_size = {0,0};
    if (dest.w==0 || src.w!=0) {
        // store this info in some data structure maybe?
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_size.x);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_size.y);
        if (src.w!=0) {
            (flip_x ? u2 : u1) = (float)src.x / (float)tex_size.x;
            (flip_x ? u1 : u2) = (float)(src.x + src.w) / (float)tex_size.x;
            (flip_y ? v2 : v1) = (float)src.y / (float)tex_size.y;
            (flip_y ? v1 : v2) = (float)(src.y + src.h) / (float)tex_size.y;
        }
    } if (dest.w==0) {
        dest.w = tex_size.x;
        dest.h = tex_size.y;
    }
    
    float vertices[] = {
        (float)dest.x,           (float)dest.y,        0.0f, u1, v1,
        (float)dest.x+dest.w,    (float)dest.y,        0.0f, u2, v1,
        (float)dest.x+dest.w,    (float)dest.y+dest.h, 0.0f, u2, v2,
        (float)dest.x,           (float)dest.y+dest.h, 0.0f, u1, v2,
    };
        
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbuffers[TEXTURE_VBO]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // uniforms
    GLint textureLoc = glGetUniformLocation(sh_textureProgram,"_texture");
    glUniform1i(textureLoc,0);
    
    glm::mat4 model = glm::mat4(1.0f);
    if (rotation != 0) {
        glm::vec2 pos = glm::vec2(dest.x, dest.y);
        float angleRadians = rotation;
        // Calculate the center of the object for rotation
        glm::vec2 size = glm::vec2(dest.w,dest.h);
        glm::vec2 center = pos + size * 0.5f;
        model = glm::translate(model, glm::vec3(center, 0.0f)); // Move pivot to center
        model = glm::rotate(model, angleRadians, glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate
        model = glm::translate(model, glm::vec3(-center, 0.0f)); // Move pivot back
    }
    
    GLint transformLoc = glGetUniformLocation(sh_textureProgram,"model");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(gl_varrays[TEXTURE_VAO]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER,0);
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    model = glm::mat4(1.0f);
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(model));
}
