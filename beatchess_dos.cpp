/*
 * beatchess_dos_enhanced.cpp - BeatChess DOS/Allegro 4 with Menu System
 * Enhanced version with File menu and side buttons
 */

#include <allegro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include "beatchess.h"
#include "chess_pieces.h"
#include "chess_pieces_loader.h"
#include "splashscreen.h"


ChessGUI chess_gui;

/* Forward declarations */
extern int chess_get_all_moves(ChessGameState *game, ChessColor color, ChessMove *moves);
extern int chess_evaluate_position(ChessGameState *game);
extern bool chess_is_in_check(ChessGameState *game, ChessColor color);
extern int chess_minimax(ChessGameState *game, int depth, int alpha, int beta, bool maximizing);

/* ============================================================================
 * UI Definitions
 * ============================================================================
 */

#define BOARD_START_X 60
#define BOARD_START_Y 80
#define SQUARE_SIZE 50
#define BUTTON_PANEL_X 480
#define BUTTON_PANEL_Y 80
#define BUTTON_WIDTH 140
#define BUTTON_HEIGHT 30
#define BUTTON_SPACING 8

/* Menu bar */
#define MENU_BAR_HEIGHT 20
#define MENU_ITEM_WIDTH 80

/* Define custom colors for chess board (green and cream/beige) */
#define LIGHT_SQUARE 15   /* Light beige/cream color */
#define DARK_SQUARE 46    /* Dark green color */


/* Side panel buttons */
Button side_buttons[] = {
    {BUTTON_PANEL_X, BUTTON_PANEL_Y + 0, BUTTON_WIDTH, BUTTON_HEIGHT, "New Game (N)", 'N', true},
    {BUTTON_PANEL_X, BUTTON_PANEL_Y + 38, BUTTON_WIDTH, BUTTON_HEIGHT, "Undo (U)", 'U', true},
    {BUTTON_PANEL_X, BUTTON_PANEL_Y + 76, BUTTON_WIDTH, BUTTON_HEIGHT, "Toggle AI (A)", 'A', true},
    {BUTTON_PANEL_X, BUTTON_PANEL_Y + 114, BUTTON_WIDTH, BUTTON_HEIGHT, "Swap Color (B)", 'B', true},
    {BUTTON_PANEL_X, BUTTON_PANEL_Y + 152, BUTTON_WIDTH, BUTTON_HEIGHT, "Help (?)", '?', true},
    {BUTTON_PANEL_X, BUTTON_PANEL_Y + 190, BUTTON_WIDTH, BUTTON_HEIGHT, "Quit (Q)", 'Q', true},
};

#define NUM_BUTTONS (sizeof(side_buttons) / sizeof(side_buttons[0]))

/* Menu items */
const char *menu_items[] = {
    "New Game     N",
    "Undo Move    U",
    "",  /* separator */
    "AI vs AI     A",
    "Swap Color   B",
    "",  /* separator */
    "Help         ?",
    "About...",
    "",  /* separator */
    "Quit         Q"
};

#define NUM_MENU_ITEMS (sizeof(menu_items) / sizeof(menu_items[0]))

void init_chess_gui(void) {
    chess_gui.game.turn = WHITE;
    chess_gui.game.white_king_moved = false;
    chess_gui.game.black_king_moved = false;
    chess_gui.game.white_rook_a_moved = false;
    chess_gui.game.white_rook_h_moved = false;
    chess_gui.game.black_rook_a_moved = false;
    chess_gui.game.black_rook_h_moved = false;
    chess_gui.game.en_passant_col = -1;
    chess_gui.game.en_passant_row = -1;
    
    chess_gui.selected_row = -1;
    chess_gui.selected_col = -1;
    chess_gui.piece_selected_row = -1;
    chess_gui.piece_selected_col = -1;
    chess_gui.piece_selected = false;
    
    chess_gui.last_move_from_row = -1;
    chess_gui.last_move_from_col = -1;
    chess_gui.last_move_to_row = -1;
    chess_gui.last_move_to_col = -1;
    chess_gui.has_last_move = false;
    
    chess_gui.show_help = false;
    chess_gui.show_about = false;
    chess_gui.show_menu = false;
    chess_gui.menu_selected = -1;
    
    chess_gui.ai_vs_ai = false;
    chess_gui.player_is_white = true;
    chess_gui.ai_move_delay = 15;
    chess_gui.ai_move_counter = 0;
    chess_gui.ai_thinking = false;
    chess_gui.ai_computing = false;
    chess_gui.ai_search_depth = 3;
    chess_gui.ai_total_moves = 0;
    chess_gui.ai_evaluated_moves = 0;
    
    chess_gui.ai_best_move.from_row = -1;
    chess_gui.ai_best_move.from_col = -1;
    chess_gui.ai_best_move.to_row = -1;
    chess_gui.ai_best_move.to_col = -1;
    chess_gui.ai_best_move.score = 0;
    
    chess_gui.white_time_seconds = 0;
    chess_gui.white_time_frames = 0;
    chess_gui.black_time_seconds = 0;
    chess_gui.black_time_frames = 0;
    chess_gui.timer_started = false;
    chess_gui.ai_thinking_start_time = 0;
    
    chess_gui.is_in_check = false;
    chess_gui.check_display_timer = 0;
    chess_gui.is_checkmate = false;
    chess_gui.is_stalemate = false;
    
    chess_gui.history = NULL;
    chess_gui.history_size = 0;
    chess_gui.history_capacity = 0;
    
    chess_init_board(&chess_gui.game);
}

/* ============================================================================
 * Helper functions
 * ============================================================================
 */

void draw_text(int x, int y, int color, const char *text) {
    textout_ex(screen, font, text, x, y, color, -1);
}

void draw_text_center(int x, int y, int color, const char *text) {
    int len = strlen(text) * 8;  /* Approximate width */
    textout_ex(screen, font, text, x - len/2, y, color, -1);
}

bool point_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

void cleanup_chess_game() {
    if (chess_gui.history) {
        free(chess_gui.history);
        chess_gui.history = NULL;
    }
    chess_gui.history_size = 0;
    chess_gui.history_capacity = 0;
}

