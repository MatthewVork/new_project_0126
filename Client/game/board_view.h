#ifndef BOARD_VIEW_H
#define BOARD_VIEW_H

#include "../lvgl/lvgl.h"
#include "game_config.h"

// 绘制棋子
void board_view_draw_stone(lv_obj_t *parent, int x, int y, int color);

// 清空棋盘
void board_view_clear(lv_obj_t *parent);

// 获取点击坐标 (返回 true 表示点击有效，x/y 存入指针)
bool board_view_get_click_xy(lv_obj_t *board_obj, int *out_x, int *out_y);

#endif