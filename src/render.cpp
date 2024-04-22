
enum TexType {
    PLAYER_TEXTURE,
    BULLET_TEXTURE,
    TEXTURE_COUNT
};

SDL_Texture *textures[TEXTURE_COUNT];

static void init_textures(SDL_Renderer *renderer) {
    textures[TexType::PLAYER_TEXTURE] = IMG_LoadTexture(renderer,"../res/charas.png");
    textures[TexType::BULLET_TEXTURE] = IMG_LoadTexture(renderer,"../res/bullet.png");
}
