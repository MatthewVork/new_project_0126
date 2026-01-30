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
#include <netinet/in.h> // 必须包含，用于 htonl/ntohl

#include "network_client.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" 

static int sock_fd = -1;

// 设置为阻塞模式 (Blocking)
// 我们需要保证数据完整发送和接收，阻塞模式更安全
void set_blocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
}

// 初始化网络
int net_init(const char* server_ip, int port) {
    // 忽略 SIGPIPE 信号，防止服务器断开时客户端崩溃
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
    
    printf("[Net] Connected to %s:%d\n", server_ip, port);
    
    // ★★★ 关键：保持阻塞模式，但我们在接收时会用技巧防止卡UI ★★★
    set_blocking(sock_fd);
    return 0;
}

// ★★★ 核心修复：带长度头的发送函数 ★★★
// 格式：[4字节长度][JSON数据]
void send_raw_packet(const char* json_str) {
    if (sock_fd < 0 || !json_str) return;
    
    int len = strlen(json_str);
    int net_len = htonl(len); // 转换为网络字节序 (大端)
    
    // 1. 发送长度头
    int sent1 = send(sock_fd, &net_len, 4, 0);
    // 2. 发送内容
    int sent2 = send(sock_fd, json_str, len, 0);
    
    if (sent1 != 4 || sent2 != len) {
        printf("[Net] 发送异常 (Head:%d, Body:%d)\n", sent1, sent2);
        // 如果发送失败，通常意味着断开了，关闭socket
        close(sock_fd);
        sock_fd = -1;
    }
}

// --- 接收函数 (供 main.c 调用) ---
// 返回值：1=成功读到一个完整包, 0=无数据/断开
int net_recv_packet(char* out_buffer, int max_len) {
    if (sock_fd < 0) return 0;

    // 1. 尝试读取 4字节长度头
    // ★关键★：使用 MSG_DONTWAIT，如果没数据立刻返回，不卡 UI
    int net_len = 0;
    int n = recv(sock_fd, &net_len, 4, MSG_DONTWAIT);
    
    if (n <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0; // 只是暂时没数据，正常返回
        // 真的断开了
        printf("[Net] 服务器断开连接\n");
        close(sock_fd);
        sock_fd = -1;
        return 0;
    }
    
    if (n != 4) {
        // 读到了数据但不足4字节，这是一个极罕见的错误（通常意味着数据流错乱）
        // 简单起见，我们认为连接损坏
        return 0; 
    }

    // 2. 解析长度
    int data_len = ntohl(net_len);
    if (data_len <= 0 || data_len >= max_len) {
        printf("[Net] 异常包长度: %d\n", data_len);
        return 0;
    }

    // 3. ★★★ 死循环读取，直到读够 data_len 个字节 ★★★
    // 既然头已经读到了，说明后面紧跟着就是数据，这里可以用阻塞读取，速度极快
    int total_read = 0;
    while (total_read < data_len) {
        int r = recv(sock_fd, out_buffer + total_read, data_len - total_read, 0);
        if (r <= 0) {
            close(sock_fd);
            sock_fd = -1;
            return 0;
        }
        total_read += r;
    }
    
    out_buffer[data_len] = '\0'; // 添加字符串结束符
    return 1; // 成功！out_buffer 里现在是一个完整的 JSON
}

// --- 业务发送函数 (全部统一调用 send_raw_packet) ---

// 辅助：构建简单指令并发送
void send_simple_json_cmd(const char* cmd_str) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, cmd_str);
    char *str = cJSON_PrintUnformatted(root);
    if(str) { send_raw_packet(str); free(str); }
    cJSON_Delete(root); 
}

void net_send_login(const char* u, const char* p) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_LOGIN);
    cJSON_AddStringToObject(root, KEY_USERNAME, u);
    cJSON_AddStringToObject(root, KEY_PASSWORD, p);
    char *str = cJSON_PrintUnformatted(root);
    if(str) { send_raw_packet(str); free(str); }
    cJSON_Delete(root);
}

void net_send_register(const char* u, const char* p) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_REGISTER);
    cJSON_AddStringToObject(root, KEY_USERNAME, u);
    cJSON_AddStringToObject(root, KEY_PASSWORD, p);
    char *str = cJSON_PrintUnformatted(root);
    if(str) { send_raw_packet(str); free(str); }
    cJSON_Delete(root);
}

void net_send_logout() { send_simple_json_cmd(CMD_STR_LOGOUT); }
void net_send_create_room() { send_simple_json_cmd(CMD_STR_CREATE_ROOM); }

void net_send_join_room(int id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_JOIN_ROOM);
    cJSON_AddNumberToObject(root, KEY_ROOM_ID, id);
    char *str = cJSON_PrintUnformatted(root);
    if(str) { send_raw_packet(str); free(str); }
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
    if (str) { send_raw_packet(str); free(str); }
    cJSON_Delete(root);
}

// 占位函数，保持兼容性，但不再被 main.c 调用
int net_poll(uint8_t* out_pkt_buffer) { return 0; }