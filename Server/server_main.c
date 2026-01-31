#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include "server_data.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" 

// --- 全局数据 ---
Player players[MAX_CLIENTS];    // 全局数组：存放所有在线玩家信息
Room rooms[MAX_ROOMS];      // 全局数组：存放所有房间信息

// 互斥锁，用于保护 players 和 rooms 数组
pthread_mutex_t g_logic_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_server_data()     //初始化在线玩家和房间数据
{
    memset(players, 0, sizeof(players));
    init_rooms();
}

// 发送辅助函数
void send_json_result(int fd, const char* cmd, int success, const char* msg) {
    if (fd <= 0) return;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, cmd);
    cJSON_AddNumberToObject(root, KEY_SUCCESS, success);
    cJSON_AddStringToObject(root, KEY_MESSAGE, msg);
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        send(fd, str, strlen(str), 0); // 在多线程中，对不同socket的send是安全的
        free(str);
    }
    cJSON_Delete(root);
}

// --- 业务逻辑分发 ---
// 这个函数现在运行在子线程中，所以操作全局数据(players/rooms)必须加锁
void process_server_json(int client_idx, cJSON *json) {
    
    // ★★★ 上锁：进入临界区 ★★★
    pthread_mutex_lock(&g_logic_mutex);

    // 再次检查 socket 是否有效 (防止在等待锁的时候连接断开了)
    if (players[client_idx].socket_fd <= 0) {
        pthread_mutex_unlock(&g_logic_mutex);
        return;
    }

    int sd = players[client_idx].socket_fd;
    cJSON *cmdItem = cJSON_GetObjectItem(json, KEY_CMD);
    if (!cmdItem || !cJSON_IsString(cmdItem)) {
        pthread_mutex_unlock(&g_logic_mutex);
        return;
    }
    char *cmd = cmdItem->valuestring;

    // --- 以下逻辑保持原样，没有任何修改 ---

    if (strcmp(cmd, CMD_STR_LOGIN) == 0) {
        cJSON *u = cJSON_GetObjectItem(json, KEY_USERNAME);
        cJSON *p = cJSON_GetObjectItem(json, KEY_PASSWORD);
        if (u && p) {
            int status = check_login(u->valuestring, p->valuestring);
            if (status == 1) {
                players[client_idx].state = STATE_LOBBY;
                strncpy(players[client_idx].username, u->valuestring, 31);
                send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Login OK");
                printf("[Thread] User %s Logged in (Index: %d)\n", players[client_idx].username, client_idx);
            } else if (status == -1) send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Already Logged In");
            else send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Wrong Password/User");
        }
    }
    else if (strcmp(cmd, CMD_STR_REGISTER) == 0) {
        cJSON *u = cJSON_GetObjectItem(json, KEY_USERNAME);
        cJSON *p = cJSON_GetObjectItem(json, KEY_PASSWORD);
        if (u && p) {
            if (check_register(u->valuestring, p->valuestring)) send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Register Success");
            else send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Username Exists");
        }
    }
    else if (strcmp(cmd, CMD_STR_LOGOUT) == 0) handle_logout(client_idx);
    else if (strcmp(cmd, CMD_STR_CREATE_ROOM) == 0) {
        int rid = create_room_logic(client_idx);
        if (rid != -1) {
            players[client_idx].state = STATE_IN_ROOM;
            players[client_idx].current_room_id = rid;
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_RESULT);
            cJSON_AddNumberToObject(res, KEY_SUCCESS, 1);
            cJSON_AddStringToObject(res, KEY_MESSAGE, "Room Created");
            cJSON_AddNumberToObject(res, KEY_ROOM_ID, rid);
            char *s = cJSON_PrintUnformatted(res);
            send(sd, s, strlen(s), 0);
            free(s);
            cJSON_Delete(res);
            usleep(50000);
            broadcast_room_info(rid);
        } else send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Create Failed");
    }
    else if (strcmp(cmd, CMD_STR_JOIN_ROOM) == 0) {
        cJSON *ridItem = cJSON_GetObjectItem(json, KEY_ROOM_ID);
        if (ridItem) {
            int rid = ridItem->valueint;
            if (join_room_logic(rid, client_idx)) {
                players[client_idx].state = STATE_IN_ROOM;
                players[client_idx].current_room_id = rid;
                cJSON *res = cJSON_CreateObject();
                cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_RESULT);
                cJSON_AddNumberToObject(res, KEY_SUCCESS, 1);
                cJSON_AddStringToObject(res, KEY_MESSAGE, "Joined Room");
                cJSON_AddNumberToObject(res, KEY_ROOM_ID, rid);
                char *s = cJSON_PrintUnformatted(res);
                send(sd, s, strlen(s), 0);
                free(s);
                cJSON_Delete(res);
                usleep(50000);
                broadcast_room_info(rid);
            } else send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Room Full/Not Found");
        }
    }
    else if (strcmp(cmd, CMD_STR_LEAVE_ROOM) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1 && leave_room_logic(rid, client_idx)) {
            players[client_idx].state = STATE_LOBBY;
            players[client_idx].current_room_id = -1;
            send_json_result(sd, CMD_STR_ROOM_RESULT, 1, "Left Room");
        }
    }
    else if (strcmp(cmd, CMD_STR_GET_ROOM_LIST) == 0) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_LIST_RES);
        cJSON *arr = cJSON_CreateArray();
        for(int j=0; j<MAX_ROOMS; j++) {
            if(rooms[j].player_count > 0) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddNumberToObject(item, KEY_ROOM_ID, rooms[j].room_id);
                cJSON_AddNumberToObject(item, KEY_COUNT, rooms[j].player_count);
                cJSON_AddNumberToObject(item, KEY_STATUS, rooms[j].status);
                cJSON_AddItemToArray(arr, item);
            }
        }
        cJSON_AddItemToObject(res, KEY_ROOM_LIST, arr);
        char *s = cJSON_PrintUnformatted(res);
        send(sd, s, strlen(s), 0);
        free(s);
        cJSON_Delete(res);
    }
    else if (strcmp(cmd, CMD_STR_READY) == 0 || strcmp(cmd, CMD_STR_CANCEL_READY) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            handle_ready_toggle(rid, client_idx, (strcmp(cmd, CMD_STR_READY) == 0));
            int can_start = (rooms[rid].player_count == 2 && rooms[rid].white_ready && rooms[rid].black_ready);
            if (can_start) {
                rooms[rid].status = 2;
                usleep(50000);
                broadcast_game_start(rid);
            } else broadcast_room_info(rid);
        }
    }
    else if (strcmp(cmd, CMD_STR_PLACE_STONE) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            cJSON *x = cJSON_GetObjectItem(json, KEY_X);
            cJSON *y = cJSON_GetObjectItem(json, KEY_Y);
            if (x && y) handle_place_stone(rid, client_idx, x->valueint, y->valueint);
        }
    }

    // ★★★ 解锁：离开临界区 ★★★
    pthread_mutex_unlock(&g_logic_mutex);
}