void init_chess_game() {
    /* CRITICAL: Don't allow new game while AI is computing - this causes crashes! */
    if (chess_gui.ai_computing) {
        return;
    }
    
    /* Save settings that should persist across new games */
    bool saved_ai_vs_ai = chess_gui.ai_vs_ai;
    bool saved_player_is_white = chess_gui.player_is_white;
    ChessGameState *saved_history = chess_gui.history;
    int saved_capacity = chess_gui.history_capacity;
    
    /* DON'T use memset on the entire structure - it's too large and causes issues */
    /* Instead, manually reset only the fields we need to reset */
    
    /* Restore/set saved values first */
    chess_gui.ai_vs_ai = saved_ai_vs_ai;
    chess_gui.player_is_white = saved_player_is_white;
    chess_gui.history = saved_history;
    chess_gui.history_capacity = saved_capacity;
    
    /* Initialize board */
    chess_init_board(&chess_gui.game);
    
    /* Reset UI state */
    chess_gui.selected_row = -1;
    chess_gui.selected_col = -1;
    chess_gui.piece_selected_row = -1;
    chess_gui.piece_selected_col = -1;
    chess_gui.piece_selected = false;
    chess_gui.last_move_from_row = -1;
    chess_gui.last_move_from_col = -1;
    chess_gui.last_move_to_row = -1;
    chess_gui.last_move_to_col = -1;
    chess_gui.has_last_move = false;
    chess_gui.show_help = false;
    chess_gui.show_about = false;
    chess_gui.show_menu = false;
    chess_gui.menu_selected = -1;
    
    /* Reset AI state completely */
    chess_gui.ai_move_counter = 0;
    chess_gui.ai_move_delay = 15;
    chess_gui.ai_thinking = false;
    chess_gui.ai_computing = false;
    chess_gui.ai_search_depth = 0;
    chess_gui.ai_total_moves = 0;
    chess_gui.ai_evaluated_moves = 0;
    chess_gui.ai_best_move.from_row = -1;
    chess_gui.ai_best_move.from_col = -1;
    chess_gui.ai_best_move.to_row = -1;
    chess_gui.ai_best_move.to_col = -1;
    chess_gui.ai_best_move.score = 0;
    
    /* Reset timers */
    chess_gui.white_time_seconds = 0;
    chess_gui.white_time_frames = 0;
    chess_gui.black_time_seconds = 0;
    chess_gui.black_time_frames = 0;
    chess_gui.timer_started = false;
    chess_gui.ai_thinking_start_time = 0;
    
    /* Reset check/checkmate/stalemate display flags */
    chess_gui.is_in_check = false;
    chess_gui.check_display_timer = 0;
    chess_gui.is_checkmate = false;
    chess_gui.is_stalemate = false;
    
    /* Allocate history ONLY if it doesn't exist (first time only) */
    if (!chess_gui.history) {
        chess_gui.history_capacity = 100;
        chess_gui.history = (ChessGameState *)malloc(sizeof(ChessGameState) * chess_gui.history_capacity);
        if (!chess_gui.history) {
            printf("ERROR: Failed to allocate history buffer\n");
            exit(1);
        }
    }
    
    /* Reset history size (reuse existing buffer) */
    chess_gui.history_size = 0;
    
    /* Save initial position */
    chess_gui.history[chess_gui.history_size++] = chess_gui.game;
}

void save_position_to_history() {
    /* Safety check */
    if (!chess_gui.history || chess_gui.history_size >= chess_gui.history_capacity) {
        return;  /* Can't save if no buffer or buffer is full */
    }
    
    chess_gui.history[chess_gui.history_size++] = chess_gui.game;
    
    /* Start timer after white's first move (move 2 in history = after first move) */
    if (chess_gui.history_size == 2) {
        chess_gui.timer_started = true;
    }
}

void undo_move() {
    /* Don't allow undo while AI is computing */
    if (chess_gui.ai_computing) {
        return;
    }
    
    /* In Player vs AI mode, undo TWO moves (AI's move + player's move) 
     * so the player gets their turn back */
    int moves_to_undo = chess_gui.ai_vs_ai ? 1 : 2;
    
    /* Safety check - make sure we have history */
    if (!chess_gui.history || chess_gui.history_size <= 1) {
        return;  /* Can't undo if no history */
    }
    
    for (int i = 0; i < moves_to_undo; i++) {
        if (chess_gui.history_size > 1) {
            chess_gui.history_size--;
            /* Bounds check before accessing array */
            if (chess_gui.history_size > 0 && chess_gui.history_size <= chess_gui.history_capacity) {
                chess_gui.game = chess_gui.history[chess_gui.history_size - 1];
            }
        } else {
            break;  /* Can't undo past the start */
        }
    }
    
    chess_gui.piece_selected = false;
}

/* Global counter for making AI computation interruptible */
static volatile int ai_eval_counter = 0;
#define AI_YIELD_INTERVAL 1000  /* Yield control every N evaluations */

/* Display splash screen and wait for keypress or timeout */
void show_splash_screen(BITMAP *backbuffer) {
    /* Load the splash screen - need to handle 8-bit indexed BMPs differently */
    const unsigned char *data = splashscreen_bmp;
    unsigned int len = splashscreen_bmp_len;
    BITMAP *splash = NULL;
    
    /* Verify BMP signature */
    if (len >= 54 && data[0] == 'B' && data[1] == 'M') {
        int data_offset = data[10] | (data[11] << 8) | (data[12] << 16) | (data[13] << 24);
        int width = data[18] | (data[19] << 8) | (data[20] << 16) | (data[21] << 24);
        int height = data[22] | (data[23] << 8) | (data[24] << 16) | (data[25] << 24);
        int bpp = data[28] | (data[29] << 8);
        
        if (height < 0) height = -height;
        
        if (width > 0 && height > 0 && width <= 2048 && height <= 2048) {
            splash = create_bitmap(width, height);
            
            if (splash) {
                if (bpp == 8) {
                    /* 8-bit indexed color BMP */
                    const unsigned char *palette = data + 54;
                    const unsigned char *pixel_data = data + data_offset;
                    int bytes_per_row = ((width + 3) / 4) * 4;
                    
                    for (int row = 0; row < height; row++) {
                        const unsigned char *row_data = pixel_data + (height - 1 - row) * bytes_per_row;
                        for (int col = 0; col < width; col++) {
                            int palette_index = row_data[col];
                            int b = palette[palette_index * 4 + 0];
                            int g = palette[palette_index * 4 + 1];
                            int r = palette[palette_index * 4 + 2];
                            putpixel(splash, col, row, makecol(r, g, b));
                        }
                    }
                } else if (bpp == 24) {
                    /* 24-bit RGB BMP */
                    const unsigned char *pixel_data = data + data_offset;
                    int bytes_per_row = ((width * 3 + 3) / 4) * 4;
                    
                    for (int row = 0; row < height; row++) {
                        const unsigned char *row_data = pixel_data + (height - 1 - row) * bytes_per_row;
                        for (int col = 0; col < width; col++) {
                            int b = row_data[col * 3 + 0];
                            int g = row_data[col * 3 + 1];
                            int r = row_data[col * 3 + 2];
                            putpixel(splash, col, row, makecol(r, g, b));
                        }
                    }
                }
            }
        }
    }
    
    if (splash) {
        /* Clear screen to black */
        clear_to_color(backbuffer, COLOR_BLACK);
        
        /* Center the splash screen */
        int x = (640 - splash->w) / 2;
        int y = (480 - splash->h) / 2;
        
        /* Draw splash to backbuffer */
        blit(splash, backbuffer, 0, 0, x, y, splash->w, splash->h);
        
        /* Display "Press any key..." message */
        textout_centre_ex(backbuffer, font, "Press any key or click to continue...", 
                         320, 450, COLOR_WHITE, -1);
        
        /* Show the splash screen */
        scare_mouse();
        blit(backbuffer, screen, 0, 0, 0, 0, 640, 480);
        unscare_mouse();
        
        /* Clear any pending keypresses and mouse clicks first */
        clear_keybuf();
        
        /* Wait for keypress, mouse click, or timeout (10 seconds) */
        int timeout = 0;
        int prev_mouse_b = 0;
        while (timeout < 1000) {  /* 1000 frames * 10ms = 10 seconds */
            poll_mouse();
            
            /* Check for keypress */
            if (keypressed()) {
                readkey();  /* Consume the key */
                break;
            }
            
            /* Check for mouse click */
            if ((mouse_b & 1) && !(prev_mouse_b & 1)) {
                break;  /* Left click detected */
            }
            prev_mouse_b = mouse_b;
            
            rest(10);
            timeout++;
        }
        
        /* Cleanup */
        destroy_bitmap(splash);
    } else {
        printf("Warning: Could not load splash screen\n");
        rest(500);  /* Brief pause so user sees the message */
    }
}

