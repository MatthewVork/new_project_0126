#ifndef GAME_PROTOCOL_H
#define GAME_PROTOCOL_H

#include <stdint.h>

// --- 基础配置 ---
#define SERVER_PORT 8888

// --- 命令字定义 (Command IDs) ---
// 0x00 - 0x0F: 系统/连接级
#define CMD_HEARTBEAT     0x00 // 心跳包 (可选)

// 0x10 - 0x1F: 账号相关
#define CMD_LOGIN         0x10 // 登录请求
#define CMD_REGISTER      0x11 // 注册请求
#define CMD_AUTH_RESULT   0x12 // 登录/注册结果
#define CMD_LOGOUT        0x13 // 退出登录请求

// 0x20 - 0x2F: 大厅/房间相关
#define CMD_GET_ROOM_LIST 0x20 // 获取房间列表
#define CMD_ROOM_LIST_RES 0x21 // 返回房间列表
#define CMD_CREATE_ROOM   0x22 // 创建房间
#define CMD_JOIN_ROOM     0x23 // 加入房间
#define CMD_ROOM_RESULT   0x24 // 房间操作结果(成功/失败)
#define CMD_LEAVE_ROOM    0x25 // 离开房间请求
#define CMD_ROOM_UPDATE   0x26 // 房间状态更新包

// 0x30 - 0x3F: 游戏逻辑相关 (围棋版)
#define CMD_GAME_START    0x30 // 游戏开始
#define CMD_PLACE_STONE   0x31 // 落子 (替代了原来的 MOVE_PIECE)
#define CMD_GAME_OVER     0x32 // 游戏结束
#define CMD_SURRENDER     0x33 // 认输
#define CMD_READY         0x34 // 准备请求
#define CMD_CANCEL_READY  0x35 // 取消准备请求

// --- 数据结构定义 ---
#pragma pack(1) // 强制1字节对齐，防止网络传输错位

// 1. 登录/注册包
typedef struct {
    uint8_t cmd;        // CMD_LOGIN 或 CMD_REGISTER
    char username[32];
    char password[32];
} AuthPacket;

// 2. 通用结果响应包
typedef struct {
    uint8_t cmd;        // CMD_AUTH_RESULT 或 CMD_ROOM_RESULT
    uint8_t success;    // 1=成功, 0=失败
    char message[64];   // 错误提示信息
} ResultPacket;

// 3. 房间信息 (用于列表显示)
typedef struct {
    int32_t room_id;
    uint8_t player_count; // 0, 1, 2
    uint8_t status;       // 0=等待中, 2=游戏中
} RoomInfo;

// 4. 房间操作请求
typedef struct {
    uint8_t cmd;        // CMD_CREATE_ROOM 或 CMD_JOIN_ROOM
    int32_t room_id;    // 创建时填-1，加入时填房间号
} RoomActionPacket;

// 5. ★围棋落子包★ (替换了原来的 MovePacket)
typedef struct {
    uint8_t cmd;    // CMD_PLACE_STONE (0x31)
    uint8_t x;      // 棋盘横坐标 (0-18)
    uint8_t y;      // 棋盘纵坐标 (0-18)
    uint8_t color;  // 0=黑子, 1=白子
} StonePacket;

// 6. ★游戏开始包★ (围棋版命名)
typedef struct {
    uint8_t cmd;         // CMD_GAME_START
    char black_name[32]; // 黑方名字 (P1)
    char white_name[32]; // 白方名字 (P2)
    uint8_t your_color;  // 0=你执黑(先手), 1=你执白
} GameStartPacket;

// 7. 房间状态更新包 (用于准备界面)
typedef struct {
    uint8_t cmd;            // CMD_ROOM_UPDATE (0x26)
    int32_t room_id;
    
    // 玩家1 (黑方/房主)
    char p1_name[32];       
    uint8_t p1_state;       // 0=空位, 1=有人
    uint8_t p1_ready;       // 1=已准备

    // 玩家2 (白方/挑战者)
    char p2_name[32];
    uint8_t p2_state;       // 0=空位, 1=有人
    uint8_t p2_ready;       // 1=已准备
} RoomUpdatePacket;

typedef struct {
    uint8_t cmd;          // CMD_GAME_OVER
    uint8_t winner_color; // 0:黑棋赢, 1:白棋赢, 2:对方逃跑(自动获胜)
} GameOverPacket;

#pragma pack()

#endif