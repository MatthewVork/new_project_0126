#include "network_client.h"
#include "../../Common/cJSON.h"
#include "../../Common/game_protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

static int sock_fd = -1;

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

void send_json_packet(cJSON *root) {
    if (sock_fd < 0) { cJSON_Delete(root); return; }
    char *str = cJSON_PrintUnformatted(root);
    send(sock_fd, str, strlen(str), 0);
    printf("[Net] Sent: %s\n", str);
    free(str);
    cJSON_Delete(root);
}

int net_init(const char* ip, int port) {
    struct sockaddr_in serv;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    serv.sin_family = AF_INET; serv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv.sin_addr);
    if (connect(sock_fd, (struct sockaddr *)&serv, sizeof(serv)) < 0) return -1;
    set_nonblocking(sock_fd);
    return 0;
}

int net_poll(uint8_t* buf) {
    if (sock_fd < 0) return 0;
    int len = recv(sock_fd, buf, 4096, 0);
    if(len > 0) buf[len] = '\0';
    return len;
}

// 业务发送函数
void net_send_login(const char* u, const char* p) {
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "cmd", CMD_LOGIN);
    cJSON_AddStringToObject(r, "username", u); cJSON_AddStringToObject(r, "password", p);
    send_json_packet(r);
}
void net_send_register(const char* u, const char* p) {
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "cmd", CMD_REGISTER);
    cJSON_AddStringToObject(r, "username", u); cJSON_AddStringToObject(r, "password", p);
    send_json_packet(r);
}
void net_send_logout() {
    cJSON *r = cJSON_CreateObject(); cJSON_AddNumberToObject(r, "cmd", CMD_LOGOUT); send_json_packet(r);
}
void net_send_create_room() {
    cJSON *r = cJSON_CreateObject(); cJSON_AddNumberToObject(r, "cmd", CMD_CREATE_ROOM); send_json_packet(r);
}
void net_send_join_room(int rid) {
    cJSON *r = cJSON_CreateObject(); cJSON_AddNumberToObject(r, "cmd", CMD_JOIN_ROOM);
    cJSON_AddNumberToObject(r, "room_id", rid); send_json_packet(r);
}
void net_send_get_room_list() {
    cJSON *r = cJSON_CreateObject(); cJSON_AddNumberToObject(r, "cmd", CMD_GET_ROOM_LIST); send_json_packet(r);
}
void net_send_ready() {
    cJSON *r = cJSON_CreateObject(); cJSON_AddNumberToObject(r, "cmd", CMD_READY); send_json_packet(r);
}
void net_send_leave_room() {
    cJSON *r = cJSON_CreateObject(); cJSON_AddNumberToObject(r, "cmd", CMD_LEAVE_ROOM); send_json_packet(r);
}