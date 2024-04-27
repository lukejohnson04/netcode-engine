
enum SfxType {
    FLINTLOCK_FIRE_SFX,
    FLINTLOCK_RELOAD_SFX,
    FLINTLOCK_NO_AMMO_FIRE_SFX,
    SFX_COUNT
};

Mix_Chunk *sound_effects[SFX_COUNT];

static void init_sfx() {
    sound_effects[SfxType::FLINTLOCK_FIRE_SFX] = Mix_LoadWAV("../res/flintlock_fire.mp3");
    sound_effects[SfxType::FLINTLOCK_RELOAD_SFX] = Mix_LoadWAV("../res/flintlock_reload.mp3");
    sound_effects[SfxType::FLINTLOCK_NO_AMMO_FIRE_SFX] = Mix_LoadWAV("../res/flintlock_fire_not_loaded.mp3");
}

