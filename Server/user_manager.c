#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server_data.h" // 必须包含这个，因为要访问 players 数组

#define USER_FILE "users.txt"

// 注册功能
int check_register(const char* user, const char* pass) {
    // 1. 查重
    FILE *fp = fopen(USER_FILE, "r");
    if (fp) {
        char u[32], p[32];
        while(fscanf(fp, "%s %s", u, p) != EOF) {
            if(strcmp(u, user) == 0) {
                fclose(fp);
                return 0; // 用户名已存在
            }
        }
        fclose(fp);
    }

    // 2. 写入
    fp = fopen(USER_FILE, "a+");
    if (!fp) return 0;

    fprintf(fp, "%s %s\n", user, pass);
    fclose(fp);
    
    printf("新用户注册成功: %s\n", user);
    return 1;
}

// 登录功能
int check_login(const char* user, const char* pass) {
    FILE *fp = fopen(USER_FILE, "r");
    if (!fp) return 0;

    char u[32], p[32];
    int found = 0;
    while(fscanf(fp, "%s %s", u, p) != EOF) {
        if(strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

// 断线处理功能 (之前报错就是缺这个)
void handle_disconnect(int player_idx) {
    if (player_idx < 0 || player_idx >= MAX_CLIENTS) return;
    
    printf("玩家 (Slot %d) 断开连接\n", player_idx);
    
    // 清理该玩家的状态
    if (players[player_idx].socket_fd > 0) {
        // socket 已经在 main 里面 close 过了，这里只需要清零数据
        players[player_idx].socket_fd = 0;
    }
    players[player_idx].state = STATE_DISCONNECTED;
    players[player_idx].current_room_id = -1;
    memset(players[player_idx].username, 0, 32);
}