/* AI move computation - standard minimax (no negamax) */
static ChessMove compute_ai_move() {
    ChessMove moves[256];
    ChessMove best_move = {-1, -1, -1, -1, 0};
    int num_moves = chess_get_all_moves(&chess_gui.game, chess_gui.game.turn, moves);
    
    if (num_moves == 0) {
        return best_move;
    }
    
    chess_gui.ai_search_depth = 3;  /* Search depth */
    ai_eval_counter = 0;
    
    /* Determine if current player is white or black
     * White maximizes (wants positive scores)
     * Black minimizes (wants negative scores) */
    bool we_are_white = (chess_gui.game.turn == WHITE);
    int best_score = we_are_white ? INT_MIN : INT_MAX;
    
    for (int i = 0; i < num_moves; i++) {
        ChessGameState temp = chess_gui.game;
        chess_make_move(&temp, moves[i]);
        
        /* Skip if move leaves king in check */
        if (chess_is_in_check(&temp, chess_gui.game.turn)) {
            continue;
        }
        
        /* Call minimax with opposite goal for opponent
         * If we're white (maximizing), opponent will be black (minimizing)
         * If we're black (minimizing), opponent will be white (maximizing) */
        bool opponent_is_white = (temp.turn == WHITE);
        int score = chess_minimax(&temp, chess_gui.ai_search_depth - 1, INT_MIN, INT_MAX, opponent_is_white);
        
        /* Update best move based on whether we're maximizing or minimizing */
        if (we_are_white) {
            /* White maximizes */
            if (score > best_score) {
                best_score = score;
                best_move = moves[i];
            }
        } else {
            /* Black minimizes */
            if (score < best_score) {
                best_score = score;
                best_move = moves[i];
            }
        }
        
        /* Update progress */
        chess_gui.ai_evaluated_moves = i + 1;
        chess_gui.ai_total_moves = num_moves;
    }
    
    return best_move;
}

/* ============================================================================
 * Drawing functions
 * ============================================================================
 */

void draw_menu_bar() {
    /* Menu bar background */
    rectfill(screen, 0, 0, 640, MENU_BAR_HEIGHT, COLOR_BLUE);
    
    /* Game menu */
    textout_ex(screen, font, "Game", 5, 5, COLOR_WHITE, -1);
    
    /* Title */
    textout_ex(screen, font, "BeatChess - DOS Edition", 200, 5, COLOR_YELLOW, -1);
    
    /* Draw dropdown menu if active */
    if (chess_gui.show_menu) {
        int menu_x = 0;
        int menu_y = MENU_BAR_HEIGHT;
        int menu_w = 200;  /* Increased width */
        int item_h = 20;
        int menu_h = NUM_MENU_ITEMS * item_h;
        
        /* Menu background with border */
        rectfill(screen, menu_x, menu_y, menu_x + menu_w, menu_y + menu_h, COLOR_BLUE);
        rect(screen, menu_x, menu_y, menu_x + menu_w, menu_y + menu_h, COLOR_WHITE);
        
        /* Menu items */
        for (int i = 0; i < NUM_MENU_ITEMS; i++) {
            int item_y = menu_y + i * item_h;
            
            /* Separator */
            if (strlen(menu_items[i]) == 0) {
                hline(screen, menu_x + 5, item_y + item_h/2, menu_x + menu_w - 5, COLOR_GRAY);
                continue;
            }
            
            /* Highlight selected */
            if (i == chess_gui.menu_selected) {
                rectfill(screen, menu_x + 2, item_y + 2, 
                        menu_x + menu_w - 2, item_y + item_h - 2, COLOR_CYAN);
            }
            
            /* Draw menu item text */
            int text_color = (i == chess_gui.menu_selected) ? COLOR_BLACK : COLOR_WHITE;
            textout_ex(screen, font, menu_items[i], menu_x + 10, item_y + 5, text_color, -1);
        }
    }
}

void draw_button(Button *btn, bool hover) {
    int bg_color = btn->enabled ? (hover ? COLOR_CYAN : COLOR_BLUE) : COLOR_GRAY;
    int text_color = btn->enabled ? (hover ? COLOR_BLACK : COLOR_WHITE) : COLOR_BLACK;
    
    /* Button background */
    rectfill(screen, btn->x, btn->y, btn->x + btn->w, btn->y + btn->h, bg_color);
    
    /* Button border */
    rect(screen, btn->x, btn->y, btn->x + btn->w, btn->y + btn->h, COLOR_WHITE);
    
    /* Button text - centered */
    int text_w = strlen(btn->label) * 8;
    int text_x = btn->x + (btn->w - text_w) / 2;
    int text_y = btn->y + (btn->h - 8) / 2;
    textout_ex(screen, font, btn->label, text_x, text_y, text_color, -1);
}

