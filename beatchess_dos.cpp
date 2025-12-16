/*
 * beatchess_dos.cpp - BeatChess DOS/Allegro 4 Implementation
 * Fixed version with AI support and proper memory management
 */

#include <allegro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
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
    bool ai_vs_ai;
    bool player_is_white;
    
    /* AI state */
    int ai_move_delay;
    int ai_move_counter;
    bool ai_thinking;
    ChessMove ai_best_move;
    int ai_search_depth;
    
} ChessGUI;

ChessGUI chess_gui;

/* Forward declarations */
extern int chess_get_all_moves(ChessGameState *game, ChessColor color, ChessMove *moves);
extern int chess_evaluate_position(ChessGameState *game);
extern bool chess_is_in_check(ChessGameState *game, ChessColor color);
extern int chess_minimax(ChessGameState *game, int depth, int alpha, int beta, bool maximizing);

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

static void cleanup_chess_game() {
    if (chess_gui.history) {
        free(chess_gui.history);
        chess_gui.history = NULL;
    }
}

static void init_chess_game() {
    chess_init_board(&chess_gui.game);
    chess_gui.selected_row = -1;
    chess_gui.selected_col = -1;
    chess_gui.piece_selected = false;
    chess_gui.show_help = false;
    chess_gui.ai_move_counter = 0;
    chess_gui.ai_move_delay = 15;  /* Frames to wait before AI moves */
    chess_gui.ai_thinking = false;
    chess_gui.ai_search_depth = 0;
    
    /* Free old history if exists */
    if (chess_gui.history) {
        free(chess_gui.history);
    }
    
    /* Initialize history */
    chess_gui.history_capacity = 200;  /* Increased from 100 */
    chess_gui.history = (ChessGameState *)malloc(sizeof(ChessGameState) * chess_gui.history_capacity);
    if (!chess_gui.history) {
        printf("ERROR: Failed to allocate history buffer\n");
        exit(1);
    }
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
    
    /* Define custom colors for chess board (green and cream/beige) */
    #define LIGHT_SQUARE 15   /* Light beige/cream color */
    #define DARK_SQUARE 46    /* Dark green color */
    
    /* Draw chess board - 50 pixels per square */
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int screen_x = BOARD_START_X + x * SQUARE_SIZE;
            int screen_y = BOARD_START_Y + y * SQUARE_SIZE;
            
            /* Alternate colors - light beige on even squares, dark green on odd */
            if ((x + y) % 2 == 0) {
                color = LIGHT_SQUARE;  /* Light beige */
            } else {
                color = DARK_SQUARE;   /* Dark green */
            }
            
            /* Highlight selected square */
            if (chess_gui.piece_selected && chess_gui.selected_row == y && chess_gui.selected_col == x) {
                color = COLOR_YELLOW;  /* Yellow highlight for selected piece */
            }
            
            rectfill(screen, screen_x, screen_y, screen_x + SQUARE_SIZE - 1, screen_y + SQUARE_SIZE - 1, color);
            
            /* Draw subtle border */
            int border_color = ((x + y) % 2 == 0) ? DARK_SQUARE : LIGHT_SQUARE;
            rect(screen, screen_x, screen_y, screen_x + SQUARE_SIZE - 1, screen_y + SQUARE_SIZE - 1, border_color);
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

/* ============================================================================
 * Geometric Piece Drawing Functions
 * ============================================================================
 */

static void draw_filled_circle(int cx, int cy, int radius, int color) {
    /* Midpoint circle algorithm */
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    
    while (x <= y) {
        /* Draw 8 symmetric points */
        line(screen, cx - x, cy - y, cx + x, cy - y, color);
        line(screen, cx - y, cy - x, cx + y, cy - x, color);
        line(screen, cx - x, cy + y, cx + x, cy + y, color);
        line(screen, cx - y, cy + x, cx + y, cy + x, color);
        
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static void draw_piece_pawn(int cx, int cy, int size, int color) {
    /* Circle on small rectangle */
    int s = size / 5;
    
    /* Head (circle) */
    draw_filled_circle(cx, cy - s, s / 2, color);
    
    /* Body (rectangle) */
    rectfill(screen, cx - s/2, cy, cx + s/2, cy + s, color);
}

static void draw_piece_knight(int cx, int cy, int size, int color) {
    /* Blocky horse head */
    int s = size / 5;
    
    /* Neck */
    rectfill(screen, cx - s/3, cy, cx + s/3, cy + 2*s, color);
    
    /* Head */
    rectfill(screen, cx - s/4, cy - 2*s, cx + 3*s/4, cy - s, color);
    
    /* Snout */
    rectfill(screen, cx + s/2, cy - s, cx + 3*s/2, cy - s/2, color);
}

static void draw_piece_bishop(int cx, int cy, int size, int color) {
    /* Triangle with circle on top */
    int s = size / 5;
    
    /* Base (filled triangle using lines) */
    int base_left = cx - s;
    int base_right = cx + s;
    int base_bottom = cy + 2*s;
    
    for (int y = cy - 2*s; y <= base_bottom; y++) {
        int progress = (y - (cy - 2*s));
        int height = base_bottom - (cy - 2*s);
        int left = cx - (s * progress) / height;
        int right = cx + (s * progress) / height;
        line(screen, left, y, right, y, color);
    }
    
    /* Top circle */
    draw_filled_circle(cx, cy - 2*s, s/2, color);
}

static void draw_piece_rook(int cx, int cy, int size, int color) {
    /* Castle tower with crenellations */
    int s = size / 5;
    
    /* Main body */
    rectfill(screen, cx - 3*s/2, cy - s/2, cx + 3*s/2, cy + 2*s, color);
    
    /* Crenellations (three tower tops) */
    rectfill(screen, cx - 3*s/2, cy - 5*s/2, cx - s/2, cy - s, color);
    rectfill(screen, cx - s/4, cy - 5*s/2, cx + s/4, cy - s, color);
    rectfill(screen, cx + s/2, cy - 5*s/2, cx + 3*s/2, cy - s, color);
}

static void draw_piece_queen(int cx, int cy, int size, int color) {
    /* Crown with multiple points and center ball */
    int s = size / 5;
    
    /* Crown body (trapezoid) */
    int crown_top = cy - 3*s;
    int crown_base = cy + s;
    
    for (int y = crown_top; y <= crown_base; y++) {
        int progress = (y - crown_top);
        int total = crown_base - crown_top;
        int width = s + (2*s * progress) / total;
        line(screen, cx - width, y, cx + width, y, color);
    }
    
    /* Center peak */
    line(screen, cx - s/2, crown_top, cx, crown_top - s, color);
    line(screen, cx + s/2, crown_top, cx, crown_top - s, color);
    
    /* Center ball */
    draw_filled_circle(cx, crown_top - s, s/2, color);
}

static void draw_piece_king(int cx, int cy, int size, int color) {
    /* Crown with cross on top */
    int s = size / 5;
    
    /* Main body */
    rectfill(screen, cx - 3*s/2, cy - s/2, cx + 3*s/2, cy + 2*s, color);
    
    /* Vertical cross bar */
    rectfill(screen, cx - s/4, cy - 5*s/2, cx + s/4, cy - s/2, color);
    
    /* Horizontal cross bar */
    rectfill(screen, cx - 3*s/4, cy - 7*s/4, cx + 3*s/4, cy - 5*s/4, color);
}

static void draw_piece_outline(int cx, int cy, int size, PieceType type, int color) {
    /* Draw simple outline based on piece type */
    int s = size / 5;
    
    switch (type) {
        case PAWN:
            circle(screen, cx, cy - s, s/2, color);
            rect(screen, cx - s/2, cy, cx + s/2, cy + s, color);
            break;
        case KNIGHT:
            rect(screen, cx - s/3, cy, cx + s/3, cy + 2*s, color);
            rect(screen, cx - s/4, cy - 2*s, cx + 3*s/4, cy - s, color);
            break;
        case BISHOP:
            circle(screen, cx, cy - 2*s, s/2, color);
            break;
        case ROOK:
            rect(screen, cx - 3*s/2, cy - s/2, cx + 3*s/2, cy + 2*s, color);
            break;
        case QUEEN:
            circle(screen, cx, cy - 3*s - s/2, s/2, color);
            break;
        case KING:
            rect(screen, cx - 3*s/2, cy - s/2, cx + 3*s/2, cy + 2*s, color);
            break;
        default:
            break;
    }
}

static void draw_piece_at_square(int x, int y, ChessPiece piece) {
    if (piece.type == EMPTY) return;
    
    #define BOARD_START_X 60
    #define BOARD_START_Y 60
    #define SQUARE_SIZE 50
    
    int screen_x = BOARD_START_X + x * SQUARE_SIZE + SQUARE_SIZE / 2;
    int screen_y = BOARD_START_Y + y * SQUARE_SIZE + SQUARE_SIZE / 2;
    int piece_size = SQUARE_SIZE - 10;
    
    int piece_color = (piece.color == WHITE) ? COLOR_WHITE : COLOR_BLACK;
    int outline_color = (piece.color == WHITE) ? COLOR_BLACK : COLOR_WHITE;
    
    switch (piece.type) {
        case PAWN:
            draw_piece_pawn(screen_x, screen_y, piece_size, piece_color);
            break;
        case KNIGHT:
            draw_piece_knight(screen_x, screen_y, piece_size, piece_color);
            break;
        case BISHOP:
            draw_piece_bishop(screen_x, screen_y, piece_size, piece_color);
            break;
        case ROOK:
            draw_piece_rook(screen_x, screen_y, piece_size, piece_color);
            break;
        case QUEEN:
            draw_piece_queen(screen_x, screen_y, piece_size, piece_color);
            break;
        case KING:
            draw_piece_king(screen_x, screen_y, piece_size, piece_color);
            break;
        default:
            break;
    }
    
    /* Draw outline */
    draw_piece_outline(screen_x, screen_y, piece_size, piece.type, outline_color);
}

static void draw_pieces() {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            ChessPiece piece = chess_gui.game.board[y][x];
            draw_piece_at_square(x, y, piece);
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
    textout_ex(screen, font, turn_str, 200, 480, COLOR_CYAN, -1);
    
    /* Draw mode */
    const char *mode_str = chess_gui.ai_vs_ai ? "AI vs AI" : 
                          (chess_gui.player_is_white ? "Player(W) vs AI(B)" : "AI(W) vs Player(B)");
    textout_ex(screen, font, mode_str, 380, 480, COLOR_GREEN, -1);
    
    /* Draw AI thinking indicator */
    if (chess_gui.ai_thinking) {
        sprintf(buf, "AI thinking... (depth %d)", chess_gui.ai_search_depth);
        textout_ex(screen, font, buf, 60, 495, COLOR_MAGENTA, -1);
    }
    
    textout_ex(screen, font, "U=Undo N=New A=AI/PvA B=Color ?=Help Q=Quit", 60, 510, COLOR_WHITE, -1);
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
    textout_ex(screen, font, "A - Toggle AI vs AI / Player vs AI", 80, y, COLOR_WHITE, -1);
    y += 10;
    textout_ex(screen, font, "B - Change player color (PvA mode)", 80, y, COLOR_WHITE, -1);
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
    } else {
        printf("WARNING: History buffer full!\n");
    }
}

static void undo_move() {
    if (chess_gui.history_size > 1) {
        chess_gui.history_size--;
        chess_gui.game = chess_gui.history[chess_gui.history_size - 1];
    }
}

/* ============================================================================
 * AI Logic - Simplified for DOS (no threading)
 * ============================================================================
 */

static ChessMove compute_ai_move() {
    ChessMove moves[256];
    int move_count = chess_get_all_moves(&chess_gui.game, chess_gui.game.turn, moves);
    
    if (move_count == 0) {
        /* No legal moves - return dummy move */
        ChessMove dummy = {0, 0, 0, 0, 0};
        return dummy;
    }
    
    /* Simple iterative deepening up to depth 3 for DOS */
    ChessMove best_move = moves[0];
    
    for (int depth = 1; depth <= 3; depth++) {
        chess_gui.ai_search_depth = depth;
        
        ChessMove depth_best_moves[256];
        int best_move_count = 0;
        int best_score = (chess_gui.game.turn == WHITE) ? INT_MIN : INT_MAX;
        
        for (int i = 0; i < move_count; i++) {
            ChessGameState temp = chess_gui.game;
            chess_make_move(&temp, moves[i]);
            int score = chess_minimax(&temp, depth - 1, INT_MIN, INT_MAX, 
                                     chess_gui.game.turn == BLACK);
            
            if (chess_gui.game.turn == WHITE) {
                if (score > best_score) {
                    best_score = score;
                    depth_best_moves[0] = moves[i];
                    best_move_count = 1;
                } else if (score == best_score && best_move_count < 256) {
                    depth_best_moves[best_move_count++] = moves[i];
                }
            } else {
                if (score < best_score) {
                    best_score = score;
                    depth_best_moves[0] = moves[i];
                    best_move_count = 1;
                } else if (score == best_score && best_move_count < 256) {
                    depth_best_moves[best_move_count++] = moves[i];
                }
            }
        }
        
        if (best_move_count > 0) {
            best_move = depth_best_moves[rand() % best_move_count];
        }
    }
    
    return best_move;
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
    
    /* Set up custom palette colors for better board appearance */
    RGB light_square, dark_square;
    light_square.r = 58;  /* Beige/cream - light square */
    light_square.g = 54;
    light_square.b = 47;
    dark_square.r = 35;   /* Dark green - dark square */
    dark_square.g = 45;
    dark_square.b = 35;
    
    set_color(15, &light_square);  /* Set color 15 to light beige */
    set_color(46, &dark_square);   /* Set color 46 to dark green */
    
    /* Show mouse cursor on screen */
    show_mouse(screen);
    
    /* Create backbuffer for double buffering */
    BITMAP *backbuffer = create_bitmap(640, 480);
    if (!backbuffer) {
        printf("Error creating backbuffer\n");
        allegro_exit();
        return 1;
    }
    
    /* Initialize game */
    init_chess_game();
    chess_gui.ai_vs_ai = false;
    chess_gui.player_is_white = true;
    
    bool running = true;
    int prev_mouse_b = 0;  /* Track previous mouse button state */
    
    /* Game loop */
    while (running) {
        /* Check if it's AI's turn */
        bool ai_should_move = false;
        if (chess_gui.ai_vs_ai) {
            ai_should_move = true;
        } else {
            /* Player vs AI mode */
            ChessColor player_color = chess_gui.player_is_white ? WHITE : BLACK;
            ai_should_move = (chess_gui.game.turn != player_color);
        }
        
        /* AI move logic */
        if (ai_should_move && !chess_gui.ai_thinking) {
            chess_gui.ai_move_counter++;
            
            if (chess_gui.ai_move_counter >= chess_gui.ai_move_delay) {
                chess_gui.ai_thinking = true;
                chess_gui.ai_move_counter = 0;
                
                /* Compute move (blocking in DOS) */
                ChessMove ai_move = compute_ai_move();
                
                /* Validate and make move */
                if (chess_is_valid_move(&chess_gui.game, 
                                       ai_move.from_row, ai_move.from_col,
                                       ai_move.to_row, ai_move.to_col)) {
                    /* Verify move doesn't leave king in check */
                    ChessGameState temp = chess_gui.game;
                    chess_make_move(&temp, ai_move);
                    
                    if (!chess_is_in_check(&temp, chess_gui.game.turn)) {
                        chess_make_move(&chess_gui.game, ai_move);
                        save_position_to_history();
                        /* Turn is switched by chess_make_move */
                    }
                }
                
                chess_gui.ai_thinking = false;
                chess_gui.piece_selected = false;  /* Clear any selection */
            }
        }
        
        /* Draw to backbuffer (mouse cursor is not drawn to backbuffer) */
        BITMAP *prev_target = screen;
        screen = backbuffer;
        
        /* Clear backbuffer */
        clear_to_color(backbuffer, COLOR_BLACK);
        
        /* Draw game (no mouse cursor interference) */
        if (chess_gui.show_help) {
            draw_help_screen();
            chess_gui.show_help = false;
        } else {
            draw_board();
            draw_pieces();
            draw_ui();
        }
        
        /* Restore screen target */
        screen = prev_target;
        
        /* Hardware mouse cursor is automatically shown on screen buffer */
        /* Blit the backbuffer to screen - this happens under the mouse cursor */
        scare_mouse();  /* Temporarily disable mouse drawing */
        blit(backbuffer, screen, 0, 0, 0, 0, 640, 480);
        unscare_mouse();  /* Re-enable mouse drawing */
        
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
                    chess_gui.ai_move_counter = 0;
                    break;
                    
                case 'u':
                case 'U':
                    undo_move();
                    chess_gui.ai_move_counter = 0;
                    break;
                    
                case 'a':
                case 'A':
                    chess_gui.ai_vs_ai = !chess_gui.ai_vs_ai;
                    chess_gui.ai_move_counter = 0;
                    break;
                    
                case 'b':
                case 'B':
                    chess_gui.player_is_white = !chess_gui.player_is_white;
                    chess_gui.ai_move_counter = 0;
                    break;
                    
                case '?':
                    chess_gui.show_help = true;
                    break;
            }
        }
        
        /* Handle mouse clicks - only if it's player's turn */
        if (!ai_should_move || chess_gui.ai_vs_ai) {
            /* Allow clicks in AI vs AI to watch, or in player's turn */
        }
        
        if (!ai_should_move && !chess_gui.ai_thinking) {
            poll_mouse();  /* Update mouse state */
            
            if ((mouse_b & 1) && !(prev_mouse_b & 1)) {  /* Left button just pressed */
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
                                /* Verify move doesn't leave king in check */
                                ChessGameState temp = chess_gui.game;
                                ChessMove move = {chess_gui.selected_row, chess_gui.selected_col, 
                                                 row, col, 0};
                                chess_make_move(&temp, move);
                                
                                if (!chess_is_in_check(&temp, chess_gui.game.turn)) {
                                    chess_make_move(&chess_gui.game, move);
                                    save_position_to_history();
                                    /* Turn is switched by chess_make_move */
                                    chess_gui.ai_move_counter = 0;  /* Reset AI timer */
                                }
                            }
                            chess_gui.piece_selected = false;
                        }
                    }
                }
            }
            
            prev_mouse_b = mouse_b;  /* Remember current state for next frame */
        }
        
        /* Small delay to prevent CPU spinning */
        rest(10);  /* 10ms delay */
    }
    
    /* Cleanup */
    destroy_bitmap(backbuffer);
    cleanup_chess_game();
    allegro_exit();
    
    return 0;
}

END_OF_MAIN()
