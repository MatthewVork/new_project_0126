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
#define CMD_LOGOUT        0x13 // 退出登录请求 (新增, 0x13)

// 0x20 - 0x2F: 大厅/房间相关
#define CMD_GET_ROOM_LIST 0x20 // 获取房间列表
#define CMD_ROOM_LIST_RES 0x21 // 返回房间列表
#define CMD_CREATE_ROOM   0x22 // 创建房间
#define CMD_JOIN_ROOM     0x23 // 加入房间
#define CMD_ROOM_RESULT   0x24 // 房间操作结果(成功/失败)
#define CMD_LEAVE_ROOM    0x25 // 离开房间请求

// 0x30 - 0x3F: 游戏逻辑相关
#define CMD_GAME_START    0x30 // 游戏开始 (分配阵营)
#define CMD_MOVE_PIECE    0x31 // 走棋
#define CMD_GAME_OVER     0x32 // 游戏结束
#define CMD_SURRENDER     0x33 // 认输

// --- 数据结构定义 ---

// 1. 登录/注册包
#pragma pack(1)
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

// 3. 房间信息
typedef struct {
    int32_t room_id;
    uint8_t player_count; // 0, 1, 2
    uint8_t status;       // 0=等待中, 1=游戏中
} RoomInfo;

// 4. 房间操作请求
typedef struct {
    uint8_t cmd;        // CMD_CREATE_ROOM 或 CMD_JOIN_ROOM
    int32_t room_id;    // 创建时填-1，加入时填房间号
} RoomActionPacket;

// 5. 走棋数据包
typedef struct {
    uint8_t cmd;        // CMD_MOVE_PIECE
    uint8_t from_x;     // 起点 X
    uint8_t from_y;     // 起点 Y
    uint8_t to_x;       // 终点 X
    uint8_t to_y;       // 终点 Y
} MovePacket;
#pragma pack()

#endif