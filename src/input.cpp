
struct InputState {
    bool is_pressed[256] = {false};
    bool just_pressed[256] = {false};
    bool just_released[256] = {false};

    bool mouse_just_pressed = false;
    bool mouse_just_released = false;
    bool mouse_pressed = false;
};

global_variable InputState input;

void PollEvents(InputState *state, bool *running) {
    memset(state->just_pressed, 0, 256*sizeof(bool));
    memset(state->just_released, 0, 256*sizeof(bool));
    
    state->mouse_just_pressed=false;
    state->mouse_just_released=false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
            state->just_pressed[e.key.keysym.scancode] = true;
            state->is_pressed[e.key.keysym.scancode] = true;
        } else if (e.type == SDL_KEYUP) {
            state->just_released[e.key.keysym.scancode] = true;
            state->is_pressed[e.key.keysym.scancode] = false;
        }

        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            state->mouse_pressed = true;
            state->mouse_just_pressed = true;
        } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            state->mouse_pressed = false;
            state->mouse_just_released = true;
        }

        if (e.type == SDL_QUIT) {
            *running = false;
        }
    }
}