// --- ★★★ 核心修改：客户端线程函数 ★★★ ---
void *client_thread_handler(void *arg) {
    int client_idx = *(int*)arg; free(arg); // 释放主线程传来的参数内存
    int sd = players[client_idx].socket_fd; // 获取该客户对应的 Socket 句柄 (用于收发数据)
    
    uint8_t rx_buffer[4096];    //每个线程独立的接收缓冲区
    int rx_len = 0;             //缓冲区内已有数据长度

    printf("[Thread] Client handler started for index %d\n", client_idx);

    while (1) {
        // 计算缓冲区还剩多少空间，防止溢出
        int remain_size = 4096 - rx_len - 1;
        if (remain_size <= 0) { rx_len = 0; remain_size = 4095; } // 溢出保护，满了就清空

        // ★ 阻塞式读取：没有数据线程就挂起睡觉，不占 CPU
        int valread = read(sd, rx_buffer + rx_len, remain_size);

        /*
        sd：要读取数据的 socket 描述符
        rx_buffer + rx_len：指向接收缓冲区中可用空间的指针
        remain_size：指定最多读取的字节数，防止缓冲区溢出
        返回值：实际读取的字节数，若返回0表示连接已关闭，返回-1表示出错
        */

        if (valread <= 0) 
        {
            // 断开连接处理
            printf("[Thread] Client %d disconnected\n", client_idx);
            
            // 操作全局 players 数组，加锁
            pthread_mutex_lock(&g_logic_mutex);
            close(sd);  //关闭 socket
            handle_disconnect(client_idx);
            players[client_idx].socket_fd = 0; // 释放位置
            pthread_mutex_unlock(&g_logic_mutex);
            
            break; // 跳出循环，结束线程
        } 
        else 
        {
            rx_len += valread;
            rx_buffer[rx_len] = '\0';

            // ★ 蓄水池解析逻辑
            int parse_offset = 0;
            while (parse_offset < rx_len) {
                char *start_ptr = strchr((char*)rx_buffer + parse_offset, '{');
                if (!start_ptr) break;
                
                parse_offset = (uint8_t*)start_ptr - rx_buffer;
                const char *end_ptr = NULL;

                // cJSON_ParseWithOpts：
                // 1. 它从 start_ptr 开始解析。
                // 2. 它会自动匹配 '{' 和 '}'。
                // 3. 如果解析成功，它会把 end_ptr 指向这个 JSON 最后一个字符的后面。
                // 4. 如果数据不全（比如只有 "{"cmd": ），它会解析失败，返回 NULL。
                cJSON *json = cJSON_ParseWithOpts(start_ptr, &end_ptr, 0);
                
                if (json) {
                    // 调用业务逻辑 (里面会自动加锁)
                    process_server_json(client_idx, json);
                    cJSON_Delete(json);
                    parse_offset = (uint8_t*)end_ptr - rx_buffer;
                } else {
                    break;
                }
            }

            // 内存搬运
            if (parse_offset > 0) {
                int remaining = rx_len - parse_offset;
                if (remaining > 0) memmove(rx_buffer, rx_buffer + parse_offset, remaining);
                rx_len = remaining;
            }
        }
    }

    return NULL;
}

