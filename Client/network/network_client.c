#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>

#include "network_client.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" 

static int sock_fd = -1;

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

// 原始发送函数（无包头）
void send_raw(void* data, size_t len) {
    if (sock_fd < 0) return;
    #ifdef MSG_NOSIGNAL
        send(sock_fd, data, len, MSG_NOSIGNAL);
    #else
        send(sock_fd, data, len, 0);
    #endif
}

int net_init(const char* server_ip, int port) {
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
    set_nonblocking(sock_fd); // 设为非阻塞，配合 net_poll
    return 0;
}

// ★★★ 复活：真实的非阻塞接收函数 ★★★
// 之前版本这里是 return 0，导致无法接收任何数据（包括登录回复）
int net_poll(uint8_t* out_pkt_buffer) {
    if (sock_fd < 0) return 0;
    
    // 尝试读取数据
    int len = recv(sock_fd, out_pkt_buffer, 1023, 0);
    
    if (len > 0) {
        out_pkt_buffer[len] = '\0'; 
        return len;
    }
    
    if (len == 0) {
        // 服务器断开
        close(sock_fd);
        sock_fd = -1;
    }
    
    return 0; // len < 0 (EAGAIN) 或 len == 0 均返回0
}

// --- 业务发送函数 (保持纯文本JSON发送) ---
void send_simple_json_cmd(const char* cmd_str) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, cmd_str);
    char *str = cJSON_PrintUnformatted(root);
    if(str) { send_raw(str, strlen(str)); free(str); }
    cJSON_Delete(root); 
}
void net_send_login(const char* u, const char* p) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_LOGIN);
    cJSON_AddStringToObject(root, KEY_USERNAME, u);
    cJSON_AddStringToObject(root, KEY_PASSWORD, p);
    char *str = cJSON_PrintUnformatted(root);
    if(str) { send_raw(str, strlen(str)); free(str); }
    cJSON_Delete(root);
}
void net_send_register(const char* u, const char* p) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_REGISTER);
    cJSON_AddStringToObject(root, KEY_USERNAME, u);
    cJSON_AddStringToObject(root, KEY_PASSWORD, p);
    char *str = cJSON_PrintUnformatted(root);
    if(str) { send_raw(str, strlen(str)); free(str); }
    cJSON_Delete(root);
}
void net_send_logout() { send_simple_json_cmd(CMD_STR_LOGOUT); }
void net_send_create_room() { send_simple_json_cmd(CMD_STR_CREATE_ROOM); }
void net_send_join_room(int id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_JOIN_ROOM);
    cJSON_AddNumberToObject(root, KEY_ROOM_ID, id);
    char *str = cJSON_PrintUnformatted(root);
    if(str) { send_raw(str, strlen(str)); free(str); }
    cJSON_Delete(root);
}
void net_send_get_room_list() { send_simple_json_cmd(CMD_STR_GET_ROOM_LIST); }
void net_send_leave_room() { send_simple_json_cmd(CMD_STR_LEAVE_ROOM); }
void net_send_ready_action(bool is_ready) {
    if (is_ready) send_simple_json_cmd(CMD_STR_READY);
    else send_simple_json_cmd(CMD_STR_CANCEL_READY);
}
void net_send_place_stone_json(int x, int y, int color) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_PLACE_STONE);
    cJSON_AddNumberToObject(root, KEY_X, x);
    cJSON_AddNumberToObject(root, KEY_Y, y);
    cJSON_AddNumberToObject(root, KEY_COLOR, color);
    char *str = cJSON_PrintUnformatted(root);
    if (str) { send_raw(str, strlen(str)); free(str); }
    cJSON_Delete(root);
}