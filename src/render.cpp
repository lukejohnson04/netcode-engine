
enum TexType {
    PLAYER_TEXTURE,
    BULLET_TEXTURE,

    // gui
    PREGAME_TEXTURE,
    STARTGAME_BUTTON_TEXTURE,
    TEXTURE_COUNT
};

SDL_Texture *textures[TEXTURE_COUNT];

static void init_textures(SDL_Renderer *renderer) {
    textures[TexType::PLAYER_TEXTURE] = IMG_LoadTexture(renderer,"../res/charas.png");
    textures[TexType::BULLET_TEXTURE] = IMG_LoadTexture(renderer,"../res/bullet.png");
    textures[TexType::PREGAME_TEXTURE] = IMG_LoadTexture(renderer,"../res/pregame_screen.png");
    textures[TexType::STARTGAME_BUTTON_TEXTURE] = IMG_LoadTexture(renderer,"../res/start_game_button.png");
}
