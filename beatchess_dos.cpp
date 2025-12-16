/*
 * beatchess_dos.cpp - BeatChess DOS/Allegro 4 Implementation
 * Fixed version with proper Allegro 4 compatibility
 */

#include <allegro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "beatchess.h"

/* ============================================================================
 * Color definitions for Allegro 4
 * ============================================================================
 */

#define COLOR_BLACK     0
#define COLOR_BLUE      1
#define COLOR_GREEN     2
#define COLOR_CYAN      3
#define COLOR_RED       4
#define COLOR_MAGENTA   5
#define COLOR_YELLOW    6
#define COLOR_WHITE     7
#define COLOR_GRAY      8

/* ============================================================================
 * Global game state
 * ============================================================================
 */

typedef struct {
    BeatChessVisualization *visualization;
    ChessGameState game;
    
    /* Board state */
    int selected_row, selected_col;
    bool piece_selected;
    
    /* History */
    ChessGameState *history;
    int history_size;
    int history_capacity;
    
    /* UI state */
    bool show_help;
    int ai_move_delay;
    int ai_move_counter;
    
} ChessGUI;

ChessGUI chess_gui;

/* ============================================================================
 * Helper functions
 * ============================================================================
 */

static void draw_text(int x, int y, int color, const char *text) {
    textout_ex(screen, font, text, x, y, color, -1);
}

static void draw_text_center(int x, int y, int color, const char *text) {
    int len = strlen(text) * 8;  /* Approximate width */
    textout_ex(screen, font, text, x - len/2, y, color, -1);
}

static void init_chess_game() {
    chess_init_board(&chess_gui.game);
    chess_gui.selected_row = -1;
    chess_gui.selected_col = -1;
    chess_gui.piece_selected = false;
    chess_gui.show_help = false;
    chess_gui.ai_move_counter = 0;
    chess_gui.ai_move_delay = 30;
    
    /* Initialize history */
    chess_gui.history_capacity = 100;
    chess_gui.history = (ChessGameState *)malloc(sizeof(ChessGameState) * chess_gui.history_capacity);
    chess_gui.history_size = 0;
    
    /* Save initial position */
    if (chess_gui.history_size < chess_gui.history_capacity) {
        chess_gui.history[chess_gui.history_size++] = chess_gui.game;
    }
}

static void draw_board() {
    int x, y, color;
    
    #define BOARD_START_X 60
    #define BOARD_START_Y 60
    #define SQUARE_SIZE 50
    
    /* Draw chess board - 50 pixels per square */
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int screen_x = BOARD_START_X + x * SQUARE_SIZE;
            int screen_y = BOARD_START_Y + y * SQUARE_SIZE;
            
            /* Alternate colors - light on even squares */
            if ((x + y) % 2 == 0) {
                color = 7;  /* Light (White) */
            } else {
                color = 0;  /* Dark (Black) */
            }
            
            rectfill(screen, screen_x, screen_y, screen_x + SQUARE_SIZE - 1, screen_y + SQUARE_SIZE - 1, color);
            
            /* Draw border */
            rect(screen, screen_x, screen_y, screen_x + SQUARE_SIZE - 1, screen_y + SQUARE_SIZE - 1, COLOR_WHITE);
        }
    }
    
    /* Draw file labels (A-H) */
    const char *files = "ABCDEFGH";
    for (x = 0; x < 8; x++) {
        char buf[2] = {files[x], '\0'};
        textout_ex(screen, font, buf, BOARD_START_X + 18 + x * SQUARE_SIZE, 35, COLOR_WHITE, -1);
    }
    
    /* Draw rank labels (8-1) */
    for (y = 0; y < 8; y++) {
        char buf[2];
        sprintf(buf, "%d", 8 - y);
        textout_ex(screen, font, buf, 35, BOARD_START_Y + 15 + y * SQUARE_SIZE, COLOR_WHITE, -1);
    }
}

