#define MAX_CHAT_ENTRIES 16
#define MAX_CHAT_MESSAGE_LENGTH 256
#define MAX_USERNAME_LENGTH 32
#define CHAT_MESSAGE_DISPLAY_TIME 3
#define CHAT_FADE_LEN 1

struct chatlog_t {
    i32 entry_count=0;
    char raw[MAX_CHAT_ENTRIES][MAX_CHAT_MESSAGE_LENGTH];
    i32 tick_added[MAX_CHAT_ENTRIES];
};

chatlog_t chatlog;

struct chatlog_display_t {
    generic_drawable sprites[MAX_CHAT_ENTRIES];
};

chatlog_display_t chatlog_display;

void add_to_chatlog(std::string sender_name,std::string message,i32 tick,chatlog_display_t *disp=nullptr) {
    std::string fin_str = sender_name + ": " + message;
    chatlog.entry_count++;
    memcpy(chatlog.raw[chatlog.entry_count-1],fin_str.c_str(),fin_str.size());
    chatlog.tick_added[chatlog.entry_count-1] = tick;

    if (disp) {
        disp->sprites[chatlog.entry_count-1] = generate_text(m5x7,fin_str,{0,100,255,255});
        disp->sprites[chatlog.entry_count-1].scale = {2,2};
        for (i32 ind=0; ind<chatlog.entry_count;ind++) {
            i32 dist_to_first = chatlog.entry_count - ind;
            disp->sprites[ind].position = {48,500 - dist_to_first * disp->sprites[ind].get_draw_rect().h - 8};
        }
    }
}