void draw_side_panel() {
    /* Panel background - don't draw over the board */
    rectfill(screen, BUTTON_PANEL_X - 10, MENU_BAR_HEIGHT, 
             640, 480, COLOR_BLACK);
    
    /* Panel title */
    textout_ex(screen, font, "Controls", BUTTON_PANEL_X + 35, MENU_BAR_HEIGHT + 5, 
               COLOR_YELLOW, -1);
    
    /* Draw buttons */
    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool hover = point_in_rect(mouse_x, mouse_y, 
                                   side_buttons[i].x, side_buttons[i].y,
                                   side_buttons[i].w, side_buttons[i].h);
        draw_button(&side_buttons[i], hover);
    }
    
    /* Game info panel */
    int info_y = BUTTON_PANEL_Y + 250;
    textout_ex(screen, font, "Game Info:", BUTTON_PANEL_X, info_y, COLOR_YELLOW, -1);
    
    char buf[64];
    snprintf(buf, 64, "Move: %d", chess_gui.history_size);
    textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 20, COLOR_WHITE, -1);
    
    const char *turn_str = (chess_gui.game.turn == WHITE) ? "White" : "Black";
    snprintf(buf, 64, "Turn: %s", turn_str);
    textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 35, COLOR_WHITE, -1);
    
    const char *mode_str = chess_gui.ai_vs_ai ? "AI vs AI" : 
                          (chess_gui.player_is_white ? "Player vs AI" : "AI vs Player");
    snprintf(buf, 64, "Mode: %s", mode_str);
    textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 50, COLOR_WHITE, -1);
    
    if (!chess_gui.ai_vs_ai) {
        const char *player_color = chess_gui.player_is_white ? "White" : "Black";
        snprintf(buf, 64, "You: %s", player_color);
        textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 65, COLOR_GREEN, -1);
    }
    
    /* Display timers */
    int timer_y = info_y + 85;
    textout_ex(screen, font, "Time Elapsed:", BUTTON_PANEL_X, timer_y, COLOR_YELLOW, -1);
    
    /* White time */
    int white_mins = chess_gui.white_time_seconds / 60;
    int white_secs = chess_gui.white_time_seconds % 60;
    snprintf(buf, 64, "White: %d:%02d", white_mins, white_secs);
    textout_ex(screen, font, buf, BUTTON_PANEL_X, timer_y + 15, COLOR_WHITE, -1);
    
    /* Black time */
    int black_mins = chess_gui.black_time_seconds / 60;
    int black_secs = chess_gui.black_time_seconds % 60;
    snprintf(buf, 64, "Black: %d:%02d", black_mins, black_secs);
    textout_ex(screen, font, buf, BUTTON_PANEL_X, timer_y + 30, COLOR_WHITE, -1);
    
    /* AI thinking indicator */
    if (chess_gui.ai_thinking) {
        snprintf(buf, 64, "AI thinking...");
        textout_ex(screen, font, buf, BUTTON_PANEL_X, timer_y + 50, COLOR_MAGENTA, -1);
        snprintf(buf, 64, "Depth: %d", chess_gui.ai_search_depth);
        textout_ex(screen, font, buf, BUTTON_PANEL_X, timer_y + 65, COLOR_MAGENTA, -1);
        if (chess_gui.ai_total_moves > 0) {
            snprintf(buf, 64, "Move: %d/%d", chess_gui.ai_evaluated_moves, chess_gui.ai_total_moves);
            textout_ex(screen, font, buf, BUTTON_PANEL_X, timer_y + 80, COLOR_MAGENTA, -1);
        }
    }
}

void draw_board() {
    int x, y, color;
    char buf[64];
    
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
            
            /* Highlight last move (from and to squares) */
            if (chess_gui.has_last_move) {
                if ((y == chess_gui.last_move_from_row && x == chess_gui.last_move_from_col) ||
                    (y == chess_gui.last_move_to_row && x == chess_gui.last_move_to_col)) {
                    color = COLOR_CYAN;  /* Cyan highlight for last move */
                }
            }
            
            /* Highlight selected square (overrides last move highlight) */
            if (chess_gui.piece_selected && chess_gui.selected_row == y && chess_gui.selected_col == x) {
                color = COLOR_YELLOW;  /* Yellow highlight for selected piece */
            }
            
            /* Highlight cursor position for keyboard navigation */
            if (!chess_gui.piece_selected && chess_gui.selected_row == y && chess_gui.selected_col == x) {
                color = COLOR_MAGENTA;  /* Magenta highlight for cursor position */
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
        textout_ex(screen, font, buf, BOARD_START_X + 18 + x * SQUARE_SIZE, BOARD_START_Y - 20, COLOR_WHITE, -1);
    }
    
    /* Draw rank labels (8-1) */
    for (y = 0; y < 8; y++) {
        snprintf(buf, 64, "%d", 8 - y);
        textout_ex(screen, font, buf, BOARD_START_X - 20, BOARD_START_Y + 15 + y * SQUARE_SIZE, COLOR_WHITE, -1);
    }
    
    /* Draw board border */
    rect(screen, BOARD_START_X - 1, BOARD_START_Y - 1, 
         BOARD_START_X + 8 * SQUARE_SIZE, BOARD_START_Y + 8 * SQUARE_SIZE, COLOR_WHITE);
}

void draw_piece_at_square(int x, int y, ChessPiece piece) {
    if (piece.type == EMPTY) return;
    
    int screen_x = BOARD_START_X + x * SQUARE_SIZE + SQUARE_SIZE / 2;
    int screen_y = BOARD_START_Y + y * SQUARE_SIZE + SQUARE_SIZE / 2;
    int piece_size = SQUARE_SIZE - 4;  // 46 pixels for 50px squares
    
    BITMAP *sprite = get_piece_sprite(piece.type, piece.color);
    if (sprite) {
        draw_piece_sprite(sprite, screen_x, screen_y, piece_size);
    }
}

void draw_pieces() {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            ChessPiece piece = chess_gui.game.board[y][x];
            draw_piece_at_square(x, y, piece);
        }
    }
}

void draw_check_status() {
    /* Only draw if something needs to be displayed */
    bool should_draw = (chess_gui.check_display_timer > 0) || 
                       chess_gui.is_checkmate || 
                       chess_gui.is_stalemate;
    
    if (!should_draw) return;
    
    /* Determine what to display and its color */
    const char *text = NULL;
    int text_color = COLOR_WHITE;
    int box_color = COLOR_WHITE;
    
    if (chess_gui.is_checkmate) {
        text = "CHECKMATE";
        text_color = COLOR_YELLOW;  /* Gold-ish yellow */
        box_color = COLOR_YELLOW;
    } else if (chess_gui.is_stalemate) {
        text = "STALEMATE";
        text_color = COLOR_GRAY;
        box_color = COLOR_GRAY;
    } else if (chess_gui.check_display_timer > 0) {
        text = "CHECK";
        text_color = COLOR_RED;
        box_color = COLOR_RED;
    }
    
    if (!text) return;
    
    /* Calculate center of board */
    int board_center_x = BOARD_START_X + (8 * SQUARE_SIZE) / 2;
    int board_center_y = BOARD_START_Y + (8 * SQUARE_SIZE) / 2;
    
    /* Calculate text box size */
    int text_width = strlen(text) * 8;  /* Rough estimate for 8-pixel wide chars */
    int box_width = text_width + 40;
    int box_height = 60;
    int box_x = board_center_x - box_width / 2;
    int box_y = board_center_y - box_height / 2;
    
    /* Draw semi-transparent background box */
    /* Fill background */
    rectfill(screen, box_x, box_y, box_x + box_width, box_y + box_height, COLOR_BLACK);
    
    /* Draw border with status color */
    rect(screen, box_x, box_y, box_x + box_width, box_y + box_height, box_color);
    rect(screen, box_x + 1, box_y + 1, box_x + box_width - 1, box_y + box_height - 1, box_color);
    
    /* Draw text centered in box */
    textout_centre_ex(screen, font, text, board_center_x, board_center_y - 8,
                      text_color, -1);
}

