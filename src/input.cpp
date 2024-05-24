
struct InputState {
    bool is_pressed[256] = {false};
    bool just_pressed[256] = {false};
    bool just_released[256] = {false};

    bool mouse_just_pressed = false;
    bool mouse_just_released = false;
    bool mouse_pressed = false;

    bool mouse_just_middle_pressed = false;
    bool mouse_just_middle_released = false;
    bool mouse_middle_pressed = false;

    bool mouse_just_right_pressed = false;
    bool mouse_just_right_released = false;
    bool mouse_right_pressed = false;
    
    i32 mouse_move_x=0;
    i32 mouse_move_y=0;

    bool scrolled_up=false;
    bool scrolled_down=false;

    bool text_input_captured=false;
    bool text_modified=false;
    bool text_submitted=false;
    std::string *input_field=nullptr;
};

global_variable InputState input;

void PollEvents(InputState *state, bool *running) {
    memset(state->just_pressed, 0, 256*sizeof(bool));
    memset(state->just_released, 0, 256*sizeof(bool));
    
    state->mouse_just_pressed=false;
    state->mouse_just_released=false;
    state->text_modified=false;
    state->text_submitted=false;

    state->scrolled_up=false;
    state->scrolled_down=false;

    state->mouse_just_middle_pressed = false;
    state->mouse_just_middle_released = false;

    state->mouse_just_right_pressed = false;
    state->mouse_just_right_released = false;

    
    state->mouse_move_x=0;
    state->mouse_move_y=0;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE && state->text_input_captured && state->input_field && state->input_field->size() > 0) {
            //lop off character
            state->input_field->pop_back();
            state->text_modified = true;

        } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN && state->text_input_captured) {
            state->text_submitted=true;
        } else if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
            state->just_pressed[e.key.keysym.scancode] = true;
            state->is_pressed[e.key.keysym.scancode] = true;
        } else if (e.type == SDL_KEYUP) {
            state->just_released[e.key.keysym.scancode] = true;
            state->is_pressed[e.key.keysym.scancode] = false;
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                state->mouse_pressed = true;
                state->mouse_just_pressed = true;
            } else if (e.button.button == SDL_BUTTON_MIDDLE) {
                state->mouse_middle_pressed = true;
                state->mouse_just_middle_pressed = true;
            } else if (e.button.button == SDL_BUTTON_RIGHT) {
                state->mouse_right_pressed = true;
                state->mouse_just_right_pressed = true;
            }
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                state->mouse_pressed = false;
                state->mouse_just_released = true;
            } else if (e.button.button == SDL_BUTTON_MIDDLE) {
                state->mouse_middle_pressed = false;
                state->mouse_just_middle_released = true;
            } else if (e.button.button == SDL_BUTTON_RIGHT) {
                state->mouse_right_pressed = false;
                state->mouse_just_right_released = true;
            }
        // text input
        } else if (e.type == SDL_MOUSEWHEEL) {
            if (e.wheel.y > 0) {
                state->scrolled_up=true;
            } else {
                state->scrolled_down=true;
            }

        } else if (e.type == SDL_MOUSEMOTION) {
            state->mouse_move_x = e.motion.xrel;
            state->mouse_move_y = e.motion.yrel;
        
        } else if (e.type == SDL_TEXTINPUT && state->input_field) {
            //Not copy or pasting
            if( !( SDL_GetModState() & KMOD_CTRL && ( e.text.text[ 0 ] == 'c' || e.text.text[ 0 ] == 'C' || e.text.text[ 0 ] == 'v' || e.text.text[ 0 ] == 'V' ) ) )
            {
                //Append character
                *state->input_field += e.text.text;
                state->text_modified = true;
            }
        }

        if (e.type == SDL_QUIT) {
            *running = false;
        }
    }
}

v2i get_mouse_position() {
    v2i pos;
    SDL_GetMouseState(&pos.x,&pos.y);
    return pos;
}
