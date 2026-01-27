#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "network_client.h"

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

// --- 发送函数 ---
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

// 新增：退出登录
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

// --- 接收函数 ---
int net_poll(uint8_t* out_pkt_buffer) {
    if (sock_fd < 0) return 0;
    int len = recv(sock_fd, out_pkt_buffer, 1024, 0);
    if (len > 0) return len;
    if (len == 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    return 0;
}

void net_send_get_room_list() {
    // 不需要带数据，只要发个命令头就行
    uint8_t cmd = CMD_GET_ROOM_LIST;
    send_raw(&cmd, 1);
    printf("[Net] 请求刷新房间列表...\n");
    printf("[Debug] UI 刷新按钮被点击了，准备发送网络包...\n");
}

void net_send_leave_room() {
    // 使用协议中定义的命令字（假设你已在 game_protocol.h 定义为 0x25）
    uint8_t cmd = CMD_LEAVE_ROOM; 
    send_raw(&cmd, 1); // 发送 1 字节的命令请求
    printf("[Net] 发送离开房间请求...\n");
}