void draw_help_screen() {
    int y = 50;
    
    rectfill(screen, 0, 0, 640, 480, COLOR_BLACK);
    
    draw_text_center(320, y, COLOR_YELLOW, "BeatChess - Help");
    y += 30;
    
    draw_text(100, y, COLOR_CYAN, "KEYBOARD SHORTCUTS:"); y += 20;
    draw_text(100, y, COLOR_WHITE, "N - New Game"); y += 15;
    draw_text(100, y, COLOR_WHITE, "U - Undo Move"); y += 15;
    draw_text(100, y, COLOR_WHITE, "A - Toggle AI Mode (Player vs AI / AI vs AI)"); y += 15;
    draw_text(100, y, COLOR_WHITE, "B - Swap Player Color (White/Black)"); y += 15;
    draw_text(100, y, COLOR_WHITE, "? - Show This Help"); y += 15;
    draw_text(100, y, COLOR_WHITE, "Q or ESC - Quit Game"); y += 25;
    
    draw_text(100, y, COLOR_CYAN, "MOUSE CONTROLS:"); y += 20;
    draw_text(100, y, COLOR_WHITE, "Click on a piece to select it"); y += 15;
    draw_text(100, y, COLOR_WHITE, "Click on a square to move the piece there"); y += 15;
    draw_text(100, y, COLOR_WHITE, "Click side buttons for quick actions"); y += 15;
    draw_text(100, y, COLOR_WHITE, "Click 'File' menu for game options"); y += 25;
    
    draw_text(100, y, COLOR_CYAN, "GAME MODES:"); y += 20;
    draw_text(100, y, COLOR_WHITE, "Player vs AI - Play against the computer"); y += 15;
    draw_text(100, y, COLOR_WHITE, "AI vs AI - Watch two AI players compete"); y += 25;
    
    draw_text_center(320, y + 20, COLOR_GREEN, "Press any key to continue...");
}

void draw_about_screen() {
    int y = 80;
    
    rectfill(screen, 0, 0, 640, 480, COLOR_BLACK);
    
    draw_text_center(320, y, COLOR_YELLOW, "BeatChess");
    y += 20;
    draw_text_center(320, y, COLOR_WHITE, "DOS Edition");
    y += 40;
    
    draw_text_center(320, y, COLOR_CYAN, "Copyright (c) 2025 Jason Brian Hall");
    y += 30;
    
    draw_text_center(320, y, COLOR_GREEN, "MIT License");
    y += 30;
    
    draw_text(80, y, COLOR_WHITE, "Permission is hereby granted, free of charge, to any person"); y += 15;
    draw_text(80, y, COLOR_WHITE, "obtaining a copy of this software and associated documentation"); y += 15;
    draw_text(80, y, COLOR_WHITE, "files (the \"Software\"), to deal in the Software without"); y += 15;
    draw_text(80, y, COLOR_WHITE, "restriction, including without limitation the rights to use,"); y += 15;
    draw_text(80, y, COLOR_WHITE, "copy, modify, merge, publish, distribute, sublicense, and/or"); y += 15;
    draw_text(80, y, COLOR_WHITE, "sell copies of the Software, and to permit persons to whom the"); y += 15;
    draw_text(80, y, COLOR_WHITE, "Software is furnished to do so, subject to the following"); y += 15;
    draw_text(80, y, COLOR_WHITE, "conditions:"); y += 25;
    
    draw_text(80, y, COLOR_WHITE, "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND."); y += 30;
    
    draw_text_center(320, y + 20, COLOR_GREEN, "Press any key to continue...");
}

/* ============================================================================
 * Menu and button handling
 * ============================================================================
 */

int execute_menu_action(int index) {
    switch (index) {
        case 0:  /* New Game */
            init_chess_game();
            chess_gui.ai_move_counter = 0;
            return 1;  /* Continue */
            
        case 1:  /* Undo Move */
            undo_move();
            chess_gui.ai_move_counter = 0;
            return 1;  /* Continue */
            
        case 3:  /* AI vs AI */
            chess_gui.ai_vs_ai = !chess_gui.ai_vs_ai;
            chess_gui.ai_move_counter = 0;
            return 1;  /* Continue */
            
        case 4:  /* Swap Color */
            chess_gui.player_is_white = !chess_gui.player_is_white;
            chess_gui.ai_move_counter = 0;
            return 1;  /* Continue */
            
        case 6:  /* Help */
            chess_gui.show_help = true;
            return 1;  /* Continue */
            
        case 7:  /* About */
            chess_gui.show_about = true;
            return 1;  /* Continue */
            
        case 9:  /* Quit */
            return 0;  /* Signal quit */
    }
    return 1;  /* Continue by default */
}

int handle_menu_click(int mx, int my) {
    /* Check if clicking menu bar */
    if (my < MENU_BAR_HEIGHT) {
        if (mx < MENU_ITEM_WIDTH) {
            chess_gui.show_menu = !chess_gui.show_menu;
            chess_gui.menu_selected = -1;
            return 1;  /* Continue */
        }
        /* Clicked elsewhere on menu bar - close menu if open */
        if (chess_gui.show_menu) {
            chess_gui.show_menu = false;
            chess_gui.menu_selected = -1;
        }
        return 1;  /* Continue */
    }
    
    /* Check if clicking menu items */
    if (chess_gui.show_menu) {
        int menu_x = 0;
        int menu_y = MENU_BAR_HEIGHT;
        int menu_w = 200;  /* Match the new width */
        int item_h = 20;
        
        if (mx >= menu_x && mx < menu_x + menu_w &&
            my >= menu_y && my < menu_y + NUM_MENU_ITEMS * item_h) {
            int item = (my - menu_y) / item_h;
            /* Bounds check */
            if (item >= 0 && item < NUM_MENU_ITEMS) {
                /* Check if it's not a separator */
                if (strlen(menu_items[item]) > 0) {
                    int result = execute_menu_action(item);
                    chess_gui.show_menu = false;
                    chess_gui.menu_selected = -1;
                    return result;  /* Return the result (0=quit, 1=continue) */
                }
            }
        } else {
            /* Clicked outside menu - close it */
            chess_gui.show_menu = false;
            chess_gui.menu_selected = -1;
            return 1;  /* Continue */
        }
    }
    
    return 1;  /* Continue by default */
}

