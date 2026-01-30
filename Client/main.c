#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"
#include "ui_generated/ui.h"
#include "network/network_client.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <sys/socket.h> 
#include "../Common/game_protocol.h" 
#include "../Common/cJSON.h" 

#include "game/game_config.h"
#include "game/board_view.h"

// --- 全局变量 ---
extern bool is_player_ready;    //玩家是否准备
int my_game_color = -1;         // 玩家颜色：0=黑，1=白，-1=未开始
int current_game_turn = 0;      //0=黑方回合, 1=白方回合
lv_timer_t * game_reset_timer = NULL;   //棋盘数据重置定时器，游戏结束后重置界面

// 玩家名称缓存，用于显示等待状态时使用
char p1_name_cache[32] = "";
char p2_name_cache[32] = "";

// 窗口尺寸
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 480

// 本地棋盘缓存
int local_board[19][19] = {0};  // 0:空, 1:黑, 2:白

void hide_all_popups(); // 隐藏所有弹窗
void timer_reset_game(lv_timer_t * timer);  // 游戏重置定时器回调
extern void net_send_place_stone_json(int x, int y, int color); // 发送落子JSON

// --- 辅助函数 ---
void clear_local_board() 
{
    memset(local_board, 0, sizeof(local_board));    // 清空本地棋盘缓存
}

void ui_reset_labels_to_waiting() { // 重置玩家标签为等待状态
    if (ui_LabelPlayer1) {
        lv_label_set_recolor(ui_LabelPlayer1, true);    // 启用重着色
        if (p1_name_cache[0] != '\0') lv_label_set_text_fmt(ui_LabelPlayer1, "Host: %s\n#ffff00 [WAITING]#", p1_name_cache);    //若有缓存名称则显示，否则显示等待
        else lv_label_set_text(ui_LabelPlayer1, "Host: ...\n#ffff00 [WAITING]#");
    }
    if (ui_LabelPlayer2) {
        lv_label_set_recolor(ui_LabelPlayer2, true);    //same
        if (p2_name_cache[0] != '\0') lv_label_set_text_fmt(ui_LabelPlayer2, "Player: %s\n#ffff00 [WAITING]#", p2_name_cache);
        else lv_label_set_text(ui_LabelPlayer2, "Player: ...\n#ffff00 [WAITING]#");
    }
}

void ui_clear_auth_inputs() {   // 清空登录注册输入框
    if (ui_InputUser) lv_textarea_set_text(ui_InputUser, ""); 
    if (ui_InputPass) lv_textarea_set_text(ui_InputPass, ""); 
}

void timer_reset_game(lv_timer_t * timer) { // 游戏重置定时器回调
    if (ui_PanelMatchWin) lv_obj_add_flag(ui_PanelMatchWin, LV_OBJ_FLAG_HIDDEN);
    if (ui_PanelMatchLoss) lv_obj_add_flag(ui_PanelMatchLoss, LV_OBJ_FLAG_HIDDEN);  //无论胜负面板是否显示，均隐藏
    if (ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
    if (ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);    //结束游戏后隐藏回合指示图标
    
    if(ui_BoardContainer) board_view_clear(ui_BoardContainer);  // 清空棋盘视图
    clear_local_board();  // 清空本地棋盘数据缓存

    //清空视图是指玩家画面的重置，清空本地棋盘数据缓存是为了同步服务器数据，防止误判

    if (ui_Button5) lv_obj_clear_flag(ui_Button5, LV_OBJ_FLAG_HIDDEN);  // 显示准备按钮---因为游戏结束后需要重新准备
    if (ui_Labelreadybtninfo) lv_label_set_text(ui_Labelreadybtninfo, "Ready"); // 重置准备按钮文本
    game_reset_timer = NULL;    // 执行完毕，清空定时器指针
}

void update_turn_ui() { // 更新回合指示UI
    if (!ui_ImgTurnP1 || !ui_ImgTurnP2) return; // 安全检查，如果指示图标不存在则返回
    if (current_game_turn == 0) { 
        lv_obj_clear_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);   
    } else { 
        lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);   
        lv_obj_clear_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN); 
    }
}

void timer_hide_start_panel(lv_timer_t * timer) {   // 隐藏开始提示面板定时器回调
    if (ui_PanelStartTip) lv_obj_add_flag(ui_PanelStartTip, LV_OBJ_FLAG_HIDDEN);
}

