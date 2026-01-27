#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server_data.h" 

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

// 登录功能 (含重复登录检查)
// 返回值: 1=成功, 0=失败, -1=重复登录
int check_login(const char* user, const char* pass) {
    // 1. 检查是否已经在线
    for(int i=0; i<MAX_CLIENTS; i++) {
        // 如果玩家状态不是断开，且名字相同
        if(players[i].state > STATE_DISCONNECTED && players[i].socket_fd > 0) {
            if(strcmp(players[i].username, user) == 0) {
                printf("拒绝登录: 用户 %s 已经在线 (Slot %d)\n", user, i);
                return -1; // 重复登录
            }
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
    
    // --- 关键联动：退出房间 ---
    int rid = players[player_idx].current_room_id; // 获取当前所在房间
    if (rid != -1) {
        leave_room_logic(rid, player_idx); // 先退房，再销户
    }
    
    printf("玩家 %s 退出登录\n", players[player_idx].username);
    
    // 清除用户信息，但保持连接（Socket不断）
    players[player_idx].state = STATE_CONNECTED;
    players[player_idx].current_room_id = -1;
    memset(players[player_idx].username, 0, 32); // 关键：清空名字
}

// 断线处理
void handle_disconnect(int player_idx) {
    if (player_idx < 0 || player_idx >= MAX_CLIENTS) return;

    // --- 关键联动：退出房间 ---
    int rid = players[player_idx].current_room_id;
    if (rid != -1) {
        leave_room_logic(rid, player_idx);
    }
    
    printf("玩家 (Slot %d) %s 断开连接\n", player_idx, players[player_idx].username);
    
    // 清理该玩家的所有状态
    if (players[player_idx].socket_fd > 0) {
        players[player_idx].socket_fd = 0;
    }
    players[player_idx].state = STATE_DISCONNECTED;
    players[player_idx].current_room_id = -1;
    memset(players[player_idx].username, 0, 32);
}