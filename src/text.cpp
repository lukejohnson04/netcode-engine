

struct generic_drawable {
    v2i position={0,0};
    SDL_Texture *texture=nullptr;
    v2 scale={1,1};
    SDL_Rect get_draw_rect() {
        SDL_Rect res={position.x,position.y,0,0};
        SDL_QueryTexture(texture,NULL,NULL,&res.w,&res.h);
        res.w = (int)(res.w*scale.x);
        res.h = (int)(res.h*scale.y);
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


struct text_renderable {
    std::string text;
    SDL_Texture *texture=nullptr;
    
    v2i position={0,0};
    
    void generate(SDL_Renderer *sdl_renderer,std::string str,SDL_Color col={255,255,255,255}) {

        /*text = str;
        
        SDL_Surface* temp_surface =
            TTF_RenderText_Solid(font, text.c_str(), col);
        

        if (texture) {
            SDL_DestroyTexture(texture);
        }
        texture = SDL_CreateTextureFromSurface(sdl_renderer, temp_surface);
        SDL_FreeSurface(temp_surface);
        */
    }

    SDL_Rect get_draw_rect() {
        SDL_Rect res={position.x,position.y,0,0};
        SDL_QueryTexture(texture,NULL,NULL,&res.w,&res.h);
        return res;
    }
};

TTF_Font *m5x7=nullptr;
