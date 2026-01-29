#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "network_client.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" // ★★★ 必须引入 cJSON ★★★

static int sock_fd = -1;

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

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

// --- 旧的二进制发送函数 (保持不变) ---
void net_send_login(const char* username, const char* password) {
    AuthPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_LOGIN;
    strncpy(pkt.username, username, 31);
    strncpy(pkt.password, password, 31);
    send_raw(&pkt, sizeof(pkt));
    printf("[Net] Sent Login: %s\n", username);
}

void net_send_register(const char* username, const char* password) {
    AuthPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_REGISTER;
    strncpy(pkt.username, username, 31);
    strncpy(pkt.password, password, 31);
    send_raw(&pkt, sizeof(pkt));
    printf("[Net] Sent Register: %s\n", username);
}

void net_send_logout() {
    uint8_t cmd = CMD_LOGOUT;
    send_raw(&cmd, 1);
    printf("[Net] Sent Logout\n");
}

void net_send_create_room() {
    RoomActionPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_CREATE_ROOM;
    pkt.room_id = -1;
    send_raw(&pkt, sizeof(pkt));
}

void net_send_join_room(int room_id) {
    RoomActionPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_JOIN_ROOM;
    pkt.room_id = room_id;
    send_raw(&pkt, sizeof(pkt));
}

void net_send_get_room_list() {
    uint8_t cmd = CMD_GET_ROOM_LIST;
    send_raw(&cmd, 1);
    printf("[Net] 请求刷新房间列表...\n");
}

void net_send_leave_room() {
    uint8_t cmd = CMD_LEAVE_ROOM; 
    send_raw(&cmd, 1); 
    printf("[Net] 发送离开房间请求...\n");
}

// ★★★ 新增：发送落子指令 (JSON版) ★★★
void net_send_place_stone_json(int x, int y, int color) {
    // 1. 创建 JSON 对象
    cJSON *root = cJSON_CreateObject();
    
    // 2. 添加字段
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_PLACE_STONE); // "cmd": "place_stone"
    cJSON_AddNumberToObject(root, KEY_X, x);
    cJSON_AddNumberToObject(root, KEY_Y, y);
    cJSON_AddNumberToObject(root, KEY_COLOR, color);

    // 3. 转换成字符串 (例如: {"cmd":"place_stone","x":3,"y":4,"color":0})
    char *json_str = cJSON_PrintUnformatted(root);
    
    // 4. 发送字符串
    if (json_str) {
        printf("[Net-JSON] 发送落子: %s\n", json_str);
        send_raw(json_str, strlen(json_str));
        free(json_str); // 记得释放字符串内存
    }

    // 5. 销毁对象
    cJSON_Delete(root);
}

// --- 接收函数 ---
int net_poll(uint8_t* out_pkt_buffer) {
    if (sock_fd < 0) return 0;
    
    // 这里为了演示简单，直接读。在真实复杂场景下 JSON 最好用 \n 分隔。
    int len = recv(sock_fd, out_pkt_buffer, 1023, 0);
    
    if (len > 0) {
        // ★ 关键：手动添加字符串结束符，防止 cJSON 解析越界
        if (len < 1024) out_pkt_buffer[len] = '\0'; 
        return len;
    }
    if (len == 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    return 0;
}