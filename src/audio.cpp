
enum SfxType {
    FLINTLOCK_FIRE_SFX,
    FLINTLOCK_RELOAD_SFX,
    FLINTLOCK_NO_AMMO_FIRE_SFX,
    INVISIBILITY_SFX,
    SHIELD_SFX,
    HIT_SFX,
    PLANT_SFX,

    PLANT_FINISHED_SFX,

    WIN_SFX,
    LOSE_SFX,
    
    SFX_COUNT
};

Mix_Chunk *sound_effects[SFX_COUNT];

struct singleton_sound_state {
    static const i32 channel_count=8;
    bool pool[channel_count]={false};
};

global_variable singleton_sound_state sound_state;

struct sound_event {
    SfxType type;
    i32 id=ID_DONT_EXIST;
    i32 tick=0;
    bool played=false;
};

std::vector<sound_event> sound_queue;

static void init_sfx() {
    sound_effects[SfxType::FLINTLOCK_FIRE_SFX] = Mix_LoadWAV("res/flintlock_fire.mp3");
    sound_effects[SfxType::FLINTLOCK_RELOAD_SFX] = Mix_LoadWAV("res/flintlock_reload.mp3");
    sound_effects[SfxType::FLINTLOCK_NO_AMMO_FIRE_SFX] = Mix_LoadWAV("res/flintlock_fire_not_loaded.mp3");
    sound_effects[SfxType::INVISIBILITY_SFX] = Mix_LoadWAV("res/invisibility_sfx.mp3");
    sound_effects[SfxType::SHIELD_SFX] = Mix_LoadWAV("res/shield_sfx.mp3");
    sound_effects[SfxType::WIN_SFX] = Mix_LoadWAV("res/win_sfx.mp3");
    sound_effects[SfxType::LOSE_SFX] = Mix_LoadWAV("res/lose_sfx.mp3");
    sound_effects[SfxType::HIT_SFX] = Mix_LoadWAV("res/hit_sfx.mp3");
    sound_effects[SfxType::PLANT_SFX] = Mix_LoadWAV("res/plant_sfx.mp3");
    sound_effects[SfxType::PLANT_FINISHED_SFX] = Mix_LoadWAV("res/plant_finished_sfx.mp3");

}

static void fire_forget_sfx(SfxType type) {
    for (i32 ind=0;ind<sound_state.channel_count;ind++) {
        if (sound_state.pool[ind]==false) {
            Mix_PlayChannel(ind, sound_effects[type], 0);
            sound_state.pool[ind]=true;
        }
    }
}

internal void play_sfx(SfxType type) {
    Mix_PlayChannel( -1, sound_effects[type], 0 );
}

internal void queue_sound(SfxType type, i32 id, i32 tick) {
    sound_event s = {type,id,tick};
    for (auto &event:sound_queue) {
        if (event.type == type && event.tick == tick && event.id == id) {
            return;
        }
    }
    sound_queue.push_back(s);
}


internal void queue_sound(sound_event s) {
    queue_sound(s.type,s.id,s.tick);
}