static void draw_pieces() {
    int x, y, color;
    char piece_char;
    
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            ChessPiece piece = chess_gui.game.board[y][x];
            if (piece.type == EMPTY) continue;
            
            int screen_x = BOARD_START_X + 15 + x * SQUARE_SIZE;
            int screen_y = BOARD_START_Y + 15 + y * SQUARE_SIZE;
            
            /* Determine piece character and color */
            if (piece.color == WHITE) {
                color = COLOR_WHITE;
            } else {
                color = COLOR_YELLOW;
            }
            
            switch (piece.type) {
                case PAWN: piece_char = 'P'; break;
                case KNIGHT: piece_char = 'N'; break;
                case BISHOP: piece_char = 'B'; break;
                case ROOK: piece_char = 'R'; break;
                case QUEEN: piece_char = 'Q'; break;
                case KING: piece_char = 'K'; break;
                default: piece_char = '?'; break;
            }
            
            char buf[2];
            buf[0] = piece_char;
            buf[1] = '\0';
            textout_ex(screen, font, buf, screen_x, screen_y, color, -1);
        }
    }
}

static void draw_ui() {
    char buf[256];
    
    /* Draw status */
    sprintf(buf, "Move: %d", chess_gui.history_size);
    textout_ex(screen, font, buf, 60, 480, COLOR_YELLOW, -1);
    
    /* Draw whose turn it is */
    const char *turn_str = (chess_gui.game.turn == WHITE) ? "White to move" : "Black to move";
    textout_ex(screen, font, turn_str, 60, 495, COLOR_CYAN, -1);
    
    textout_ex(screen, font, "U=Undo N=New ?=Help Q=Quit", 60, 510, COLOR_WHITE, -1);
}

static void draw_help_screen() {
    int y = 50;
    
    rectfill(screen, 0, 0, 640, 480, COLOR_BLACK);
    
    textout_ex(screen, font, "=== BEATCHESS DOS HELP ===", 160, y, COLOR_YELLOW, -1);
    y += 20;
    
    textout_ex(screen, font, "Click on a piece to select it", 80, y, COLOR_WHITE, -1);
    y += 10;
    textout_ex(screen, font, "Click on destination to move", 80, y, COLOR_WHITE, -1);
    y += 10;
    
    textout_ex(screen, font, "Controls:", 80, y, COLOR_YELLOW, -1);
    y += 20;
    
    textout_ex(screen, font, "U - Undo last move", 80, y, COLOR_WHITE, -1);
    y += 10;
    textout_ex(screen, font, "N - New game", 80, y, COLOR_WHITE, -1);
    y += 10;
    textout_ex(screen, font, "F - Flip board", 80, y, COLOR_WHITE, -1);
    y += 10;
    textout_ex(screen, font, "A - AI vs AI mode", 80, y, COLOR_WHITE, -1);
    y += 10;
    textout_ex(screen, font, "B - Change player color", 80, y, COLOR_WHITE, -1);
    y += 10;
    textout_ex(screen, font, "? - Show this help", 80, y, COLOR_WHITE, -1);
    y += 10;
    textout_ex(screen, font, "Q - Quit game", 80, y, COLOR_WHITE, -1);
    y += 20;
    
    textout_ex(screen, font, "Press any key to continue...", 120, y, COLOR_CYAN, -1);
    
    readkey();  /* Wait for key press */
}

static void save_position_to_history() {
    if (chess_gui.history_size < chess_gui.history_capacity) {
        chess_gui.history[chess_gui.history_size++] = chess_gui.game;
    }
}

static void undo_move() {
    if (chess_gui.history_size > 1) {
        chess_gui.history_size--;
        chess_gui.game = chess_gui.history[chess_gui.history_size - 1];
    }
}

/* ============================================================================
 * Main game loop
 * ============================================================================
 */

