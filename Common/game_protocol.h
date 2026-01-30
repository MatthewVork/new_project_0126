#ifndef GAME_PROTOCOL_H
#define GAME_PROTOCOL_H

#include <stdint.h>

// --- 基础配置 ---
#define SERVER_PORT 8888

// =============================================================
// ★★★ 第一部分：JSON 协议常量定义 (新增核心) ★★★
// =============================================================

// 1. 通用字段 Key
#define KEY_CMD       "cmd"         // 指令类型
#define KEY_PAYLOAD   "payload"     // 数据载荷(备用)
#define KEY_SUCCESS   "success"     // 成功标记 (1/0)
#define KEY_MESSAGE   "message"     // 提示信息

// 2. 账号相关 Key
#define KEY_USERNAME  "username"
#define KEY_PASSWORD  "password"

// 3. 房间/游戏相关 Key
#define KEY_ROOM_ID   "room_id"
#define KEY_X         "x"
#define KEY_Y         "y"
#define KEY_COLOR     "color"       // 0=Black, 1=White
#define KEY_WINNER    "winner"      // 获胜者颜色

// 4. 房间列表相关 Key
#define KEY_ROOM_LIST "list"        // 房间数组
#define KEY_COUNT     "count"       // 房间/人数数量
#define KEY_STATUS    "status"      // 房间状态: 0=Wait, 2=Play

// 5. 房间内状态同步 Key
#define KEY_P1_NAME   "p1_name"     // 房主名字
#define KEY_P1_READY  "p1_ready"    // 房主准备状态
#define KEY_P2_NAME   "p2_name"     // 挑战者名字
#define KEY_P2_READY  "p2_ready"    // 挑战者准备状态
#define KEY_YOUR_COLOR "your_color" // 游戏开始时分配给你的颜色

// -------------------------------------------------------------
// JSON 指令字 (Value Definitions) - 这些是网络传输的实际字符串
// -------------------------------------------------------------

// [账号模块]
#define CMD_STR_LOGIN         "login"
#define CMD_STR_REGISTER      "register"
#define CMD_STR_LOGOUT        "logout"
#define CMD_STR_AUTH_RESULT   "auth_result"

// [大厅模块]
#define CMD_STR_CREATE_ROOM   "create_room"
#define CMD_STR_JOIN_ROOM     "join_room"
#define CMD_STR_LEAVE_ROOM    "leave_room"
#define CMD_STR_GET_ROOM_LIST "get_room_list"
#define CMD_STR_ROOM_LIST_RES "room_list_res" // 房间列表响应
#define CMD_STR_ROOM_RESULT   "room_result"   // 进房/建房的操作结果

// [游戏/房间模块]
#define CMD_STR_ROOM_UPDATE   "room_update"   // 广播房间内状态(谁准备了)
#define CMD_STR_READY         "ready"         // 准备
#define CMD_STR_CANCEL_READY  "cancel_ready"  // 取消准备
#define CMD_STR_GAME_START    "game_start"    // 游戏开始
#define CMD_STR_PLACE_STONE   "place_stone"   // 落子
#define CMD_STR_GAME_OVER     "game_over"     // 游戏结束


// =============================================================
// ★★★ 第二部分：旧的二进制定义 (暂时保留，防止编译报错) ★★★
// (等你把后面4个文件都改完，下面这些 struct 就可以删掉了)
// =============================================================

// 旧命令 ID
#define CMD_LOGIN_BIN         0x10 
#define CMD_REGISTER_BIN      0x11 
#define CMD_AUTH_RESULT_BIN   0x12 
#define CMD_LOGOUT_BIN        0x13 
#define CMD_GET_ROOM_LIST_BIN 0x20 
#define CMD_ROOM_LIST_RES_BIN 0x21 
#define CMD_CREATE_ROOM_BIN   0x22 
#define CMD_JOIN_ROOM_BIN     0x23 
#define CMD_ROOM_RESULT_BIN   0x24 
#define CMD_LEAVE_ROOM_BIN    0x25 
#define CMD_ROOM_UPDATE_BIN   0x26 
#define CMD_GAME_START_BIN    0x30 
#define CMD_PLACE_STONE_BIN   0x31 
#define CMD_GAME_OVER_BIN     0x32 
#define CMD_READY_BIN         0x34 
#define CMD_CANCEL_READY_BIN  0x35 

#pragma pack(1)

typedef struct {
    uint8_t cmd;        
    char username[32];
    char password[32];
} AuthPacket;

typedef struct {
    uint8_t cmd;        
    uint8_t success;    
    char message[64];   
} ResultPacket;

typedef struct {
    int32_t room_id;
    uint8_t player_count; 
    uint8_t status;       
} RoomInfo;

typedef struct {
    uint8_t cmd;        
    int32_t room_id;    
} RoomActionPacket;

typedef struct {
    uint8_t cmd;    
    uint8_t x;      
    uint8_t y;      
    uint8_t color;  
} StonePacket;

typedef struct {
    uint8_t cmd;         
    char black_name[32]; 
    char white_name[32]; 
    uint8_t your_color;  
} GameStartPacket;

typedef struct {
    uint8_t cmd;            
    int32_t room_id;
    char p1_name[32];       
    uint8_t p1_state;       
    uint8_t p1_ready;       
    char p2_name[32];
    uint8_t p2_state;       
    uint8_t p2_ready;       
} RoomUpdatePacket;

typedef struct {
    uint8_t cmd;          
    uint8_t winner_color; 
} GameOverPacket;

#pragma pack()

#endif