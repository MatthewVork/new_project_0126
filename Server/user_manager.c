#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server_data.h" 

#define USER_FILE "users.txt"

// 注册功能 (保持不变)
int check_register(const char* user, const char* pass) {
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

    fp = fopen(USER_FILE, "a+");
    if (!fp) return 0;
    fprintf(fp, "%s %s\n", user, pass);
    fclose(fp);
    printf("新用户注册: %s\n", user);
    return 1;
}

// 登录功能 (升级版：防止重复登录)
// 返回值：1=成功, 0=密码错/无账号, -1=重复登录
int check_login(const char* user, const char* pass) {
    
    // 1. 先检查是否已经在线 (防止重复登录的核心！)
    for(int i=0; i<MAX_CLIENTS; i++) {
        // 如果某个玩家的状态是“在房间”或“在大厅”，且名字和当前请求的一样
        if(players[i].state > STATE_CONNECTED && strcmp(players[i].username, user) == 0) {
            printf("拒绝登录: 用户 %s 已经在线了\n", user);
            return -1; // 这是一个特殊的错误码，表示“Repeat”
        }
    }

    // 2. 查文件验证密码
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

// 退出登录处理 (新增)
void handle_logout(int player_idx) {
    if (player_idx < 0 || player_idx >= MAX_CLIENTS) return;
    
    printf("玩家 %s 退出登录 (Socket %d)\n", players[player_idx].username, players[player_idx].socket_fd);

    // 只是清除登录状态，不断开连接
    players[player_idx].state = STATE_CONNECTED; // 降级为“仅连接”
    memset(players[player_idx].username, 0, 32); // 清空名字，这样就可以再次登录了
    players[player_idx].current_room_id = -1;
}

// 断线处理 (复用逻辑)
void handle_disconnect(int player_idx) {
    if (player_idx < 0 || player_idx >= MAX_CLIENTS) return;

    printf("玩家 %s 断开连接\n", players[player_idx].username);
    
    // 这里可以加离开房间的逻辑...
    
    players[player_idx].socket_fd = 0;
    players[player_idx].state = STATE_DISCONNECTED;
    memset(players[player_idx].username, 0, 32); // 必须清空，否则断线重连会提示重复
    players[player_idx].current_room_id = -1;
}