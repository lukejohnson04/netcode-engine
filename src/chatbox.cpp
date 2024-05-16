
void client_update_chatbox(i32 target_tick) {
    local_persist bool chatbox_isopen=false;
    local_persist std::string chatbox_text="";
    if (chatbox_isopen == false && input.just_pressed[SDL_SCANCODE_T]) {
        chatbox_isopen = true;
        SDL_StartTextInput();
        input.text_input_captured=true;
        input.input_field = &chatbox_text;
        input.text_modified=true;

    } else if (chatbox_isopen && input.just_pressed[SDL_SCANCODE_ESCAPE]) {
        chatbox_isopen=false;
        SDL_StopTextInput();
        input.text_input_captured=false;
        input.input_field = nullptr;
    }

    if (chatbox_isopen) {
        // if you press the enter key, submit that field!
        iRect text_input_rect = {48,500+40,400,36};
        glUseProgram(sh_colorProgram);
        GL_DrawRect(text_input_rect,{255,255,255,100});

        local_persist generic_drawable chatbox_entry;
        if (input.text_submitted) {
            // send to server
            packet_t p = {};
            p.type = CHAT_MESSAGE;

            strcpy_s(p.data.chat_message.message,MAX_CHAT_MESSAGE_LENGTH,chatbox_text.c_str());
            strcpy_s(p.data.chat_message.name,MAX_USERNAME_LENGTH,client_st.username.c_str());
            // ought to put this in the client tick code? idk man. doesn't matter too much tho
            send_packet(client_st.socket,&client_st.servaddr,&p);
            //add_to_chatlog("Luke","Hello",target_tick,&chatlog_display);
            //add_to_chatlog("Luke","Hello",target_tick,&chatlog_display);

            //add_to_chatlog("Luke",chatbox_text,target_tick,&chatlog_display);
            chatbox_text="";
            chatbox_isopen=false;
                    
            SDL_StopTextInput();
            input.text_submitted = false;
            input.text_input_captured=false;
            input.input_field = nullptr;
        } else if (input.text_modified) {
                    
            chatbox_entry = generate_text(m5x7,chatbox_text==""?" ":chatbox_text,{255,255,255,255},chatbox_entry.gl_texture);
            chatbox_entry.scale = {1,1};
            chatbox_entry.position = {text_input_rect.x + 4, text_input_rect.y + text_input_rect.h - chatbox_entry.get_draw_irect().h-4};
        }

        if (chatbox_entry.gl_texture) {
            glUseProgram(sh_textureProgram);
            GL_DrawTexture(chatbox_entry.gl_texture,chatbox_entry.get_draw_irect());
        }
    }

    glUseProgram(sh_textureProgram);
    for (i32 ind=chatlog.entry_count-1;ind>=0;ind--) {
        i32 fade_start_tick = chatlog.tick_added[ind]+(CHAT_MESSAGE_DISPLAY_TIME*60);
        i32 fade_end_tick = fade_start_tick + (CHAT_FADE_LEN * 60);
        if (true || target_tick < fade_end_tick) {
            if (false && target_tick > fade_start_tick) {
                // fade out
                u8 a = 255 - (u8)(((double)(target_tick - fade_start_tick) / (double)(fade_end_tick - fade_start_tick)) * 255.0);
                //glUniform4f(colUni,255,255,255,a);
            } else {
                //glUniform4f(colUni,255,255,255,255);
            }
            iRect dest = chatlog_display.sprites[ind].get_draw_irect();
            // TODO: why TF is this width and height constantly stuck at 0???
            GL_DrawTexture(chatlog_display.sprites[ind].gl_texture,dest);
        }
    }

}
