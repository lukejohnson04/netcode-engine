

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
    SNAPSHOT_TRANSIENT_DATA,
    SNAPSHOT_PERSIST_DATA,
    INFO_DUMP_ON_CONNECTED,

    COMMAND_CALLBACK_INFO,

    NEW_CLIENT_CONNECTED,
    GAME_START_ANNOUNCEMENT,
    GMS_CHANGE,
    END_ROUND
};


// just assume all packets are 1KB for now lol
struct packet_t {
    u8 type;
    u16 size;

    union {
        
        struct {
            i32 start_tick;
            i32 end_tick;
            i32 count;
            command_t commands[64];
        } command_data;

        struct {
            entity_id id;
            //double server_time;
            //double time_of_last_tick;
            LARGE_INTEGER t_1;
            LARGE_INTEGER t_2;
            //int last_tick;
            server_header_t server_header;

        } connection_dump;

        struct {
            int server_time_on_accept;
        } ping_dump;

        struct {
            i32 client_count=0;
        } info_dump_on_connected;

        LARGE_INTEGER game_start_time;
        
        overall_game_manager gms;

        struct {
            i32 count;
            sound_event sounds[128];
        } command_callback_info;

        // transient
        snapshot_t snapshot;
    } data;
};


inline int send_packet(int socket, sockaddr_in *addr, packet_t *p) {
    return sendto(socket, (char*)p, sizeof(packet_t), 0,
                  (sockaddr*)addr, sizeof(*addr));
}

inline int recieve_packet(int socket, sockaddr_in *addr, packet_t *p) {
    socklen_t addr_len = sizeof(*addr);
    return recvfrom(socket, (char*)p, sizeof(packet_t), 0,
                    (struct sockaddr *)addr, &addr_len);
}

struct timer_t {
    LARGE_INTEGER start_time,frequency;
    LARGE_INTEGER temp_offset;

    timer_t() {
        if (!QueryPerformanceFrequency(&frequency)) {
            std::cerr << "High-resolution performance counter not supported." << std::endl;
            return;
        }
    }

    void Start() {
        QueryPerformanceCounter(&start_time);
    }

    inline LARGE_INTEGER get_high_res_elapsed() {
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        end.QuadPart -= start_time.QuadPart;
        return end;
    }
    
    double get() {
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        // Calculate the interval in seconds
        return static_cast<double>(end.QuadPart - start_time.QuadPart) / frequency.QuadPart;
    }

    void add(double time) {
        start_time.QuadPart += static_cast<LONGLONG>(time)*frequency.QuadPart;
    }

    void Restart() {
        QueryPerformanceCounter(&start_time);
    }
};
