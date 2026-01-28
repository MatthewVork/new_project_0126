#ifndef BOARD_VIEW_H
#define BOARD_VIEW_H

#include "../lvgl/lvgl.h"
#include "game_config.h"

void board_view_draw_stone(lv_obj_t *parent, int x, int y, int color);
void board_view_clear(lv_obj_t *parent);
int board_view_get_click_xy(lv_obj_t *board_obj, int *out_x, int *out_y);

#endif