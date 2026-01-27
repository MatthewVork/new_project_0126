#include "chess_data.h"
#include "../ui_generated/ui.h"
#include "lvgl/lvgl.h"
#include <stdio.h>

int chess_board[10][9];

const char* get_piece_text(int id) {
    switch(id) {
        case RED_JU: return "车"; case BLACK_JU: return "車";
        case RED_MA: return "马"; case BLACK_MA: return "馬";
        case RED_XIANG: return "相"; case BLACK_XIANG: return "象";
        case RED_SHI: return "仕"; case BLACK_SHI: return "士";
        case RED_SHUAI: return "帅"; case BLACK_JIANG: return "將";
        case RED_PAO: return "炮"; case BLACK_PAO: return "砲";
        case RED_BING: return "兵"; case BLACK_ZU: return "卒";
        default: return "";
    }
}

void init_chess_board() {
    for(int i=0;i<10;i++) for(int j=0;j<9;j++) chess_board[i][j]=PIECE_EMPTY;
    // 黑
    chess_board[0][0]=BLACK_JU; chess_board[0][1]=BLACK_MA; chess_board[0][2]=BLACK_XIANG;
    chess_board[0][3]=BLACK_SHI; chess_board[0][4]=BLACK_JIANG; chess_board[0][5]=BLACK_SHI;
    chess_board[0][6]=BLACK_XIANG; chess_board[0][7]=BLACK_MA; chess_board[0][8]=BLACK_JU;
    chess_board[2][1]=BLACK_PAO; chess_board[2][7]=BLACK_PAO;
    chess_board[3][0]=BLACK_ZU; chess_board[3][2]=BLACK_ZU; chess_board[3][4]=BLACK_ZU;
    chess_board[3][6]=BLACK_ZU; chess_board[3][8]=BLACK_ZU;
    // 红
    chess_board[9][0]=RED_JU; chess_board[9][1]=RED_MA; chess_board[9][2]=RED_XIANG;
    chess_board[9][3]=RED_SHI; chess_board[9][4]=RED_SHUAI; chess_board[9][5]=RED_SHI;
    chess_board[9][6]=RED_XIANG; chess_board[9][7]=RED_MA; chess_board[9][8]=RED_JU;
    chess_board[7][1]=RED_PAO; chess_board[7][7]=RED_PAO;
    chess_board[6][0]=RED_BING; chess_board[6][2]=RED_BING; chess_board[6][4]=RED_BING;
    chess_board[6][6]=RED_BING; chess_board[6][8]=RED_BING;
}

void render_board_ui() {
    lv_obj_clean(ui_PanelBoard); 
    int cell_w = 40, cell_h = 40;
    for(int y=0; y<10; y++) {
        for(int x=0; x<9; x++) {
            int piece = chess_board[y][x];
            if(piece == PIECE_EMPTY) continue;
            
            lv_obj_t * btn = lv_btn_create(ui_PanelBoard);
            lv_obj_set_size(btn, 36, 36);
            lv_obj_set_pos(btn, x * cell_w + 2, y * cell_h + 2);
            lv_obj_set_style_radius(btn, 50, 0);
            
            if (piece > 0) { // 红
                lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFEEEE), 0);
                lv_obj_set_style_text_color(btn, lv_color_hex(0xDD0000), 0);
            } else { // 黑
                lv_obj_set_style_bg_color(btn, lv_color_hex(0xEEEEEE), 0);
                lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
            }
            
            lv_obj_t * label = lv_label_create(btn);
            lv_label_set_text(label, get_piece_text(piece));
            lv_obj_center(label);
            
            int pos_id = y * 10 + x;
            lv_obj_set_user_data(btn, (void*)(intptr_t)pos_id);
        }
    }
}