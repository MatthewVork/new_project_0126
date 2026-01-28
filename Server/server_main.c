#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "server_data.h"
#include "../Common/game_protocol.h"

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

    // 网络初始化
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
    printf("=== 围棋服务器启动 (Port: %d) ===\n", SERVER_PORT);

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0)) printf("Select error\n");

        // 1. 处理新连接
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("新连接: %s, Socket %d\n", inet_ntoa(address.sin_addr), new_socket);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if(players[i].socket_fd == 0) {
                    players[i].socket_fd = new_socket;
                    players[i].state = STATE_CONNECTED;
                    break;
                }
            }
        }

        // 2. 处理客户端消息
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;

            if (FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, 1024);

                if (valread == 0) {
                    close(sd);
                    handle_disconnect(i);
                } else {
                    uint8_t cmd = buffer[0];
                    
                    // --- 登录 ---
                    if (cmd == CMD_LOGIN) {
                        AuthPacket *pkt = (AuthPacket*)buffer;
                        ResultPacket res;
                        res.cmd = CMD_AUTH_RESULT;
                        
                        int status = check_login(pkt->username, pkt->password);
                        
                        if (status == 1) {
                            res.success = 1;
                            strcpy(res.message, "Login OK");
                            players[i].state = STATE_LOBBY;
                            strcpy(players[i].username, pkt->username);
                            printf("登录成功: %s\n", pkt->username);
                        } else if (status == -1) {
                            res.success = 0;
                            strcpy(res.message, "Login Repeat!"); 
                        } else {
                            res.success = 0;
                            strcpy(res.message, "Wrong Password");
                        }
                        send(sd, &res, sizeof(res), 0);
                    }
                    // --- 注册 ---
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
                    // --- 登出 ---
                    else if (cmd == CMD_LOGOUT) { 
                        handle_logout(i);
                    }
                    // --- 创建房间 ---
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
                            strcpy(res.message, "Create Failed");
                        }
                        send(sd, &res, sizeof(res), 0);

                        // 3. ★ 核心修复：延时 50ms 后再发“更新包”
                        // 这样客户端有时间先处理跳转，且避免粘包
                        if (rid != -1) {
                            usleep(50000); // 等待 50ms
                            broadcast_room_info(rid);
                        }
                        
                    }
                    
                    // --- 加入房间 ---
                    else if (cmd == CMD_JOIN_ROOM) {
                        RoomActionPacket *req = (RoomActionPacket*)buffer;
                        ResultPacket res;
                        res.cmd = CMD_ROOM_RESULT;

                        if (join_room_logic(req->room_id, i)) {
                            // === 成功逻辑 ===
                            res.success = 1;
                            players[i].state = STATE_IN_ROOM;
                            players[i].current_room_id = req->room_id;
                            sprintf(res.message, "Joined Room %d", req->room_id);
                            
                            // 1. 发送成功结果
                            send(sd, &res, sizeof(res), 0);

                            // 2. 广播房间信息 (让大家看到名字)
                            usleep(50000); 
                            broadcast_room_info(req->room_id);

                            // ★★★ 关键操作：这里原来那段“满员自动开始”的代码必须删掉！ ★★★
                            // 删掉下面这几行：
                            /*
                            if (rooms[req->room_id].player_count == 2) {
                                usleep(20000); 
                                broadcast_game_start(req->room_id); // <--- 就是这行导致一进房就开赛，必须删掉
                            }
                            */
                            
                        } else {
                            // === 失败逻辑 ===
                            res.success = 0;
                            strcpy(res.message, "Room Full or Not Found");
                            send(sd, &res, sizeof(res), 0);
                        }
                    }

                    // --- 离开房间 ---
                    else if (cmd == CMD_LEAVE_ROOM) {
                        int rid = players[i].current_room_id;
                        if (rid != -1) {
                            if (leave_room_logic(rid, i)) { 
                                players[i].state = STATE_LOBBY;
                                players[i].current_room_id = -1;
                                ResultPacket res;
                                res.cmd = CMD_ROOM_RESULT;
                                res.success = 1;
                                strcpy(res.message, "Left Room Success");
                                send(sd, &res, sizeof(res), 0);
                            }
                        }
                    }
                    // --- 刷新列表 ---
                    else if (cmd == CMD_GET_ROOM_LIST) {
                        uint8_t res_pkt[1024];
                        memset(res_pkt, 0, sizeof(res_pkt));
                        res_pkt[0] = CMD_ROOM_LIST_RES; 
                        
                        uint8_t active_count = 0; 
                        RoomInfo *info_array = (RoomInfo *)&res_pkt[2]; 

                        for (int j = 0; j < MAX_ROOMS; j++) {
                            if (rooms[j].player_count > 0) {
                                info_array[active_count].room_id      = rooms[j].room_id;
                                info_array[active_count].player_count = (uint8_t)rooms[j].player_count;
                                info_array[active_count].status       = (uint8_t)rooms[j].status;
                                active_count++;
                            }
                        }
                        res_pkt[1] = active_count;
                        int total_len = 2 + (active_count * sizeof(RoomInfo));
                        send(sd, res_pkt, total_len, 0);
                    }
                    // --- 准备/取消准备 ---
                    else if (cmd == CMD_READY || cmd == CMD_CANCEL_READY) {
                        int rid = players[i].current_room_id;
                        if (rid != -1) {
                            // 1. 更新内存状态
                            handle_ready_toggle(rid, i, (cmd == CMD_READY));
                            
                            // 2. 先判断是否可以开始游戏
                            int can_start = 0;
                            if (rooms[rid].player_count == 2 && rooms[rid].white_ready && rooms[rid].black_ready) {
                                can_start = 1;
                            }

                            if (can_start) {
                                // ★★★ 方案 B (绝杀): 如果能开始，就只发“开始信号”！ ★★★
                                // 既然游戏都要开始了，那个“变绿”的状态包就别发了，防止粘包卡住客户端
                                printf("[Server] 条件满足，直接发送开始指令！\n");
                                rooms[rid].status = 2; // 设为游戏中
                                
                                // 稍微延时一点点即可
                                usleep(50000); 
                                broadcast_game_start(rid); 
                            } 
                            else {
                                // ★★★ 只有不能开始时，才广播“变绿/变黄”的状态 ★★★
                                // 这样平时点准备，别人能看到变绿；最后一次点准备，直接进游戏
                                broadcast_room_info(rid);
                                printf("[Server] 等待其他玩家，广播状态更新...\n");
                            }
                        }
                    }
                    // ★★★ 新增：处理落子 ★★★
                    else if (cmd == CMD_PLACE_STONE) {
                        StonePacket *pkt = (StonePacket*)buffer;
                        int rid = players[i].current_room_id;
                        if (rid != -1) {
                            // 转交给 room_manager.c 的逻辑函数
                            handle_place_stone(rid, i, pkt->x, pkt->y);
                        }
                    }
                }
            }
        }
    }
    return 0;
}