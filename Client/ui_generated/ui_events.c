#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "../network/network_client.h"

// --- 登录/注册 ---
void OnLoginClicked(lv_event_t * e) {
    const char* u = lv_textarea_get_text(ui_InputUser);
    const char* p = lv_textarea_get_text(ui_InputPass);
    if(strlen(u)==0 || strlen(p)==0) {
        printf("[UI] Login: User/Pass empty\n");
        return;
    }
    net_send_login(u, p);
}

void OnRegisterClicked(lv_event_t * e) {
    const char* u = lv_textarea_get_text(ui_RegUser);
    const char* p = lv_textarea_get_text(ui_RegPass);
    const char* p_confirm = lv_textarea_get_text(ui_RegPassConfirm);

    printf("[Debug] Register Clicked: %s, %s, %s\n", u, p, p_confirm);

    // 1. 判空
    if(strlen(u) == 0 || strlen(p) == 0) {
        printf("[UI] Error: Username or Password empty\n");
        return;
    }

    // 2. 检查两次密码是否一致
    if(strcmp(p, p_confirm) != 0) {
        printf("[UI] Error: Password confirm mismatch!\n");
        // 这里如果 UI 有错误提示Label，可以在这里设置 text
        if(ui_PanelRegFail) {
             lv_obj_clear_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
             // 如果有 ui_Label12 是显示错误信息的，可以在这改内容
             // lv_label_set_text(ui_Label12, "Password Mismatch!"); 
        }
        return;
    }

    // 3. 发送注册请求
    net_send_register(u, p);
}

void OnLogoutClicked(lv_event_t * e) {
    net_send_logout();
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLogin_screen_init);
}

// --- 大厅 ---
void OnRefreshClicked(lv_event_t * e) {
    net_send_get_room_list();
}
void OnCreateRoomClicked(lv_event_t * e) {
    net_send_create_room();
}

// --- 房间准备 ---
void OnReadyClicked(lv_event_t * e) {
    net_send_ready();
}
void OnLeaveRoomClicked(lv_event_t * e) {
    net_send_leave_room();
    // 切回大厅并刷新
    _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLobby_screen_init);
    net_send_get_room_list();
}

// --- 游戏 ---
void OnExitGameClicked(lv_event_t * e) {
    // 认输/退出其实也是离开房间
    net_send_leave_room();
    _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLobby_screen_init);
    net_send_get_room_list();
}