// --- 事件处理 ---
void event_board_click_handler(lv_event_t * e) {    // 棋盘点击事件
    if (my_game_color != current_game_turn) return; // 非自己回合不处理

    int x, y;
    if (board_view_get_click_xy(e->target, &x, &y)) {   // 若获取点击坐标成功，调用发送函数

        //防呆处理防止越界和有子覆盖
        if (x < 0 || x >= 19 || y < 0 || y >= 19) return;
        if (local_board[x][y] != 0) return;

        net_send_place_stone_json(x, y, my_game_color);
        printf("[Game] Sent move (%d, %d)\n", x, y);
    }
}

void event_join_room_handler(lv_event_t * e) {  // 房间按钮点击事件
    lv_obj_t * item = lv_event_get_target(e);   // 获取触发事件的对象
    int room_id = (int)(intptr_t)lv_obj_get_user_data(item);    // 从用户数据中获取房间ID
    net_send_join_room(room_id);
}

// ★★★ 确保这些回调存在，否则登录不会跳转 ★★★
void timer_reg_success_callback(lv_timer_t * timer) {   // 注册成功跳转登录回调
    hide_all_popups();  //隐藏所有弹窗
    ui_clear_auth_inputs(); // 清空输入框
    // 注册成功 -> 跳转登录
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLogin_screen_init);  //切换到登录界面
}
void timer_login_success_callback(lv_timer_t * timer) { //  登录成功跳转大厅回调
    hide_all_popups();
    ui_clear_auth_inputs();
    // 登录成功 -> 跳转大厅
    _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLobby_screen_init);
    net_send_get_room_list();   // 获取房间列表
}
void timer_close_popup_callback(lv_timer_t * timer) { hide_all_popups(); }  // 关闭弹窗回调，负责隐藏所有弹窗

void hide_all_popups() {
    if(ui_PanelLoginSuccess) lv_obj_add_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginFail)    lv_obj_add_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginRepeat)  lv_obj_add_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegSuccess)   lv_obj_add_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegFail)      lv_obj_add_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegRepeat)    lv_obj_add_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
}

