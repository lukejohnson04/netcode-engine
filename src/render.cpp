
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
    SHADOW_TEXTURE,
    RAYCAST_DOT_TEXTURE,

    TEXTURE_COUNT
};

SDL_Renderer *shitty_renderer_singleton=nullptr;

SDL_Texture *textures[TEXTURE_COUNT] = {nullptr};

SDL_Texture *LoadTexture(SDL_Renderer *renderer,const char* path) {
    SDL_Texture *res = IMG_LoadTexture(renderer,path);
    if (res == NULL) {
        printf("ERROR: Could not load %s from file\n",path);
    }
    return res;
}

static void init_textures(SDL_Renderer *renderer) {
    textures[TexType::PLAYER_TEXTURE] = LoadTexture(renderer,"res/charas.png");
    textures[TexType::BULLET_TEXTURE] = LoadTexture(renderer,"res/bullet.png");
    textures[TexType::PREGAME_TEXTURE] = LoadTexture(renderer,"res/pregame_screen.png");
    textures[TexType::STARTGAME_BUTTON_TEXTURE] = LoadTexture(renderer,"res/start_game_button.png");
    textures[TexType::LEVEL_TEXTURE] = LoadTexture(renderer,"res/levels.png");
    textures[TexType::ABILITIES_TEXTURE] = LoadTexture(renderer,"res/abilities.png");
    textures[TexType::UI_TEXTURE] = LoadTexture(renderer,"res/ui.png");
    textures[TexType::BUY_MENU_WEAPONS_TEXTURE] = LoadTexture(renderer,"res/buy_menu_weapons.png");
    textures[TexType::BUY_MENU_TEXTURE] = LoadTexture(renderer,"res/buy_menu.png");
    textures[TexType::TILE_TEXTURE] = LoadTexture(renderer,"res/tiles.png");
    textures[TexType::ITEM_TEXTURE] = LoadTexture(renderer,"res/items.png");    
    textures[TexType::RAYCAST_DOT_TEXTURE] = LoadTexture(renderer,"res/dot.png");
    textures[TexType::SHADOW_TEXTURE] = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,1280,720);

    SDL_SetTextureBlendMode(textures[SHADOW_TEXTURE],SDL_BLENDMODE_MOD);

    for(i32 ind=0;ind<TEXTURE_COUNT;ind++) {
        if (textures[ind] == nullptr) {
            printf("WARNING: texture with ID %d is not loaded\n",ind);
        }
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
    v2 scale={1,1};
    iRect bound = {0,0,0,0};
    SDL_Rect get_draw_rect() {
        int w=bound.w,h=bound.h;
        if (bound.w==0||bound.h==0) {
            SDL_QueryTexture(texture,NULL,NULL,&w,&h);
        }
        SDL_Rect res={position.x,position.y,(int)((float)w*scale.x),(int)((float)h*scale.y)};
        return res;
    }
};

generic_drawable generate_text(SDL_Renderer *sdl_renderer,TTF_Font *font,std::string str,SDL_Color col={255,255,255,255}) {
    generic_drawable res;
    
    SDL_Surface* temp_surface =
        TTF_RenderText_Solid(font, str.c_str(), col);        

    if (res.texture) {
        SDL_DestroyTexture(res.texture);
    }
    res.texture = SDL_CreateTextureFromSurface(sdl_renderer, temp_surface);
    SDL_FreeSurface(temp_surface);
    return res;
}

TTF_Font *m5x7=nullptr;

void render_ability_sprite(generic_drawable *sprite,double timer,SDL_Renderer *sdl_renderer) {
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
        generic_drawable cooldown_text = generate_text(sdl_renderer,m5x7,cooldown_str,{255,0,0,0});
        cooldown_text.position = {sprite->position.x + 40,sprite->position.y + sprite->get_draw_rect().h - 16};
        SDL_Rect cl_draw_rect = cooldown_text.get_draw_rect();
        SDL_RenderCopy(sdl_renderer,cooldown_text.texture,NULL,&cl_draw_rect);
    }
}