bool handle_button_click(int mx, int my) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (side_buttons[i].enabled && 
            point_in_rect(mx, my, side_buttons[i].x, side_buttons[i].y,
                         side_buttons[i].w, side_buttons[i].h)) {
            
            /* Simulate keypress for the button's hotkey */
            int key = side_buttons[i].hotkey;
            switch (key) {
                case 'N': init_chess_game(); chess_gui.ai_move_counter = 0; break;
                case 'U': undo_move(); chess_gui.ai_move_counter = 0; break;
                case 'A': chess_gui.ai_vs_ai = !chess_gui.ai_vs_ai; chess_gui.ai_move_counter = 0; break;
                case 'B': chess_gui.player_is_white = !chess_gui.player_is_white; chess_gui.ai_move_counter = 0; break;
                case '?': chess_gui.show_help = true; break;
                case 'Q': return false;  /* Signal to quit */
            }
            return true;
        }
    }
    return true;
}

void update_menu_selection(int my) {
    /* Safety check input */
    if (my < 0 || my >= 480) {
        chess_gui.menu_selected = -1;
        return;
    }
    
    if (chess_gui.show_menu) {
        int menu_y = MENU_BAR_HEIGHT;
        int item_h = 20;
        int max_menu_y = menu_y + NUM_MENU_ITEMS * item_h;
        
        /* Bounds check */
        if (my >= menu_y && my < max_menu_y) {
            int item = (my - menu_y) / item_h;
            /* Double bounds check and skip separators */
            if (item >= 0 && item < NUM_MENU_ITEMS) {
                if (strlen(menu_items[item]) > 0) {
                    chess_gui.menu_selected = item;
                } else {
                    chess_gui.menu_selected = -1;
                }
            } else {
                chess_gui.menu_selected = -1;
            }
        } else {
            chess_gui.menu_selected = -1;
        }
    } else {
        chess_gui.menu_selected = -1;
    }
}

/* ============================================================================
 * Main function
 * ============================================================================
 */

