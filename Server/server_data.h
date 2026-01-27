#ifndef SERVER_DATA_H
#define SERVER_DATA_H

#define MAX_CLIENTS 10
#define MAX_ROOMS 10

#define STATE_DISCONNECTED 0
#define STATE_CONNECTED    1
#define STATE_LOBBY        2
#define STATE_IN_ROOM      3

#include <stdio.h>

typedef struct {
    int socket_fd;
    int state;
    char username[32];
    int current_room_id;
} Player;

typedef struct {
    int room_id;
    int player_count; 
    int status; // 0=Wait, 1=Playing
    
    int white_player_idx; // P1 (房主/红方)
    int black_player_idx; // P2 (加入者/黑方)
    
    int white_ready;      // 0=未准备, 1=已准备
    int black_ready;      // 0=未准备, 1=已准备
} Room;

extern Player players[MAX_CLIENTS];
extern Room rooms[MAX_ROOMS];

#endif