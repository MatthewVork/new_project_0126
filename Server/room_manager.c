#include <stdio.h>
#include "server_data.h"

void init_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].room_id = i;
        rooms[i].player_count = 0;
        rooms[i].status = 0;
        rooms[i].white_player_idx = -1;
        rooms[i].black_player_idx = -1;
        rooms[i].white_ready = 0;
        rooms[i].black_ready = 0;
    }
}

int create_room_logic(int player_idx) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].player_count == 0) {
            rooms[i].player_count = 1;
            rooms[i].status = 0;
            rooms[i].white_player_idx = player_idx; // 房主
            rooms[i].black_player_idx = -1;
            rooms[i].white_ready = 0;
            rooms[i].black_ready = 0;
            printf("[Room] Room %d created by %s\n", i, players[player_idx].username);
            return i;
        }
    }
    return -1;
}

int join_room_logic(int room_id, int player_idx) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    if (rooms[room_id].player_count >= 2) return 0;
    
    if (rooms[room_id].player_count == 1) {
        rooms[room_id].player_count = 2;
        // 谁空着就坐谁的位置
        if(rooms[room_id].white_player_idx == -1) rooms[room_id].white_player_idx = player_idx;
        else rooms[room_id].black_player_idx = player_idx;
        
        rooms[room_id].black_ready = 0; // 重置准备状态
        printf("[Room] Player %s joined Room %d\n", players[player_idx].username, room_id);
        return 1;
    }
    return 0;
}