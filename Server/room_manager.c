#include <stdio.h>
#include "server_data.h"

// 初始化房间数据
void init_rooms() {
    for(int i=0; i<MAX_ROOMS; i++) {
        rooms[i].id = 100 + i; // 房间号从100开始
        rooms[i].is_active = 0;
        rooms[i].player1_idx = -1;
        rooms[i].player2_idx = -1;
        rooms[i].game_started = 0;
    }
}

// 创建房间，返回房间ID，失败返回-1
int create_room_logic(int player_idx) {
    for(int i=0; i<MAX_ROOMS; i++) {
        if (!rooms[i].is_active) {
            rooms[i].is_active = 1;
            rooms[i].player1_idx = player_idx;
            rooms[i].player2_idx = -1;
            rooms[i].game_started = 0;
            return rooms[i].id;
        }
    }
    return -1;
}

// 加入房间
int join_room_logic(int room_id, int player_idx) {
    for(int i=0; i<MAX_ROOMS; i++) {
        if (rooms[i].is_active && rooms[i].id == room_id) {
            if (rooms[i].player2_idx == -1) {
                rooms[i].player2_idx = player_idx;
                return 1; // 加入成功
            }
        }
    }
    return 0; // 满员或不存在
}