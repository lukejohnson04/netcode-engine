
enum SfxType {
    FLINTLOCK_FIRE_SFX,
    FLINTLOCK_RELOAD_SFX,
    FLINTLOCK_NO_AMMO_FIRE_SFX,
    INVISIBILITY_SFX,
    SHIELD_SFX,
    HIT_SFX,

    PLANT_SFX,
    PLANT_FINISHED_SFX,

    BOMB_BEEP_SFX,
    BOMB_DETONATE_SFX,
    BOMB_DEFUSE_SFX,

    BOMB_DEFUSE_FINISH_SFX,

    WIN_SFX,
    LOSE_SFX,

    VO_FAVORABLE,
    VO_DISPLEASUARBLE,
    VO_BOMBSITE_A,
    VO_BOMBSITE_AA,
    VO_HES_OVER_THERE,
    VO_UM_SO_THAT_JUST_HAPPENED,
    VO_IM_GOING_TO_KILL_THEM,
    VO_UM_ITS_CLOBBER_TIME,
    
    SFX_COUNT
};

global_variable Mix_Chunk *sound_effects[SFX_COUNT] = {nullptr};

struct sound_event {
    SfxType type;
    i32 id=ID_DONT_EXIST;
    i32 tick=0;
    bool played=false;
};

std::vector<sound_event> sound_queue;

Mix_Chunk *LoadSfx(const char* path) {
    Mix_Chunk *res = Mix_LoadWAV(path);
    if (res == NULL) {
        printf("ERROR: Could not load %s from file\n",path);
    }
    return res;
}

static void init_sfx() {
    sound_effects[SfxType::FLINTLOCK_FIRE_SFX] = LoadSfx("res/flintlock_fire.mp3");
    sound_effects[SfxType::FLINTLOCK_RELOAD_SFX] = LoadSfx("res/flintlock_reload.mp3");
    sound_effects[SfxType::FLINTLOCK_NO_AMMO_FIRE_SFX] = LoadSfx("res/flintlock_fire_not_loaded.mp3");
    sound_effects[SfxType::INVISIBILITY_SFX] = LoadSfx("res/invisibility_sfx.mp3");
    sound_effects[SfxType::SHIELD_SFX] = LoadSfx("res/shield_sfx.mp3");
    sound_effects[SfxType::WIN_SFX] = LoadSfx("res/win_sfx.mp3");
    sound_effects[SfxType::LOSE_SFX] = LoadSfx("res/lose_sfx.mp3");
    sound_effects[SfxType::HIT_SFX] = LoadSfx("res/hit_sfx.mp3");
    sound_effects[SfxType::PLANT_SFX] = LoadSfx("res/plant_sfx.mp3");
    sound_effects[SfxType::PLANT_FINISHED_SFX] = LoadSfx("res/plant_finished_sfx.mp3");
    sound_effects[SfxType::BOMB_BEEP_SFX] = LoadSfx("res/bomb_beep.mp3");
    sound_effects[SfxType::BOMB_DETONATE_SFX] = LoadSfx("res/bomb_detonate.mp3");
    sound_effects[SfxType::BOMB_DEFUSE_SFX] = LoadSfx("res/bomb_defuse.mp3");
    sound_effects[SfxType::BOMB_DEFUSE_FINISH_SFX] = LoadSfx("res/bomb_defuse_finish.mp3");

    sound_effects[SfxType::VO_FAVORABLE] = LoadSfx("res/vo/vo_favorable.wav");
    sound_effects[SfxType::VO_DISPLEASUARBLE] = LoadSfx("res/vo/vo_displeasurable.wav");
    sound_effects[SfxType::VO_BOMBSITE_A] = LoadSfx("res/vo/vo_bombsite_a.wav");
    sound_effects[SfxType::VO_BOMBSITE_AA] = LoadSfx("res/vo/vo_bombsite_aa.wav");
    sound_effects[SfxType::VO_HES_OVER_THERE] = LoadSfx("res/vo/vo_hes_over_there.wav");
    sound_effects[SfxType::VO_UM_SO_THAT_JUST_HAPPENED] = LoadSfx("res/vo/vo_um_so_that_just_happened.wav");
    sound_effects[SfxType::VO_IM_GOING_TO_KILL_THEM] = LoadSfx("res/vo/vo_im_going_to_kill_them.wav");
    sound_effects[SfxType::VO_UM_ITS_CLOBBER_TIME] = LoadSfx("res/vo/vo_um_its_clobber_time.wav");

    Mix_VolumeChunk(sound_effects[SfxType::BOMB_BEEP_SFX],80);
    
    for(i32 ind=0;ind<SFX_COUNT;ind++) {
        if (sound_effects[ind] == nullptr) {
            printf("WARNING: sfx with ID %d is not loaded\n",ind);
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
    if (type==SfxType::PLANT_FINISHED_SFX) printf("Added bomb plant sfx to queue\n");
    sound_queue.push_back(s);
}


internal void queue_sound(sound_event s) {
    queue_sound(s.type,s.id,s.tick);
}
