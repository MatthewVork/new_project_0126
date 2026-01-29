#ifndef GAME_PROTOCOL_H
#define GAME_PROTOCOL_H

#include <stdint.h>

// --- 基础配置 ---
#define SERVER_PORT 8888

// --- 1. 新增：JSON 协议相关定义 ---
// 只要包的第一个字符是 '{'，就认为是 JSON 包
#define KEY_CMD "cmd"
#define KEY_PAYLOAD "payload"
#define KEY_MESSAGE "message"
#define KEY_SUCCESS "success"

// JSON 指令字 (字符串)
#define CMD_STR_PLACE_STONE "place_stone"

// JSON 字段 Key
#define KEY_X "x"
#define KEY_Y "y"
#define KEY_COLOR "color" 
// color: 0=Black, 1=White


// --- 2. 旧的二进制命令 ID (保持不变，为了兼容) ---
// 0x00 - 0x0F: 系统/连接级
#define CMD_HEARTBEAT     0x00 

// 0x10 - 0x1F: 账号相关
#define CMD_LOGIN         0x10 
#define CMD_REGISTER      0x11 
#define CMD_AUTH_RESULT   0x12 
#define CMD_LOGOUT        0x13 

// 0x20 - 0x2F: 大厅/房间相关
#define CMD_GET_ROOM_LIST 0x20 
#define CMD_ROOM_LIST_RES 0x21 
#define CMD_CREATE_ROOM   0x22 
#define CMD_JOIN_ROOM     0x23 
#define CMD_ROOM_RESULT   0x24 
#define CMD_LEAVE_ROOM    0x25 
#define CMD_ROOM_UPDATE   0x26 

// 0x30 - 0x3F: 游戏逻辑相关
#define CMD_GAME_START    0x30 
#define CMD_PLACE_STONE   0x31 // (虽然我们这次用JSON，但保留定义防止报错)
#define CMD_GAME_OVER     0x32 
#define CMD_SURRENDER     0x33 
#define CMD_READY         0x34 
#define CMD_CANCEL_READY  0x35 

// --- 3. 旧的数据结构定义 (保持不变) ---
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