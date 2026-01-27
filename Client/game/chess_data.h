#ifndef CHESS_DATA_H
#define CHESS_DATA_H

#define PIECE_EMPTY  0
#define RED_JU       1
#define RED_MA       2
#define RED_XIANG    3
#define RED_SHI      4
#define RED_SHUAI    5
#define RED_PAO      6
#define RED_BING     7
#define BLACK_JU    -1
#define BLACK_MA    -2
#define BLACK_XIANG -3
#define BLACK_SHI   -4
#define BLACK_JIANG -5
#define BLACK_PAO   -6
#define BLACK_ZU    -7

extern int chess_board[10][9];
void init_chess_board();
void render_board_ui();

#endif