int main() {
    /* Initialize Allegro */
    if (allegro_init() != 0) {
        printf("Error initializing Allegro\n");
        return 1;
    }
    
    install_keyboard();
    install_mouse();
    install_timer();
    
    /* Set graphics mode: 640x480 VGA */
    set_color_depth(8);  /* 8-bit color (256 colors) */
    if (set_gfx_mode(GFX_AUTODETECT, 640, 480, 0, 0) != 0) {
        printf("Error setting graphics mode\n");
        return 1;
    }
    
    /* Create backbuffer for double buffering */
    BITMAP *backbuffer = create_bitmap(640, 480);
    if (!backbuffer) {
        printf("Error creating backbuffer\n");
        allegro_exit();
        return 1;
    }
    
    /* Initialize game */
    init_chess_game();
    
    bool running = true;
    bool ai_vs_ai = false;
    bool white_to_move = true;
    bool player_is_white = true;
    
    /* Game loop */
    while (running) {
        /* Draw to backbuffer instead of screen */
        BITMAP *prev_target = screen;
        screen = backbuffer;
        
        /* Clear backbuffer */
        clear_to_color(backbuffer, COLOR_BLACK);
        
        /* Draw game */
        if (chess_gui.show_help) {
            draw_help_screen();
            chess_gui.show_help = false;
        } else {
            draw_board();
            draw_pieces();
            draw_ui();
        }
        
        /* Flip buffers - blit backbuffer to screen */
        screen = prev_target;
        blit(backbuffer, screen, 0, 0, 0, 0, 640, 480);
        
        /* Handle input */
        if (keypressed()) {
            int key = readkey();
            int key_code = key & 0xFF;
            
            switch (key_code) {
                case 'q':
                case 'Q':
                case 27:  /* ESC */
                    running = false;
                    break;
                    
                case 'n':
                case 'N':
                    init_chess_game();
                    white_to_move = true;
                    break;
                    
                case 'u':
                case 'U':
                    undo_move();
                    white_to_move = !white_to_move;
                    break;
                    
                case 'f':
                case 'F':
                    /* Flip board (would need additional logic) */
                    break;
                    
                case 'a':
                case 'A':
                    ai_vs_ai = !ai_vs_ai;
                    break;
                    
                case 'b':
                case 'B':
                    player_is_white = !player_is_white;
                    break;
                    
                case '?':
                    chess_gui.show_help = true;
                    break;
            }
        }
        
        /* Handle mouse clicks */
        if (mouse_b & 1) {  /* Left mouse button */
            int mx = mouse_x;
            int my = mouse_y;
            
            /* Convert to board coordinates */
            if (mx >= BOARD_START_X && mx < BOARD_START_X + 400 && 
                my >= BOARD_START_Y && my < BOARD_START_Y + 400) {
                
                int col = (mx - BOARD_START_X) / SQUARE_SIZE;
                int row = (my - BOARD_START_Y) / SQUARE_SIZE;
                
                if (col >= 0 && col < 8 && row >= 0 && row < 8) {
                    ChessPiece piece = chess_gui.game.board[row][col];
                    
                    if (!chess_gui.piece_selected && piece.type != EMPTY && 
                        piece.color == chess_gui.game.turn) {
                        /* Select piece */
                        chess_gui.selected_row = row;
                        chess_gui.selected_col = col;
                        chess_gui.piece_selected = true;
                    } else if (chess_gui.piece_selected) {
                        /* Try to move piece */
                        if (chess_is_valid_move(&chess_gui.game,
                                               chess_gui.selected_row,
                                               chess_gui.selected_col,
                                               row, col)) {
                            ChessMove move;
                            move.from_row = chess_gui.selected_row;
                            move.from_col = chess_gui.selected_col;
                            move.to_row = row;
                            move.to_col = col;
                            chess_make_move(&chess_gui.game, move);
                            save_position_to_history();
                            chess_gui.game.turn = (chess_gui.game.turn == WHITE) ? BLACK : WHITE;
                        }
                        chess_gui.piece_selected = false;
                    }
                }
            }
            
            /* Wait for mouse button release */
            while (mouse_b & 1) {
                rest(1);  /* Don't busy-wait */
            }
        }
    }
    
    /* Cleanup */
    destroy_bitmap(backbuffer);
    free(chess_gui.history);
    allegro_exit();
    
    return 0;
}

END_OF_MAIN()
