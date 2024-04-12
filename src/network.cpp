
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
            int start_time;
            int end_time;
            int count;
            command_t commands[256];
        } command_data;

        struct {
            entity_id id;
            int server_time;
        } connection_dump;

        struct {
            int server_time_on_accept;
        } ping_dump;

        
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


struct snapshot_t {
    game_state state;
    int time;
};


static void load_snapshot(snapshot_t s) {
    gs = s.state;
}