int main() {
    signal(SIGPIPE, SIG_IGN);   //忽略 SIGPIPE 信号，防止往已关闭的 socket 写数据导致程序崩溃

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { exit(EXIT_FAILURE); } //若创建失败，则返回报错
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;   //IPv4
    address.sin_addr.s_addr = INADDR_ANY;   //监听所有网卡地址
    address.sin_port = htons(SERVER_PORT);  //绑定端口
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {    //绑定端口失败，则报错退出
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    listen(server_fd, 10);  //监听，最大等待队列10
    init_server_data(); // 初始化在线玩家和房间数据
    printf("=== 围棋服务器 (Multi-Threaded) 启动 Port: %d ===\n", SERVER_PORT);

    // --- ★★★ 核心修改：主线程只负责 Accept ★★★ ---
    while(1) {
        // 1. 阻塞等待新连接，直到新客户端连接进来
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) continue;

        /*
        int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
        sockfd：监听套接字描述符
        addr：指向 sockaddr_in 结构体的指针，用于存储客户端地址
        addrlen：指向地址长度的指针，初始值为 sizeof(sockaddr_in)，调用后会更新为实际地址长度
        返回值：成功时返回新创建的套接字描述符，失败时返回
        */

        printf("[Main] New Connection: %s, Socket %d\n", inet_ntoa(address.sin_addr), new_socket);

        // 2. 找一个空闲位置 (操作 players 数组需加锁)
        pthread_mutex_lock(&g_logic_mutex); // 上锁
        int free_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if(players[i].socket_fd == 0) {
                players[i].socket_fd = new_socket;
                players[i].state = STATE_CONNECTED;
                free_idx = i;
                break;
            }
        }
        pthread_mutex_unlock(&g_logic_mutex);

        if (free_idx != -1) 
        {
            // 3. 创建新线程专门服务这个客户端
            pthread_t tid;
            int *arg = malloc(sizeof(int)); // 专门为线程分配内存
            *arg = free_idx;

            //因为free_idx是主线程的变量，内容会实时变化，所以要分配一个新内存给子线程使用
            
            //尝试启动一个新线程
            if (pthread_create(&tid, NULL, client_thread_handler, arg) != 0) {
                
                /*
                &tid：如果创建成功，系统会把新线程的 ID 写在这里
                NULL：线程属性，NULL 表示默认属性
                client_thread_handler：线程函数指针，线程启动后会执行这个函数
                arg：传递给线程函数的参数，这里传递的是客户端在 players 数组中的索引
                返回值：成功返回 0，失败返回错误码
                */

                printf("Failed to create thread\n");
                close(new_socket);
                free(arg);
            
            } else pthread_detach(tid);    // 线程分离，线程结束后自动回收资源，不需要主线程 join
        } 
        else 
        {
            printf("Server Full, closing connection\n");
            close(new_socket);
        }
    }
    return 0;
}