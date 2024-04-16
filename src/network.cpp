

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
    SNAPSHOT_DATA
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
            server_header_t server_header;
        } connection_dump;

        struct {
            int server_time_on_accept;
        } ping_dump;
        
        snapshot_t snapshot;

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
};