void process_one_json(cJSON *json) {    // 处理一条JSON消息，负责分发任务到各个模块
    if (!json) return;
    cJSON *cmdItem = cJSON_GetObjectItem(json, KEY_CMD);    // 获取命令字段
    if (!cmdItem || !cJSON_IsString(cmdItem)) return;   // 如果没有 cmd 或者 cmd 不是字符串，直接忽略
    char *cmd = cmdItem->valuestring;

    // ★ 登录/注册回复逻辑 ★
    if (strcmp(cmd, CMD_STR_AUTH_RESULT) == 0) {
        cJSON *succItem = cJSON_GetObjectItem(json, KEY_SUCCESS);   // 获取成功字段
        cJSON *msgItem = cJSON_GetObjectItem(json, KEY_MESSAGE);    // 获取消息字段
        int success = succItem ? succItem->valueint : 0;
        char *msg = msgItem ? msgItem->valuestring : "";    // 默认空消息
        
        if (lv_scr_act() == ui_ScreenRegister) {
            hide_all_popups(); 
            if (success) {
                if(ui_PanelRegSuccess) lv_obj_clear_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
                lv_timer_set_repeat_count(lv_timer_create(timer_reg_success_callback, 1500, NULL), 1);  //注册成功跳转登录，1.5秒后自动关闭弹窗
            } else {
                if (strstr(msg, "Exist") || strstr(msg, "Repeat")) {    //注册失败，用户名已存在或重复
                    if(ui_PanelRegRepeat) lv_obj_clear_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
                } else {
                    if(ui_PanelRegFail) lv_obj_clear_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
                }
                lv_timer_set_repeat_count(lv_timer_create(timer_close_popup_callback, 1500, NULL), 1);
            }
        } else if (lv_scr_act() == ui_ScreenLogin) {
            hide_all_popups();
            if (success) {
                if(ui_PanelLoginSuccess) lv_obj_clear_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
                // 触发跳转定时器
                lv_timer_set_repeat_count(lv_timer_create(timer_login_success_callback, 1500, NULL), 1);
            } else {
                if (strstr(msg, "Repeat") || strstr(msg, "Already")) {
                    if(ui_PanelLoginRepeat) lv_obj_clear_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
                } else {
                    if(ui_PanelLoginFail) lv_obj_clear_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
                }
                lv_timer_set_repeat_count(lv_timer_create(timer_close_popup_callback, 1500, NULL), 1);
            }
        }
    }
    // ★ 房间列表回复逻辑 ★
    else if (strcmp(cmd, CMD_STR_ROOM_LIST_RES) == 0) {
        cJSON *list = cJSON_GetObjectItem(json, KEY_ROOM_LIST);
        // 如果 list 是数组
        if (cJSON_IsArray(list)) {
            // 清空旧的房间按钮
            uint32_t child_cnt = lv_obj_get_child_cnt(ui_PanelRoomContainer);
            for(int i = child_cnt - 1; i >= 0; i--) {
                lv_obj_t * child = lv_obj_get_child(ui_PanelRoomContainer, i);
                if (child != ui_PanelRoomTemplate) lv_obj_del(child);
            }
            // 遍历数组，创建新按钮
            int count = cJSON_GetArraySize(list);
            for (int i=0; i<count; i++) {
                cJSON *item = cJSON_GetArrayItem(list, i);
                int rid = cJSON_GetObjectItem(item, KEY_ROOM_ID)->valueint;
                int p_cnt = cJSON_GetObjectItem(item, KEY_COUNT)->valueint;
                int stat = cJSON_GetObjectItem(item, KEY_STATUS)->valueint;
                
                // 创建按钮背景
                lv_obj_t * room_btn = lv_obj_create(ui_PanelRoomContainer);
                lv_obj_set_size(room_btn, 260, 120);
                lv_obj_set_style_bg_color(room_btn, lv_color_hex(0x404040), 0);
                lv_obj_set_style_border_width(room_btn, 2, 0);
                lv_obj_set_style_radius(room_btn, 10, 0);
                // 绑定房间ID到按钮
                lv_obj_set_user_data(room_btn, (void*)(intptr_t)rid);
                
                // 创建房间号标签
                lv_obj_t * label = lv_label_create(room_btn);
                lv_label_set_text_fmt(label, "Room #%d\n%s", rid, stat==0?"Wait":"Play");
                lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
                
                // 创建状态标签
                lv_obj_t * label_stat = lv_label_create(room_btn);
                if(stat == 0) {
                    lv_label_set_text_fmt(label_stat, "Waiting (%d/2)", p_cnt);
                    lv_obj_set_style_text_color(label_stat, lv_color_hex(0x00FF00), 0);
                } else {
                    lv_label_set_text(label_stat, "Playing (2/2)");
                    lv_obj_set_style_text_color(label_stat, lv_color_hex(0xFF0000), 0);
                }
                lv_obj_align(label_stat, LV_ALIGN_BOTTOM_MID, 0, -15);
                // 绑定点击事件
                lv_obj_add_event_cb(room_btn, event_join_room_handler, LV_EVENT_CLICKED, NULL);
            }
        }
    }
    // ★ 进出房间回复逻辑 (含跳转) ★
    else if (strcmp(cmd, CMD_STR_ROOM_RESULT) == 0) {
        cJSON *succItem = cJSON_GetObjectItem(json, KEY_SUCCESS);
        cJSON *msgItem = cJSON_GetObjectItem(json, KEY_MESSAGE);
        int success = succItem ? succItem->valueint : 0;
        char *msg = msgItem ? msgItem->valuestring : "";
        if (success) {
            if (msg && strstr(msg, "Left")) {
                // 离开房间 -> 回大厅
                _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLobby_screen_init);
                net_send_get_room_list(); 
            } else {
                // 建房/进房 -> 进游戏界面
                if (lv_scr_act() != ui_ScreenGame) {
                    _ui_screen_change(&ui_ScreenGame, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenGame_screen_init);
                    if(ui_BoardContainer) board_view_clear(ui_BoardContainer); 
                    clear_local_board(); 
                }
            }
        }
    }
    else if (strcmp(cmd, CMD_STR_ROOM_UPDATE) == 0) {
        cJSON *p1nItem = cJSON_GetObjectItem(json, KEY_P1_NAME);    // 主机名
        cJSON *p1rItem = cJSON_GetObjectItem(json, KEY_P1_READY);   // 主机准备状态
        cJSON *p2nItem = cJSON_GetObjectItem(json, KEY_P2_NAME);    // 玩家名
        cJSON *p2rItem = cJSON_GetObjectItem(json, KEY_P2_READY);   // 玩家准备状态
        char *p1n = p1nItem ? p1nItem->valuestring : "";    //若空则设为""，避免野指针，反之则为取值
        int p1r = p1rItem ? p1rItem->valueint : 0;
        char *p2n = p2nItem ? p2nItem->valuestring : "";
        int p2r = p2rItem ? p2rItem->valueint : 0;
        if (ui_LabelPlayer1) {
            lv_label_set_recolor(ui_LabelPlayer1, true);
            if (strlen(p1n) > 0) {
                strncpy(p1_name_cache, p1n, 31);
                lv_label_set_text_fmt(ui_LabelPlayer1, "Host: %s\n%s", p1n, p1r ? "#00ff00 [READY]#" : "#ffff00 [WAITING]#");
            } else {
                p1_name_cache[0] = '\0';
                lv_label_set_text(ui_LabelPlayer1, "Host: [EMPTY]\n#808080 [OFFLINE]#");
            }
        }
        if (ui_LabelPlayer2) {
            lv_label_set_recolor(ui_LabelPlayer2, true);
            if (strlen(p2n) > 0) {
                strncpy(p2_name_cache, p2n, 31);
                lv_label_set_text_fmt(ui_LabelPlayer2, "Player: %s\n%s", p2n, p2r ? "#00ff00 [READY]#" : "#ffff00 [WAITING]#");
            } else {
                p2_name_cache[0] = '\0';
                lv_label_set_text(ui_LabelPlayer2, "Waiting for\nPlayer...");
            }
        }
    }
    else if (strcmp(cmd, CMD_STR_GAME_START) == 0) {
        cJSON *colorItem = cJSON_GetObjectItem(json, KEY_YOUR_COLOR);
        my_game_color = colorItem ? colorItem->valueint : 0;
        printf("[Game] Start! I am: %d\n", my_game_color);
        current_game_turn = 0; 
        if (game_reset_timer) { lv_timer_del(game_reset_timer); game_reset_timer = NULL; }
        if(ui_BoardContainer) board_view_clear(ui_BoardContainer);
        clear_local_board(); 
        if (ui_PanelMatchWin) lv_obj_add_flag(ui_PanelMatchWin, LV_OBJ_FLAG_HIDDEN);
        if (ui_PanelMatchLoss) lv_obj_add_flag(ui_PanelMatchLoss, LV_OBJ_FLAG_HIDDEN);
        if (ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
        if (ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);
        update_turn_ui(); 
        is_player_ready = false; 
        if(ui_Labelreadybtninfo) lv_label_set_text(ui_Labelreadybtninfo, "Ready?");
        if (ui_PanelStartTip) {
            lv_obj_clear_flag(ui_PanelStartTip, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_PanelStartTip);
            lv_timer_set_repeat_count(lv_timer_create(timer_hide_start_panel, 1500, NULL), 1);
        }
        if (ui_Button5) lv_obj_add_flag(ui_Button5, LV_OBJ_FLAG_HIDDEN);
    }
    // ★★★ 增量落子 (只画一颗子) ★★★
    else if (strcmp(cmd, CMD_STR_PLACE_STONE) == 0) {
        cJSON *xItem = cJSON_GetObjectItem(json, KEY_X);
        cJSON *yItem = cJSON_GetObjectItem(json, KEY_Y);
        cJSON *cItem = cJSON_GetObjectItem(json, KEY_COLOR);
        if (xItem && yItem && cItem) {
            int x = xItem->valueint;
            int y = yItem->valueint;
            int color = cItem->valueint;
            
            if(x >=0 && x < 19 && y >= 0 && y < 19) {
                // 1. 更新本地数据
                local_board[x][y] = color + 1; 
                // 2. 直接画子 (不清屏)
                board_view_draw_stone(ui_BoardContainer, x, y, color);
                // 3. 切换回合
                current_game_turn = !current_game_turn;
                update_turn_ui();
            }
        }
    }
    else if (strcmp(cmd, CMD_STR_GAME_OVER) == 0) {
        cJSON *winItem = cJSON_GetObjectItem(json, KEY_WINNER);
        int winner = winItem ? winItem->valueint : -1;
        printf("[Client] Game Over, Winner: %d\n", winner);
        if (winner == my_game_color) {
            if (ui_PanelMatchWin) {
                lv_obj_clear_flag(ui_PanelMatchWin, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(ui_PanelMatchWin);
            }
        } else {
            if (ui_PanelMatchLoss) {
                lv_obj_clear_flag(ui_PanelMatchLoss, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(ui_PanelMatchLoss);
            }
        }
        ui_reset_labels_to_waiting();
        if (game_reset_timer) lv_timer_del(game_reset_timer);
        game_reset_timer = lv_timer_create(timer_reset_game, 3000, NULL);
        lv_timer_set_repeat_count(game_reset_timer, 1);
    }
}

int main(void)
{
    lv_init();
    sdl_init();

    // 显示驱动初始化
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[WINDOW_WIDTH * WINDOW_HEIGHT / 10];
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, WINDOW_WIDTH * WINDOW_HEIGHT / 10);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = WINDOW_WIDTH;
    disp_drv.ver_res = WINDOW_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);
    
    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read;
    lv_indev_t * kb_indev = lv_indev_drv_register(&kb_drv);
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(kb_indev, g);

    ui_init();
    
    if (ui_BoardContainer) {
        lv_obj_set_style_pad_all(ui_BoardContainer, 0, 0);       
        lv_obj_set_style_border_width(ui_BoardContainer, 0, 0);  
        lv_obj_add_event_cb(ui_BoardContainer, event_board_click_handler, LV_EVENT_CLICKED, NULL);
    }
    if(ui_InputUser) lv_group_add_obj(g, ui_InputUser);
    if(ui_InputPass) lv_group_add_obj(g, ui_InputPass);
    if(ui_BtnLogin)  lv_group_add_obj(g, ui_BtnLogin);

    if(ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
    if(ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);

    // ★ 请确认IP ★
    if (net_init("172.24.139.145", 8888) != 0) {
        printf("Connect failed\n");
        return -1;
    }

    // ★★★ 核心：蓄水池接收逻辑 (兼容 Text-Stream 协议) ★★★
    static uint8_t rx_buffer[10240];    // 10KB 蓄水池
    static int rx_len = 0;  // 蓄水池当前数据长度

    //该蓄水池负责存放从网络收到的数据，然后尝试解析完整的JSON对象
    //如果解析成功则处理该JSON对象，并将已处理的数据从蓄水池中移除
    //如果解析失败（可能是半包），则等待更多数据到来
    //这样可以处理TCP流式传输中可能出现的粘包和半包问题
    //解析时通过查找起始符 '{' 来定位JSON对象的开始位置
    //然后使用 cJSON_ParseWithOpts 尝试解析
    //成功后处理该对象，失败则保留数据等待下次接收
    //处理完后将剩余数据移到蓄水池前端，准备下一次接收
    //这种方式确保了即使数据包被拆分或合并，仍然能够正确解析出完整的JSON对象
    //从而实现可靠的网络通信

    while(1) {
        lv_timer_handler(); 
        
        uint8_t temp_buf[1024];
        // 调用真实的 net_poll
        int n = net_poll(temp_buf); 
        
        if (n > 0) {
            // 拼接到蓄水池
            if (rx_len + n < 10240) {
                memcpy(rx_buffer + rx_len, temp_buf, n);
                rx_len += n;
                rx_buffer[rx_len] = '\0';
            } else {
                rx_len = 0; // 溢出重置
            }
            
            // 循环解析
            int parse_offset = 0;
            while (parse_offset < rx_len) {
                // 找 JSON 起始符
                char *start_ptr = strchr((char*)rx_buffer + parse_offset, '{'); //找寻{作为cJSON起始
                if (!start_ptr) break; 
                
                parse_offset = (uint8_t*)start_ptr - rx_buffer; // 更新偏移到起始符位置
                const char *end_ptr = NULL; // 结尾指针 
                // 尝试解析
                cJSON *json = cJSON_ParseWithOpts(start_ptr, &end_ptr, 0);
                
                if (json) {
                    process_one_json(json); // 处理该 JSON 对象
                    cJSON_Delete(json); // 释放内存
                    // 移动到 JSON 结尾，继续找下一个
                    parse_offset = (uint8_t*)end_ptr - rx_buffer;
                } else {
                    // 半包，等待更多数据
                    break;
                }
            }

            // 内存搬运：把剩下的数据移到最前面
            if (parse_offset > 0) {
                int remaining = rx_len - parse_offset;
                if (remaining > 0) {
                    memmove(rx_buffer, rx_buffer + parse_offset, remaining);
                }
                rx_len = remaining;
                memset(rx_buffer + rx_len, 0, 10240 - rx_len);
            }
        }

        usleep(5000); 
        lv_tick_inc(5);
    }
    return 0;
}