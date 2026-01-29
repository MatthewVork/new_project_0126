#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../network/network_client.h"
#include "../game/board_view.h"

bool is_player_ready = false;
extern lv_timer_t * game_reset_timer;
extern void send_raw(void *data, int len); // 声明 network_client.c 里的函数

// 登录按钮
void OnLoginClicked(lv_event_t * e)
{
    const char* username = lv_textarea_get_text(ui_InputUser);
    const char* password = lv_textarea_get_text(ui_InputPass);

    if (strlen(username) == 0 || strlen(password) == 0) {
        printf("[UI] Input cannot be empty\n");
        return;
    }
    printf("[UI] Login Request: %s\n", username);
    net_send_login(username, password);
}

// 注册按钮
void OnRegisterClicked(lv_event_t * e)
{
    const char* username = lv_textarea_get_text(ui_RegUser);
    const char* password = lv_textarea_get_text(ui_RegPass);
    const char* confirm  = lv_textarea_get_text(ui_RegPassConfirm);

    if (strlen(username) == 0 || strlen(password) == 0) return;

    if (strcmp(password, confirm) != 0) {
        printf("[UI] Passwords do not match!\n");
        return;
    }
    printf("[UI] Register Request: %s\n", username);
    net_send_register(username, password);
}

// 退出登录按钮 (新增)
void OnLogoutClicked(lv_event_t * e)
{
    printf("[UI] 玩家点击了注销 (Logout)\n");

    // 1. 发送网络请求：告诉服务器清除我的在线状态
    // (这需要你在 network_client.c 里已经实现了 net_send_logout)
    net_send_logout();
    
    // 2. 界面跳转：切回登录页面
    // 参数说明: 目标屏幕, 动画方式(向右滑出), 动画时间500ms, 延迟0ms, 初始化函数
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLogin_screen_init);
}
void OnRefreshClicked(lv_event_t * e) {
    net_send_get_room_list(); // 发送请求
}

// 创建房间按钮被点击
void OnCreateRoomClicked(lv_event_t * e) {
    net_send_create_room();   // 发送请求
}

// 对应你在 SLS 里给按钮设置的 Clicked 事件函数名
// Client/ui_events.c

// Client/ui_events.c

void OnLeaveRoomClicked(lv_event_t * e)
{
    printf("[UI] 玩家点击了离开房间按钮\n");
    net_send_leave_room();

    // ★★★ 这里的修改：增加杀定时器逻辑 ★★★
    if (game_reset_timer) {
        lv_timer_del(game_reset_timer);
        game_reset_timer = NULL;
    }

    // (下面的清理代码保持不变)
    if (ui_BoardContainer) board_view_clear(ui_BoardContainer);
    if (ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
    if (ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);
    if (ui_PanelMatchWin) lv_obj_add_flag(ui_PanelMatchWin, LV_OBJ_FLAG_HIDDEN);
    if (ui_PanelMatchLoss) lv_obj_add_flag(ui_PanelMatchLoss, LV_OBJ_FLAG_HIDDEN);

    is_player_ready = false;
    if (ui_Labelreadybtninfo) lv_label_set_text(ui_Labelreadybtninfo, "Ready");
    if (ui_Button5) lv_obj_clear_flag(ui_Button5, LV_OBJ_FLAG_HIDDEN);

    // 如果你想自动跳回大厅，取消下面这行的注释
    // _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLobby_screen_init);
}

void OnReadyClicked(lv_event_t * e) {
    if (!is_player_ready) {
        // --- 动作：准备 ---
        uint8_t cmd = CMD_READY;
        send_raw(&cmd, 1);
        
        // 更新 UI
        lv_label_set_text(ui_Labelreadybtninfo, "I'm Ready");
        // 如果想改按钮颜色，也可以在这里改
        // lv_obj_set_style_bg_color(ui_BtnReady, lv_palette_main(LV_PALETTE_GREEN), 0);
        
        is_player_ready = true;
    } else {
        // --- 动作：取消准备 ---
        uint8_t cmd = CMD_CANCEL_READY;
        send_raw(&cmd, 1);
        
        // 更新 UI
        lv_label_set_text(ui_Labelreadybtninfo, "Not Ready");
        
        is_player_ready = false;
    }
}
void Texture_clean(lv_event_t * e)
{
	lv_textarea_set_text(ui_InputUser, "");
    lv_textarea_set_text(ui_InputPass, "");
    lv_textarea_set_text(ui_RegUser, "");
    lv_textarea_set_text(ui_RegPass, "");
    lv_textarea_set_text(ui_RegPassConfirm, "");
}
