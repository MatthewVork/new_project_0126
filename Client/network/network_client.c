#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#include "network_client.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" // 必须引入 cJSON

static int sock_fd = -1;

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

// 基础发送函数：负责把字符串发给服务器
void send_raw(void* data, size_t len) {
    if (sock_fd < 0) return;
    send(sock_fd, data, len, 0);
}

int net_init(const char* server_ip, int port) {
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

// --- 内部辅助函数：快速发送只有 cmd 的简单指令 ---
void send_simple_json_cmd(const char* cmd_str) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, cmd_str);
    
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        printf("[Net] 发送指令: %s\n", str);
        send_raw(str, strlen(str));
        free(str); // 必须释放！
    }
    cJSON_Delete(root); // 必须释放！
}

// =========================================================
// ★★★ 以下所有发送函数均已改为 cJSON 格式 ★★★
// =========================================================

// 1. 登录
void net_send_login(const char* username, const char* password) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_LOGIN);
    cJSON_AddStringToObject(root, KEY_USERNAME, username);
    cJSON_AddStringToObject(root, KEY_PASSWORD, password);
    
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        printf("[Net] 发送登录请求: %s\n", username);
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
        printf("[Net] 发送注册请求: %s\n", username);
        send_raw(str, strlen(str));
        free(str);
    }
    cJSON_Delete(root);
}

// 3. 登出
void net_send_logout() {
    send_simple_json_cmd(CMD_STR_LOGOUT);
}

// 4. 创建房间
void net_send_create_room() {
    send_simple_json_cmd(CMD_STR_CREATE_ROOM);
}

// 5. 加入房间
void net_send_join_room(int room_id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_JOIN_ROOM);
    cJSON_AddNumberToObject(root, KEY_ROOM_ID, room_id);

    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        printf("[Net] 申请加入房间: %d\n", room_id);
        send_raw(str, strlen(str));
        free(str);
    }
    cJSON_Delete(root);
}

// 6. 获取房间列表
void net_send_get_room_list() {
    send_simple_json_cmd(CMD_STR_GET_ROOM_LIST);
}

// 7. 离开房间
void net_send_leave_room() {
    send_simple_json_cmd(CMD_STR_LEAVE_ROOM);
}

// 8. 准备 / 取消准备
void net_send_ready_action(bool is_ready) {
    if (is_ready) send_simple_json_cmd(CMD_STR_READY);
    else send_simple_json_cmd(CMD_STR_CANCEL_READY);
}

// 9. 落子 (之前已经改好了，保持不变)
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

// =========================================================
// ★★★ 接收函数 (严格防止溢出) ★★★
// =========================================================
int net_poll(uint8_t* out_pkt_buffer) {
    if (sock_fd < 0) return 0;
    
    // ★★★ 这里的 1023 绝对不能改回 1024 ★★★
    // 必须预留 1 个字节给 '\0'
    int len = recv(sock_fd, out_pkt_buffer, 1023, 0);
    
    if (len > 0) {
        out_pkt_buffer[len] = '\0'; // 安全封口，变成合法字符串
        return len;
    }
    if (len == 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    return 0;
}