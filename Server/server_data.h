#ifndef SERVER_DATA_H
#define SERVER_DATA_H

#include <netinet/in.h> // for sockaddr_in

#define MAX_CLIENTS 100
#define MAX_ROOMS 50

// 玩家状态枚举
typedef enum {
    STATE_DISCONNECTED = 0, // 断开
    STATE_CONNECTED,        // 已连接但未登录
    STATE_LOBBY,            //在大厅（已登录）
    STATE_IN_ROOM,          // 在房间等待
    STATE_PLAYING           // 游戏中
} ClientState;

// 玩家结构体
typedef struct {
    int socket_fd;          // 套接字描述符
    ClientState state;      // 当前状态
    char username[32];      // 用户名
    int current_room_id;    // 当前所在的房间ID (-1表示不在)
    int is_red_side;        // 1=红方, 0=黑方
} Player;

// 房间结构体
typedef struct {
    int id;                 // 房间ID
    int player1_idx;        // 玩家1在 players数组中的下标 (-1为空)
    int player2_idx;        // 玩家2在 players数组中的下标 (-1为空)
    int is_active;          // 是否被占用
    int game_started;       // 是否正在游戏
} Room;

// 导出全局变量 (在 server_main.c 中定义)
extern Player players[MAX_CLIENTS];
extern Room rooms[MAX_ROOMS];

// 函数声明
void init_server_data();
int find_free_player_slot();
int find_player_by_fd(int fd);

#endif