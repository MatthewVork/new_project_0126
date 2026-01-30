#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h> // ★ 新增：用于处理信号

#include "network_client.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" 

static int sock_fd = -1;

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

// 基础发送函数
void send_raw(void* data, size_t len) {
    if (sock_fd < 0) return;
    // ★ 修复：使用 MSG_NOSIGNAL 防止服务器断开时客户端崩溃 (Linux/Mac通用)
    // 如果编译器报错 MSG_NOSIGNAL 未定义，可以改回 0，依靠 net_init 里的 signal 忽略
    #ifdef MSG_NOSIGNAL
        send(sock_fd, data, len, MSG_NOSIGNAL);
    #else
        send(sock_fd, data, len, 0);
    #endif
}

int net_init(const char* server_ip, int port) {
    // ★ 修复：全局忽略 SIGPIPE 信号，防止“断开连接”导致程序闪退
    signal(SIGPIPE, SIG_IGN);

    struct sockaddr_in serv_addr;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) return -1;

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[Net] Connect Failed\n");
        return -1;
    }
    printf("[Net] Connected!\n");
    set_nonblocking(sock_fd);
    return 0;
}

// --- 简单指令发送 ---
void send_simple_json_cmd(const char* cmd_str) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, cmd_str);
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        // printf("[Net] 发送指令: %s\n", str); // 调试时可以打开
        send_raw(str, strlen(str));
        free(str); 
    }
    cJSON_Delete(root); 
}

// 1. 登录
void net_send_login(const char* username, const char* password) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_LOGIN);
    cJSON_AddStringToObject(root, KEY_USERNAME, username);
    cJSON_AddStringToObject(root, KEY_PASSWORD, password);
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        printf("[Net] 发送登录: %s\n", username);
        send_raw(str, strlen(str));
        free(str);
    }
    cJSON_Delete(root);
}

// 2. 注册
void net_send_register(const char* username, const char* password) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_REGISTER);
    cJSON_AddStringToObject(root, KEY_USERNAME, username);
    cJSON_AddStringToObject(root, KEY_PASSWORD, password);
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        printf("[Net] 发送注册: %s\n", username);
        send_raw(str, strlen(str));
        free(str);
    }
    cJSON_Delete(root);
}

// 3. 登出
void net_send_logout() { send_simple_json_cmd(CMD_STR_LOGOUT); }
// 4. 创建房间
void net_send_create_room() { send_simple_json_cmd(CMD_STR_CREATE_ROOM); }

// 5. 加入房间
void net_send_join_room(int room_id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_JOIN_ROOM);
    cJSON_AddNumberToObject(root, KEY_ROOM_ID, room_id);
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        printf("[Net] 加入房间: %d\n", room_id);
        send_raw(str, strlen(str));
        free(str);
    }
    cJSON_Delete(root);
}

// 6. 获取列表
void net_send_get_room_list() { send_simple_json_cmd(CMD_STR_GET_ROOM_LIST); }
// 7. 离开房间
void net_send_leave_room() { send_simple_json_cmd(CMD_STR_LEAVE_ROOM); }

// 8. 准备
void net_send_ready_action(bool is_ready) {
    if (is_ready) send_simple_json_cmd(CMD_STR_READY);
    else send_simple_json_cmd(CMD_STR_CANCEL_READY);
}

// 9. 落子
void net_send_place_stone_json(int x, int y, int color) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_PLACE_STONE);
    cJSON_AddNumberToObject(root, KEY_X, x);
    cJSON_AddNumberToObject(root, KEY_Y, y);
    cJSON_AddNumberToObject(root, KEY_COLOR, color);
    char *str = cJSON_PrintUnformatted(root);
    if (str) {
        send_raw(str, strlen(str));
        free(str);
    }
    cJSON_Delete(root);
}

// 接收函数
int net_poll(uint8_t* out_pkt_buffer) {
    if (sock_fd < 0) return 0;
    
    // 1023 限制，预留 \0
    int len = recv(sock_fd, out_pkt_buffer, 1023, 0);
    
    // ★ 修复：明确判断 > 0，防止 -1 错误
    if (len > 0) {
        out_pkt_buffer[len] = '\0'; 
        return len;
    }
    
    // 如果 len == 0 (断开) 或 len == -1 (错误)，且不是 EWOULDBLOCK
    if (len == 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    return 0;
}