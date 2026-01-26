#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>      // 用于设置非阻塞
#include <errno.h>

#include "network_client.h"

static int sock_fd = -1;

// 辅助：设置 Socket 为非阻塞模式
void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

// 辅助：通用发送函数
void send_raw(void* data, size_t len) {
    if (sock_fd < 0) return;
    send(sock_fd, data, len, 0);
}

// 初始化连接
int net_init(const char* server_ip, int port) {
    struct sockaddr_in serv_addr;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[Net] Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // 将 IP 字符串转换为二进制
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("[Net] Invalid address\n");
        return -1;
    }

    // 连接服务器 (这一步通常是阻塞的)
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[Net] Connection Failed\n");
        return -1;
    }

    printf("[Net] Connected to Server!\n");
    
    // 连接成功后，设为非阻塞模式，防止 recv 卡死界面
    set_nonblocking(sock_fd);
    
    return 0;
}

// --- 发送实现 ---

void net_send_login(const char* username, const char* password) {
    AuthPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_LOGIN;
    strncpy(pkt.username, username, 31);
    strncpy(pkt.password, password, 31);
    send_raw(&pkt, sizeof(pkt));
    printf("[Net] Sent Login: %s\n", username);
}

// 新增：发送注册
void net_send_register(const char* username, const char* password) {
    AuthPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_REGISTER; // 必须是 0x11
    strncpy(pkt.username, username, 31);
    strncpy(pkt.password, password, 31);
    send_raw(&pkt, sizeof(pkt));
    printf("[Net] Sent Register: %s\n", username);
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

// --- 接收实现 ---

int net_poll(uint8_t* out_pkt_buffer) {
    if (sock_fd < 0) return 0;

    // 非阻塞读取
    int len = recv(sock_fd, out_pkt_buffer, 1024, 0);

    if (len > 0) {
        return len; // 有数据
    } else if (len == 0) {
        printf("[Net] Server disconnected\n");
        close(sock_fd);
        sock_fd = -1;
    } else {
        // len < 0，检查是不是没数据
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0; // 正常情况，暂时没数据
        }
        // 其他错误可以打印 perror("recv");
    }
    return 0;
}