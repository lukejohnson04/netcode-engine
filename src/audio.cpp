
enum SfxType {
    FLINTLOCK_FIRE_SFX,
    FLINTLOCK_RELOAD_SFX,
    FLINTLOCK_NO_AMMO_FIRE_SFX,
    INVISIBILITY_SFX,
    SHIELD_SFX,
    SFX_COUNT
};

Mix_Chunk *sound_effects[SFX_COUNT];

struct singleton_sound_state {
    static const i32 channel_count=8;
    bool pool[channel_count]={false};
};

global_variable singleton_sound_state sound_state;

static void init_sfx() {
    sound_effects[SfxType::FLINTLOCK_FIRE_SFX] = Mix_LoadWAV("res/flintlock_fire.mp3");
    sound_effects[SfxType::FLINTLOCK_RELOAD_SFX] = Mix_LoadWAV("res/flintlock_reload.mp3");
    sound_effects[SfxType::FLINTLOCK_NO_AMMO_FIRE_SFX] = Mix_LoadWAV("res/flintlock_fire_not_loaded.mp3");
    sound_effects[SfxType::INVISIBILITY_SFX] = Mix_LoadWAV("res/invisibility_sfx.mp3");
    sound_effects[SfxType::SHIELD_SFX] = Mix_LoadWAV("res/shield_sfx.mp3");
}

static void fire_forget_sfx(SfxType type) {
    for (i32 ind=0;ind<sound_state.channel_count;ind++) {
        if (sound_state.pool[ind]==false) {
            Mix_PlayChannel(ind, sound_effects[type], 0);
            sound_state.pool[ind]=true;
        }
    }
}
