#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "server_data.h"
#include "../Common/game_protocol.h"

// --- 外部函数声明 (修复 Implicit declaration 报错) ---
extern int check_login(const char* user, const char* pass);
extern int check_register(const char* user, const char* pass); // 新增
extern int create_room_logic(int player_idx);
extern int join_room_logic(int room_id, int player_idx);
extern void handle_disconnect(int player_idx); // 必须声明
extern void init_rooms();

// 全局变量定义
Player players[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

void init_server_data() {
    memset(players, 0, sizeof(players));
    init_rooms();
}

int main() {
    int server_fd, new_socket, max_sd, activity, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;
    uint8_t buffer[1024];

    // 1. 网络初始化
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    init_server_data();
    printf("=== 象棋服务器启动 ===\n");
    printf("监听端口: %d\n", SERVER_PORT);

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // 添加所有客户端到 select 集合
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0)) {
            printf("Select error\n");
        }

        // A. 新连接进入
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            
            printf("新连接: IP %s, Socket %d\n", inet_ntoa(address.sin_addr), new_socket);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if(players[i].socket_fd == 0) {
                    players[i].socket_fd = new_socket;
                    players[i].state = STATE_CONNECTED;
                    break;
                }
            }
        }

        // B. 处理客户端消息
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;

            if (FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, 1024);

                if (valread == 0) {
                    // 断开连接
                    close(sd);
                    handle_disconnect(i); // 调用断线处理
                } else {
                    uint8_t cmd = buffer[0];
                    
                    // 1. 登录
                    if (cmd == CMD_LOGIN) {
                        AuthPacket *pkt = (AuthPacket*)buffer;
                        ResultPacket res;
                        res.cmd = CMD_AUTH_RESULT;
                        
                        if (check_login(pkt->username, pkt->password)) {
                            res.success = 1;
                            strcpy(res.message, "Login OK");
                            players[i].state = STATE_LOBBY;
                            strcpy(players[i].username, pkt->username);
                            printf("玩家登录成功: %s\n", pkt->username);
                        } else {
                            res.success = 0;
                            strcpy(res.message, "Wrong Password");
                        }
                        send(sd, &res, sizeof(res), 0);
                    }
                    // 2. 注册 (新增逻辑)
                    else if (cmd == CMD_REGISTER) {
                        AuthPacket *pkt = (AuthPacket*)buffer;
                        ResultPacket res;
                        res.cmd = CMD_AUTH_RESULT;
                        
                        if (check_register(pkt->username, pkt->password)) {
                            res.success = 1;
                            strcpy(res.message, "Register Success!");
                        } else {
                            res.success = 0;
                            strcpy(res.message, "Username Exists");
                        }
                        send(sd, &res, sizeof(res), 0);
                    }
                    // 3. 创建房间
                    else if (cmd == CMD_CREATE_ROOM) {
                        ResultPacket res;
                        res.cmd = CMD_ROOM_RESULT;
                        int rid = create_room_logic(i);
                        
                        if (rid != -1) {
                            res.success = 1;
                            players[i].state = STATE_IN_ROOM;
                            players[i].current_room_id = rid;
                            sprintf(res.message, "Room %d Created", rid);
                        } else {
                            res.success = 0;
                            strcpy(res.message, "Create Failed (Limit Reached)");
                        }
                        send(sd, &res, sizeof(res), 0);
                    }
                    // 4. 加入房间
                    else if (cmd == CMD_JOIN_ROOM) {
                        RoomActionPacket *req = (RoomActionPacket*)buffer;
                        ResultPacket res;
                        res.cmd = CMD_ROOM_RESULT;
                        
                        if (join_room_logic(req->room_id, i)) {
                            res.success = 1;
                            players[i].state = STATE_IN_ROOM;
                            players[i].current_room_id = req->room_id;
                            sprintf(res.message, "Joined Room %d", req->room_id);
                        } else {
                            res.success = 0;
                            strcpy(res.message, "Room Full or Not Found");
                        }
                        send(sd, &res, sizeof(res), 0);
                    }
                }
            }
        }
    }
    return 0;
}