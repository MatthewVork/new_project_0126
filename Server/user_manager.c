#include <string.h>
#include "server_data.h"

int check_login(const char* user, const char* pass) {
    // 简单验证：密码是123就算过，且不能重复登录
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(players[i].socket_fd > 0 && strcmp(players[i].username, user) == 0) return -1; 
    }
    if (strcmp(pass, "123") == 0) return 1;
    return 0;
}

int check_register(const char* user, const char* pass) {
    return 1; // 总是注册成功
}

void handle_disconnect(int idx) {
    if(players[idx].socket_fd > 0) {
        int rid = players[idx].current_room_id;
        // 如果在房间里，需要清理房间数据
        if (rid != -1 && rooms[rid].player_count > 0) {
            if (rooms[rid].white_player_idx == idx) { rooms[rid].white_player_idx = -1; rooms[rid].white_ready = 0; }
            if (rooms[rid].black_player_idx == idx) { rooms[rid].black_player_idx = -1; rooms[rid].black_ready = 0; }
            rooms[rid].player_count--;
            rooms[rid].status = 0; // 只要有人掉线，游戏就强制结束或暂停
        }
        players[idx].socket_fd = 0;
        players[idx].state = 0;
        players[idx].current_room_id = -1;
        printf("Player %d disconnected\n", idx);
    }
}
void handle_logout(int idx) {
    handle_disconnect(idx); // 简单处理：登出等于断线清理
}