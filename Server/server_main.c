#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "server_data.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h"

// 声明外部函数
extern int check_login(const char* u, const char* p);
extern int check_register(const char* u, const char* p);
extern int create_room_logic(int p_idx);
extern int join_room_logic(int rid, int p_idx);
extern void handle_disconnect(int idx);
extern void handle_logout(int idx);
extern void init_rooms();

Player players[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

void send_json_response(int socket_fd, cJSON *root) {
    if (socket_fd <= 0) { cJSON_Delete(root); return; }
    char *str = cJSON_PrintUnformatted(root);
    // 发送字符串 + \0 结束符
    send(socket_fd, str, strlen(str)+1, 0); 
    free(str);
    cJSON_Delete(root);
}

// 广播房间状态
void broadcast_room_update(int rid) {
    if (rid < 0 || rid >= MAX_ROOMS) return;

    cJSON *update = cJSON_CreateObject();
    cJSON_AddNumberToObject(update, "cmd", CMD_ROOM_UPDATE);
    
    int p1 = rooms[rid].white_player_idx;
    int p2 = rooms[rid].black_player_idx;
    
    cJSON_AddStringToObject(update, "p1_name", (p1!=-1) ? players[p1].username : "Empty");
    cJSON_AddBoolToObject(update, "p1_ready", rooms[rid].white_ready);
    cJSON_AddStringToObject(update, "p2_name", (p2!=-1) ? players[p2].username : "Waiting...");
    cJSON_AddBoolToObject(update, "p2_ready", rooms[rid].black_ready);

    if(p1 != -1) send_json_response(players[p1].socket_fd, cJSON_Duplicate(update, 1));
    if(p2 != -1) send_json_response(players[p2].socket_fd, update);
}

int main() {
    int server_fd, new_socket, max_sd, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;
    char buffer[4096];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt=1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(SERVER_PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);
    
    memset(players, 0, sizeof(players));
    init_rooms();
    printf("=== Chess Server Started Port: %d ===\n", SERVER_PORT);

    while(1) {
        FD_ZERO(&readfds); FD_SET(server_fd, &readfds); max_sd = server_fd;
        for(int i=0; i<MAX_CLIENTS; i++) {
            if(players[i].socket_fd > 0) FD_SET(players[i].socket_fd, &readfds);
            if(players[i].socket_fd > max_sd) max_sd = players[i].socket_fd;
        }
        select(max_sd+1, &readfds, NULL, NULL, NULL);

        if(FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            for(int i=0; i<MAX_CLIENTS; i++) {
                if(players[i].socket_fd == 0) {
                    players[i].socket_fd = new_socket; players[i].state = STATE_CONNECTED; break;
                }
            }
        }

        for(int i=0; i<MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;
            if(FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, 4096);
                if(valread == 0) { close(sd); handle_disconnect(i); }
                else {
                    buffer[valread] = '\0';
                    cJSON *json = cJSON_Parse(buffer);
                    if(!json) continue;

                    cJSON *cmd_item = cJSON_GetObjectItem(json, "cmd");
                    if(!cmd_item) { cJSON_Delete(json); continue; }
                    int cmd = cmd_item->valueint;

                    // --- 1. 登录处理 ---
                    if(cmd == CMD_LOGIN) {
                        cJSON *u = cJSON_GetObjectItem(json, "username");
                        cJSON *p = cJSON_GetObjectItem(json, "password");
                        cJSON *res = cJSON_CreateObject();
                        cJSON_AddNumberToObject(res, "cmd", CMD_AUTH_RESULT);
                        int status = 0; if(u && p) status = check_login(u->valuestring, p->valuestring);
                        cJSON_AddBoolToObject(res, "success", status==1);
                        cJSON_AddStringToObject(res, "message", status==1?"OK":(status==-1?"Repeat":"Fail"));
                        if(status==1) { players[i].state = STATE_LOBBY; strcpy(players[i].username, u->valuestring); }
                        send_json_response(sd, res);
                    }
                    // --- 2. 注册处理 (之前缺失的部分补在这里) ---
                    else if(cmd == CMD_REGISTER) {
                        cJSON *u = cJSON_GetObjectItem(json, "username");
                        cJSON *p = cJSON_GetObjectItem(json, "password");
                        cJSON *res = cJSON_CreateObject();
                        cJSON_AddNumberToObject(res, "cmd", CMD_AUTH_RESULT);

                        int status = 0; 
                        if(u && p) status = check_register(u->valuestring, p->valuestring);
                        
                        cJSON_AddBoolToObject(res, "success", status == 1);
                        cJSON_AddStringToObject(res, "message", status == 1 ? "Register Success" : "Register Fail");
                        
                        send_json_response(sd, res);
                    }
                    // --- 3. 创建房间 ---
                    else if(cmd == CMD_CREATE_ROOM) {
                        int rid = create_room_logic(i);
                        cJSON *res = cJSON_CreateObject();
                        cJSON_AddNumberToObject(res, "cmd", CMD_ROOM_RESULT);
                        cJSON_AddBoolToObject(res, "success", rid != -1);
                        if(rid != -1) {
                            players[i].state = STATE_IN_ROOM;
                            players[i].current_room_id = rid;
                            cJSON_AddNumberToObject(res, "room_id", rid);
                        }
                        send_json_response(sd, res);
                        if(rid != -1) broadcast_room_update(rid);
                    }
                    // --- 4. 加入房间 ---
                    else if(cmd == CMD_JOIN_ROOM) {
                        cJSON *item = cJSON_GetObjectItem(json, "room_id");
                        int rid = item ? item->valueint : -1;
                        int ok = join_room_logic(rid, i);
                        cJSON *res = cJSON_CreateObject();
                        cJSON_AddNumberToObject(res, "cmd", CMD_ROOM_RESULT);
                        cJSON_AddBoolToObject(res, "success", ok);
                        if(ok) {
                            players[i].state = STATE_IN_ROOM;
                            players[i].current_room_id = rid;
                        }
                        send_json_response(sd, res);
                        if(ok) broadcast_room_update(rid);
                    }
                    // --- 5. 准备 ---
                    else if(cmd == CMD_READY) {
                        int rid = players[i].current_room_id;
                        if(rid >= 0) {
                            if(rooms[rid].white_player_idx == i) rooms[rid].white_ready = 1;
                            if(rooms[rid].black_player_idx == i) rooms[rid].black_ready = 1;
                            
                            broadcast_room_update(rid);

                            // 双方就绪 -> 开始游戏
                            if(rooms[rid].player_count==2 && rooms[rid].white_ready && rooms[rid].black_ready) {
                                printf("Room %d Start!\n", rid);
                                rooms[rid].status = 1; 

                                // 红方
                                cJSON *pkg1 = cJSON_CreateObject();
                                cJSON_AddNumberToObject(pkg1, "cmd", CMD_GAME_START);
                                cJSON_AddNumberToObject(pkg1, "your_color", 0); 
                                cJSON_AddStringToObject(pkg1, "opponent_name", players[rooms[rid].black_player_idx].username);
                                send_json_response(players[rooms[rid].white_player_idx].socket_fd, pkg1);

                                // 黑方
                                cJSON *pkg2 = cJSON_CreateObject();
                                cJSON_AddNumberToObject(pkg2, "cmd", CMD_GAME_START);
                                cJSON_AddNumberToObject(pkg2, "your_color", 1); 
                                cJSON_AddStringToObject(pkg2, "opponent_name", players[rooms[rid].white_player_idx].username);
                                send_json_response(players[rooms[rid].black_player_idx].socket_fd, pkg2);
                                
                                rooms[rid].white_ready = 0; rooms[rid].black_ready = 0;
                            }
                        }
                    }
                    // --- 6. 获取房间列表 ---
                    else if(cmd == CMD_GET_ROOM_LIST) {
                        cJSON *res = cJSON_CreateObject();
                        cJSON_AddNumberToObject(res, "cmd", CMD_ROOM_LIST_RES);
                        cJSON *arr = cJSON_CreateArray();
                        for(int r=0; r<MAX_ROOMS; r++) {
                            if(rooms[r].player_count > 0) {
                                cJSON *item = cJSON_CreateObject();
                                cJSON_AddNumberToObject(item, "id", rooms[r].room_id);
                                cJSON_AddNumberToObject(item, "players", rooms[r].player_count);
                                cJSON_AddNumberToObject(item, "status", rooms[r].status);
                                cJSON_AddItemToArray(arr, item);
                            }
                        }
                        cJSON_AddItemToObject(res, "rooms", arr);
                        send_json_response(sd, res);
                    }
                    // --- 7. 离开房间/退出游戏 ---
                    else if(cmd == CMD_LEAVE_ROOM) {
                         int rid = players[i].current_room_id;
                         if(rid >=0) {
                             rooms[rid].player_count--;
                             if(rooms[rid].white_player_idx == i) { rooms[rid].white_player_idx = -1; rooms[rid].white_ready=0; }
                             if(rooms[rid].black_player_idx == i) { rooms[rid].black_player_idx = -1; rooms[rid].black_ready=0; }
                             
                             players[i].state = STATE_LOBBY;
                             players[i].current_room_id = -1;
                             
                             if(rooms[rid].player_count == 0) rooms[rid].status = 0; 
                             else broadcast_room_update(rid);
                         }
                    }
                    cJSON_Delete(json);
                }
            }
        }
    }
    return 0;
}