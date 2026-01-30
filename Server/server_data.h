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
    int player_count;
    int status; // 0=Wait, 2=Playing
    
    // 玩家位置
    int white_player_idx;   // 玩家索引 (-1表示无玩家，1代表白方，0代表黑方)
    int black_player_idx;   
    int white_ready;
    int black_ready;

    // 0:空, 1:黑(BLACK), 2:白(WHITE) —— 这样 memset(0) 就是清空
    int board[19][19];   
    
    uint8_t current_turn;   // 0=黑方回合, 1=白方回合
} Room;

// --- 全局变量声明 (extern) ---
extern Player players[MAX_CLIENTS];
extern Room rooms[MAX_ROOMS];

// --- 函数声明 ---

// 房间管理 (来自 room_manager.c)
void init_rooms();
int create_room_logic(int player_idx);
int join_room_logic(int room_id, int player_idx);
int leave_room_logic(int room_id, int player_idx);
void broadcast_room_info(int room_id);
void broadcast_game_start(int room_id);
void handle_ready_toggle(int room_id, int player_idx, int is_ready);

// ★ 新增：围棋逻辑声明 ★
void handle_place_stone(int room_id, int player_idx, int x, int y);
// void handle_surrender(int room_id, int player_idx); // 如果暂时没做认输，这行可以先注释

// 用户管理 (来自 user_manager.c)
int check_login(const char* user, const char* pass);
int check_register(const char* user, const char* pass);
void handle_logout(int player_idx);
void handle_disconnect(int player_idx);

#endif