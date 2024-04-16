enum TexType {
    PLAYER_TEXTURE,
    TEXTURE_COUNT
};

SDL_Texture *textures[TEXTURE_COUNT];

static void init_textures(SDL_Renderer *renderer) {
    textures[TexType::PLAYER_TEXTURE] = IMG_LoadTexture(renderer,"../res/charas.png");
}
