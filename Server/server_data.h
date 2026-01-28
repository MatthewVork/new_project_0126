#ifndef SERVER_DATA_H
#define SERVER_DATA_H

#include <netinet/in.h>

// --- 参数配置 ---
#define MAX_CLIENTS 10
#define MAX_ROOMS 5

// --- 状态定义 ---
#define STATE_DISCONNECTED 0
#define STATE_CONNECTED    1  // 已连接但未登录
#define STATE_LOBBY        2  // 已登录，在大厅
#define STATE_IN_ROOM      3  // 在房间中

// --- 数据结构 ---

// 玩家结构体
typedef struct {
    int socket_fd;          // 套接字句柄 (0表示空闲)
    int state;              // 当前状态
    char username[32];      // 用户名
    int current_room_id;    // 当前所在的房间ID (-1表示不在房间)
} Player;

// 房间结构体
typedef struct {
    int room_id;
    int player_count;       // 当前人数 (0-2)
    int status;             // 0=等待中, 1=游戏中
    
    // 记录两个玩家在 players 数组中的下标 (index)
    int white_player_idx;   
    int black_player_idx;

    // --- 新增：准备标记 ---
    int white_ready;      // 1=已准备, 0=未准备
    int black_ready;      // 1=已准备, 0=未准备
} Room;

// --- 全局变量声明 (extern) ---
// 告诉其他 .c 文件：这些变量是在 server_main.c 里定义的，你们可以直接用
extern Player players[MAX_CLIENTS];
extern Room rooms[MAX_ROOMS];

// 房间管理逻辑 (来自 room_manager.c)
void init_rooms();
int create_room_logic(int player_idx);
int join_room_logic(int room_id, int player_idx);
int leave_room_logic(int room_id, int player_idx); // 新增的退出逻辑
void broadcast_game_start(int room_id);
void broadcast_room_info(int room_id);

// 用户管理逻辑 (来自 user_manager.c)
int check_login(const char* user, const char* pass);
int check_register(const char* user, const char* pass);
void handle_logout(int player_idx);
void handle_disconnect(int player_idx);
void handle_ready_toggle(int room_id, int player_idx, int is_ready);
void broadcast_game_start(int room_id);

#endif