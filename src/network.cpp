

struct server_header_t {
    double command_sliding_window_len = 0.5;
    int tickrate=40;
    int snaprate=20;
};

enum PACKET_TYPE {
    NONE,
    CONNECTION_REQUEST,
    CONNECTION_ACCEPTED,
    CONNECTION_DENIED,
    ADD_PLAYER,
    PING,
    HANDSHAKE,
    DISCONNECT,
    RANDOMIZE,
    MESSAGE,
    GS_FULL_INFO,
    GAME_DATA,
    COMMAND_DATA,
    SNAPSHOT_DATA,
    INFO_DUMP_ON_CONNECTED,

    NEW_CLIENT_CONNECTED,
    GAME_START_ANNOUNCEMENT,
    GMS_CHANGE,
    END_ROUND
};

struct snapshot_t {
    game_state state;
    double time;
};


static void load_snapshot(snapshot_t s) {
    gs = s.state;
}

// just assume all packets are 1KB for now lol
struct packet_t {
    u8 type;
    u16 size;

    union {
        char msg[1024];
        time_t time;
        struct {i32 x, y;} vec;
        entity_id id;
        u32 count;

        character new_player;

        struct {
            u32 p_count;
            character players[8];
        } full_info_dump;
        
        struct {
            double start_time;
            double end_time;
            int count;
            command_t commands[256];
        } command_data;

        struct {
            entity_id id;
            double server_time;
            double time_of_last_tick;
            int last_tick;
            server_header_t server_header;
        } connection_dump;

        struct {
            int server_time_on_accept;
        } ping_dump;

        struct {
            i32 client_count=0;
        } info_dump_on_connected;

        double game_start_time;
        
        overall_game_manager gms;
        
        game_state snapshot;

        command_t command;
    } data;
};


int send_packet(int socket, sockaddr_in *addr, packet_t *p) {
    return sendto(socket, (char*)p, sizeof(packet_t), 0,
                  (sockaddr*)addr, sizeof(*addr));
}

int recieve_packet(int socket, sockaddr_in *addr, packet_t *p) {
    socklen_t addr_len = sizeof(*addr);
    return recvfrom(socket, (char*)p, sizeof(packet_t), 0,
                    (struct sockaddr *)addr, &addr_len);
}

struct timer_t {
    LARGE_INTEGER start_time,frequency;

    void Start() {
        if (!QueryPerformanceFrequency(&frequency)) {
            std::cerr << "High-resolution performance counter not supported." << std::endl;
            return;
        }
        QueryPerformanceCounter(&start_time);
    }

    double get() {
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        // Calculate the interval in seconds
        return static_cast<double>(end.QuadPart - start_time.QuadPart) / frequency.QuadPart;
    }

    void Restart() {
        QueryPerformanceCounter(&start_time);
    }

    double Tick() {
        double val = get();
        Restart();
        return val;
    }
};

// clock_t records time relative to a global clock;
struct r_clock_t {

    r_clock_t(timer_t &t) : m_timer(t) {
        start_time = m_timer.get();
    }    

    double getElapsedTime() {
        return m_timer.get() - start_time;
    }

    void Restart() {
        start_time = m_timer.get();
    }

    double start_time;
    
private:
    timer_t &m_timer;
};