int main(void) {
    /* Initialize Allegro */
    if (allegro_init() != 0) {
        printf("Failed to initialize Allegro\n");
        return 1;
    }
    
    install_keyboard();
    install_mouse();
    install_timer();
    
    /* Seed random number generator with current time for variety in AI moves */
    srand((unsigned int)time(NULL));
    
    /* Set graphics mode */
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
    
    /* Load chess piece sprites from embedded data */
    printf("Loading chess piece sprites...\n");
    if (load_chess_pieces() != 0) {
        printf("Error loading chess piece sprites!\n");
        destroy_bitmap(backbuffer);
        allegro_exit();
        return 1;
    }
    printf("Chess pieces loaded successfully!\n");
    
    /* Display splash screen */
    printf("Displaying splash screen...\n");
    show_splash_screen(backbuffer);
    
    init_chess_gui(void);

    bool running = true;
    int prev_mouse_b = 0;  /* Track previous mouse button state */
    
    /* Game loop */
    while (running) {
        /* Update check/checkmate/stalemate display */
        
        /* Detect check (only during actual gameplay) */
        bool in_check = chess_is_in_check(&chess_gui.game, chess_gui.game.turn);
        if (in_check && !chess_gui.is_in_check) {
            /* Transition from not-in-check to in-check */
            chess_gui.is_in_check = true;
            chess_gui.check_display_timer = 1.0;  /* Display "CHECK" for 1 second */
        } else if (!in_check) {
            chess_gui.is_in_check = false;
            chess_gui.check_display_timer = 0;  /* Hide the display */
        }
        
        /* Count down the check display timer */
        if (chess_gui.check_display_timer > 0) {
            chess_gui.check_display_timer -= 0.01;  /* ~10ms per frame */
            if (chess_gui.check_display_timer < 0) {
                chess_gui.check_display_timer = 0;
            }
        }
        
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
                chess_gui.ai_computing = true;  /* Flag that we're computing */
                chess_gui.ai_move_counter = 0;
                
                /* Record retrace count before AI starts (for accurate timing even when blocking) */
                int start_retrace = retrace_count;
                
                /* Make a COPY of the game state for AI to analyze */
                /* This prevents race conditions with drawing/display code */
                ChessGameState game_copy = chess_gui.game;
                
                /* In DOS, AI computation will block, but this is necessary for strong play.
                 * The AI evaluates thousands of positions via minimax search.
                 * On modern CPUs via DOSBox, this typically takes 1-2 seconds. */
                ChessMove ai_move = compute_ai_move();
                
                chess_gui.ai_computing = false;  /* Done computing */
                
                /* Calculate elapsed time during AI thinking using retrace counter */
                if (chess_gui.timer_started) {
                    int elapsed_retraces = retrace_count - start_retrace;
                    /* Assuming ~70 retraces per second (typical VGA), convert to our frame units */
                    /* We use 100 frames = 1 second, so scale: elapsed_retraces * (100/70) */
                    int elapsed_frames = (elapsed_retraces * 10) / 7;
                    
                    /* Add elapsed time to the AI's color */
                    if (chess_gui.game.turn == WHITE) {
                        chess_gui.white_time_frames += elapsed_frames;
                        while (chess_gui.white_time_frames >= 100) {
                            chess_gui.white_time_frames -= 100;
                            chess_gui.white_time_seconds++;
                        }
                    } else {
                        chess_gui.black_time_frames += elapsed_frames;
                        while (chess_gui.black_time_frames >= 100) {
                            chess_gui.black_time_frames -= 100;
                            chess_gui.black_time_seconds++;
                        }
                    }
                }
                
                /* Validate and make move */
                if (chess_is_valid_move(&chess_gui.game, 
                                       ai_move.from_row, ai_move.from_col,
                                       ai_move.to_row, ai_move.to_col)) {
                    /* Verify move doesn't leave king in check */
                    ChessGameState temp = chess_gui.game;
                    chess_make_move(&temp, ai_move);
                    
                    if (!chess_is_in_check(&temp, chess_gui.game.turn)) {
                        /* Record this move for highlighting */
                        chess_gui.last_move_from_row = ai_move.from_row;
                        chess_gui.last_move_from_col = ai_move.from_col;
                        chess_gui.last_move_to_row = ai_move.to_row;
                        chess_gui.last_move_to_col = ai_move.to_col;
                        chess_gui.has_last_move = true;
                        
                        chess_make_move(&chess_gui.game, ai_move);
                        save_position_to_history();
                        
                        /* Check for checkmate or stalemate */
                        if (!chess_is_in_check(&chess_gui.game, chess_gui.game.turn)) {
                            /* Current player is not in check - check if they have any legal moves */
                            ChessMove test_moves[256];
                            int num_moves = chess_get_all_moves(&chess_gui.game, chess_gui.game.turn, test_moves);
                            
                            /* Check if all moves are illegal (leave king in check) */
                            bool has_legal_move = false;
                            for (int i = 0; i < num_moves; i++) {
                                ChessGameState temp = chess_gui.game;
                                chess_make_move(&temp, test_moves[i]);
                                if (!chess_is_in_check(&temp, chess_gui.game.turn)) {
                                    has_legal_move = true;
                                    break;
                                }
                            }
                            
                            if (!has_legal_move) {
                                /* Stalemate */
                                chess_gui.is_stalemate = true;
                            }
                        } else {
                            /* Current player is in check - check if they have any legal moves */
                            ChessMove test_moves[256];
                            int num_moves = chess_get_all_moves(&chess_gui.game, chess_gui.game.turn, test_moves);
                            
                            /* Check if all moves are illegal (leave king in check) */
                            bool has_legal_move = false;
                            for (int i = 0; i < num_moves; i++) {
                                ChessGameState temp = chess_gui.game;
                                chess_make_move(&temp, test_moves[i]);
                                if (!chess_is_in_check(&temp, chess_gui.game.turn)) {
                                    has_legal_move = true;
                                    break;
                                }
                            }
                            
                            if (!has_legal_move) {
                                /* Checkmate */
                                chess_gui.is_checkmate = true;
                            }
                        }
                    }
                }
                
                chess_gui.ai_thinking = false;
                chess_gui.piece_selected = false;
            }
        }
        
        /* Update mouse position for menu highlighting */
        poll_mouse();
        
        /* Bounds check mouse position before using it */
        int safe_mouse_y = mouse_y;
        if (safe_mouse_y < 0) safe_mouse_y = 0;
        if (safe_mouse_y >= 480) safe_mouse_y = 479;
        
        update_menu_selection(safe_mouse_y);
        
        /* Update timers - only after first move (when timer_started is true) */
        if (chess_gui.timer_started) {
            /* Increment frame counter for current player */
            if (chess_gui.game.turn == WHITE) {
                chess_gui.white_time_frames++;
                if (chess_gui.white_time_frames >= 100) {  /* 100 frames * 10ms = 1 second */
                    chess_gui.white_time_frames = 0;
                    chess_gui.white_time_seconds++;
                }
            } else {
                chess_gui.black_time_frames++;
                if (chess_gui.black_time_frames >= 100) {
                    chess_gui.black_time_frames = 0;
                    chess_gui.black_time_seconds++;
                }
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
        } else if (chess_gui.show_about) {
            draw_about_screen();
        } else {
            draw_board();
            draw_pieces();
            draw_check_status();  /* Draw check/checkmate/stalemate overlay */
            draw_side_panel();
            draw_menu_bar();  /* Draw menu last so it appears on top */
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
            int key_scancode = key >> 8;  /* Get extended key code for arrow keys */
            
            /* If showing help, any key returns to game */
            if (chess_gui.show_help) {
                chess_gui.show_help = false;
                continue;
            }
            
            /* If showing about, any key returns to game */
            if (chess_gui.show_about) {
                chess_gui.show_about = false;
                continue;
            }
            
            /* Handle board navigation with arrow keys and WASD */
            if (!ai_should_move) {
                /* Arrow keys and WASD for movement */
                bool is_movement = false;
                switch (key_scancode) {
                    case KEY_UP:
                        if (chess_gui.selected_row > 0) {
                            chess_gui.selected_row--;
                        } else {
                            chess_gui.selected_row = 0;
                        }
                        is_movement = true;
                        break;
                        
                    case KEY_DOWN:
                        if (chess_gui.selected_row < 7) {
                            chess_gui.selected_row++;
                        } else {
                            chess_gui.selected_row = 7;
                        }
                        is_movement = true;
                        break;
                        
                    case KEY_LEFT:
                        if (chess_gui.selected_col > 0) {
                            chess_gui.selected_col--;
                        } else {
                            chess_gui.selected_col = 0;
                        }
                        is_movement = true;
                        break;
                        
                    case KEY_RIGHT:
                        if (chess_gui.selected_col < 7) {
                            chess_gui.selected_col++;
                        } else {
                            chess_gui.selected_col = 7;
                        }
                        is_movement = true;
                        break;
                }
                
                /* WASD movement (check ASCII for W/A/S/D) */
                if (!is_movement) {
                    switch (key_code) {
                        case 'w':
                        case 'W':
                            if (chess_gui.selected_row > 0) {
                                chess_gui.selected_row--;
                            } else {
                                chess_gui.selected_row = 0;
                            }
                            is_movement = true;
                            break;
                            
                        case 's':
                        case 'S':
                            if (chess_gui.selected_row < 7) {
                                chess_gui.selected_row++;
                            } else {
                                chess_gui.selected_row = 7;
                            }
                            is_movement = true;
                            break;
                            
                        case 'a':
                        case 'A':
                            if (chess_gui.selected_col > 0) {
                                chess_gui.selected_col--;
                            } else {
                                chess_gui.selected_col = 0;
                            }
                            is_movement = true;
                            break;
                            
                        case 'd':
                        case 'D':
                            if (chess_gui.selected_col < 7) {
                                chess_gui.selected_col++;
                            } else {
                                chess_gui.selected_col = 7;
                            }
                            is_movement = true;
                            break;
                    }
                }
                
                if (is_movement) {
                    continue;  /* Skip further key processing */
                }
            }
            
            /* Initialize cursor position on first movement key */
            if (chess_gui.selected_row < 0 && (key_scancode == KEY_UP || key_scancode == KEY_DOWN ||
                                                key_scancode == KEY_LEFT || key_scancode == KEY_RIGHT ||
                                                key_code == 'w' || key_code == 'W' ||
                                                key_code == 'a' || key_code == 'A' ||
                                                key_code == 's' || key_code == 'S' ||
                                                key_code == 'd' || key_code == 'D')) {
                chess_gui.selected_row = 0;
                chess_gui.selected_col = 0;
                continue;
            }
            
            /* Handle Enter key for selection/movement */
            if (key_code == 13) {  /* Enter key */
                if (!ai_should_move && chess_gui.selected_row >= 0) {
                    ChessPiece piece = chess_gui.game.board[chess_gui.selected_row][chess_gui.selected_col];
                    
                    if (!chess_gui.piece_selected && piece.type != EMPTY && 
                        piece.color == chess_gui.game.turn) {
                        /* Select piece - remember where it was selected from */
                        chess_gui.piece_selected_row = chess_gui.selected_row;
                        chess_gui.piece_selected_col = chess_gui.selected_col;
                        chess_gui.piece_selected = true;
                    } else if (chess_gui.piece_selected) {
                        /* Try to move piece from selected position to current position */
                        if (chess_is_valid_move(&chess_gui.game,
                                               chess_gui.piece_selected_row,
                                               chess_gui.piece_selected_col,
                                               chess_gui.selected_row,
                                               chess_gui.selected_col)) {
                            
                            ChessGameState temp = chess_gui.game;
                            ChessMove move = {chess_gui.piece_selected_row,
                                             chess_gui.piece_selected_col,
                                             chess_gui.selected_row,
                                             chess_gui.selected_col, 0};
                            chess_make_move(&temp, move);
                            
                            if (!chess_is_in_check(&temp, chess_gui.game.turn)) {
                                chess_gui.last_move_from_row = chess_gui.piece_selected_row;
                                chess_gui.last_move_from_col = chess_gui.piece_selected_col;
                                chess_gui.last_move_to_row = chess_gui.selected_row;
                                chess_gui.last_move_to_col = chess_gui.selected_col;
                                chess_gui.has_last_move = true;
                                
                                chess_make_move(&chess_gui.game, move);
                                save_position_to_history();
                                chess_gui.ai_move_counter = 0;
                                
                                /* Check for checkmate or stalemate */
                                if (!chess_is_in_check(&chess_gui.game, chess_gui.game.turn)) {
                                    /* Current player is not in check - check if they have any legal moves */
                                    ChessMove test_moves[256];
                                    int num_moves = chess_get_all_moves(&chess_gui.game, chess_gui.game.turn, test_moves);
                                    
                                    /* Check if all moves are illegal (leave king in check) */
                                    bool has_legal_move = false;
                                    for (int i = 0; i < num_moves; i++) {
                                        ChessGameState temp = chess_gui.game;
                                        chess_make_move(&temp, test_moves[i]);
                                        if (!chess_is_in_check(&temp, chess_gui.game.turn)) {
                                            has_legal_move = true;
                                            break;
                                        }
                                    }
                                    
                                    if (!has_legal_move) {
                                        /* Stalemate */
                                        chess_gui.is_stalemate = true;
                                    }
                                } else {
                                    /* Current player is in check - check if they have any legal moves */
                                    ChessMove test_moves[256];
                                    int num_moves = chess_get_all_moves(&chess_gui.game, chess_gui.game.turn, test_moves);
                                    
                                    /* Check if all moves are illegal (leave king in check) */
                                    bool has_legal_move = false;
                                    for (int i = 0; i < num_moves; i++) {
                                        ChessGameState temp = chess_gui.game;
                                        chess_make_move(&temp, test_moves[i]);
                                        if (!chess_is_in_check(&temp, chess_gui.game.turn)) {
                                            has_legal_move = true;
                                            break;
                                        }
                                    }
                                    
                                    if (!has_legal_move) {
                                        /* Checkmate */
                                        chess_gui.is_checkmate = true;
                                    }
                                }
                            }
                        }
                        
                        chess_gui.piece_selected = false;
                        chess_gui.piece_selected_row = -1;
                        chess_gui.piece_selected_col = -1;
                    }
                }
                continue;
            }
            
            /* Handle ESC to deselect piece or cancel */
            if (key_code == 27) {  /* ESC */
                chess_gui.piece_selected = false;
                chess_gui.piece_selected_row = -1;
                chess_gui.piece_selected_col = -1;
                continue;
            }
            
            switch (key_code) {
                case 'q':
                case 'Q':
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
        
        /* Handle mouse clicks */
        if ((mouse_b & 1) && !(prev_mouse_b & 1)) {  /* Left button just pressed */
            /* CRITICAL: Bounds check mouse coordinates FIRST */
            int mx = mouse_x;
            int my = mouse_y;
            
            /* Validate mouse coordinates are within screen bounds */
            if (mx < 0) mx = 0;
            if (mx >= 640) mx = 639;
            if (my < 0) my = 0;
            if (my >= 480) my = 479;
            
            /* If showing help, click returns to game */
            if (chess_gui.show_help) {
                chess_gui.show_help = false;
                prev_mouse_b = mouse_b;
                continue;
            }
            
            /* If showing about, click returns to game */
            if (chess_gui.show_about) {
                chess_gui.show_about = false;
                prev_mouse_b = mouse_b;
                continue;
            }
            
            /* Check menu clicks first */
            int menu_result = handle_menu_click(mx, my);
            if (menu_result == 0) {
                running = false;  /* Quit was selected */
                prev_mouse_b = mouse_b;
                continue;
            }
            
            /* If menu was clicked (even if just opened/closed), don't process other clicks */
            if (chess_gui.show_menu || my < MENU_BAR_HEIGHT) {
                prev_mouse_b = mouse_b;
                continue;
            }
            
            /* Check button clicks */
            if (!handle_button_click(mx, my)) {
                running = false;  /* Quit button was pressed */
                prev_mouse_b = mouse_b;
                continue;
            }
            
            /* Handle board clicks - only if it's player's turn */
            if (!ai_should_move) {
                /* Convert to board coordinates */
                if (mx >= BOARD_START_X && mx < BOARD_START_X + 400 && 
                    my >= BOARD_START_Y && my < BOARD_START_Y + 400) {
                    
                    int col = (mx - BOARD_START_X) / SQUARE_SIZE;
                    int row = (my - BOARD_START_Y) / SQUARE_SIZE;
                    
                    /* Extra bounds check for board coordinates */
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
                                    /* Record this move for highlighting */
                                    chess_gui.last_move_from_row = chess_gui.selected_row;
                                    chess_gui.last_move_from_col = chess_gui.selected_col;
                                    chess_gui.last_move_to_row = row;
                                    chess_gui.last_move_to_col = col;
                                    chess_gui.has_last_move = true;
                                    
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
        }
        
        prev_mouse_b = mouse_b;  /* Remember current state for next frame */
        
        /* Small delay to prevent CPU spinning */
        rest(10);  /* 10ms delay */
    }
    
    /* Cleanup */
    destroy_chess_pieces();  /* Free sprite bitmaps */
    destroy_bitmap(backbuffer);
    cleanup_chess_game();
    allegro_exit();
    
    return 0;
}

END_OF_MAIN()
