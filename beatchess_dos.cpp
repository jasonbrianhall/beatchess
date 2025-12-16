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
#include "beatchess.h"
#include "chess_pieces.h"
#include "chess_pieces_loader.h"

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
 * UI Button Structure
 * ============================================================================
 */

typedef struct {
    int x, y, w, h;
    const char *label;
    int hotkey;  /* Keyboard shortcut */
    bool enabled;
} Button;

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
    bool show_menu;
    int menu_selected;
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
    "",  /* separator */
    "Quit         Q"
};

#define NUM_MENU_ITEMS (sizeof(menu_items) / sizeof(menu_items[0]))

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

static bool point_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
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
    chess_gui.show_menu = false;
    chess_gui.menu_selected = -1;
    chess_gui.ai_move_counter = 0;
    chess_gui.ai_move_delay = 15;  /* Frames to wait before AI moves */
    chess_gui.ai_thinking = false;
    chess_gui.ai_search_depth = 0;
    
    /* Free old history if exists */
    if (chess_gui.history) {
        free(chess_gui.history);
        chess_gui.history = NULL;
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

static void save_position_to_history() {
    if (chess_gui.history_size < chess_gui.history_capacity) {
        chess_gui.history[chess_gui.history_size++] = chess_gui.game;
    }
}

static void undo_move() {
    if (chess_gui.history_size > 1) {
        chess_gui.history_size--;
        chess_gui.game = chess_gui.history[chess_gui.history_size - 1];
        chess_gui.piece_selected = false;
    }
}

/* AI move computation */
static ChessMove compute_ai_move() {
    ChessMove moves[256];
    ChessMove best_move = {-1, -1, -1, -1, 0};
    int num_moves = chess_get_all_moves(&chess_gui.game, chess_gui.game.turn, moves);
    
    if (num_moves == 0) {
        return best_move;
    }
    
    int best_score = INT_MIN;
    chess_gui.ai_search_depth = 3;  /* Search depth */
    
    for (int i = 0; i < num_moves; i++) {
        ChessGameState temp = chess_gui.game;
        chess_make_move(&temp, moves[i]);
        
        /* Skip if move leaves king in check */
        if (chess_is_in_check(&temp, chess_gui.game.turn)) {
            continue;
        }
        
        int score = -chess_minimax(&temp, chess_gui.ai_search_depth - 1, INT_MIN, INT_MAX, false);
        
        if (score > best_score) {
            best_score = score;
            best_move = moves[i];
        }
    }
    
    return best_move;
}

/* ============================================================================
 * Drawing functions
 * ============================================================================
 */

static void draw_menu_bar() {
    /* Menu bar background */
    rectfill(screen, 0, 0, 640, MENU_BAR_HEIGHT, COLOR_BLUE);
    
    /* File menu */
    textout_ex(screen, font, "File", 5, 5, COLOR_WHITE, -1);
    
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

static void draw_button(Button *btn, bool hover) {
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

static void draw_side_panel() {
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
    sprintf(buf, "Move: %d", chess_gui.history_size);
    textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 20, COLOR_WHITE, -1);
    
    const char *turn_str = (chess_gui.game.turn == WHITE) ? "White" : "Black";
    sprintf(buf, "Turn: %s", turn_str);
    textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 35, COLOR_WHITE, -1);
    
    const char *mode_str = chess_gui.ai_vs_ai ? "AI vs AI" : 
                          (chess_gui.player_is_white ? "Player vs AI" : "AI vs Player");
    sprintf(buf, "Mode: %s", mode_str);
    textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 50, COLOR_WHITE, -1);
    
    if (!chess_gui.ai_vs_ai) {
        const char *player_color = chess_gui.player_is_white ? "White" : "Black";
        sprintf(buf, "You: %s", player_color);
        textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 65, COLOR_GREEN, -1);
    }
    
    /* AI thinking indicator */
    if (chess_gui.ai_thinking) {
        sprintf(buf, "AI thinking...");
        textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 85, COLOR_MAGENTA, -1);
        sprintf(buf, "Depth: %d", chess_gui.ai_search_depth);
        textout_ex(screen, font, buf, BUTTON_PANEL_X, info_y + 100, COLOR_MAGENTA, -1);
    }
}

static void draw_board() {
    int x, y, color;
    
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
        textout_ex(screen, font, buf, BOARD_START_X + 18 + x * SQUARE_SIZE, BOARD_START_Y - 20, COLOR_WHITE, -1);
    }
    
    /* Draw rank labels (8-1) */
    for (y = 0; y < 8; y++) {
        char buf[2];
        sprintf(buf, "%d", 8 - y);
        textout_ex(screen, font, buf, BOARD_START_X - 20, BOARD_START_Y + 15 + y * SQUARE_SIZE, COLOR_WHITE, -1);
    }
    
    /* Draw board border */
    rect(screen, BOARD_START_X - 1, BOARD_START_Y - 1, 
         BOARD_START_X + 8 * SQUARE_SIZE, BOARD_START_Y + 8 * SQUARE_SIZE, COLOR_WHITE);
}

static void draw_piece_at_square(int x, int y, ChessPiece piece) {
    if (piece.type == EMPTY) return;
    
    int screen_x = BOARD_START_X + x * SQUARE_SIZE + SQUARE_SIZE / 2;
    int screen_y = BOARD_START_Y + y * SQUARE_SIZE + SQUARE_SIZE / 2;
    int piece_size = SQUARE_SIZE - 4;  // 46 pixels for 50px squares
    
    BITMAP *sprite = get_piece_sprite(piece.type, piece.color);
    if (sprite) {
        draw_piece_sprite(sprite, screen_x, screen_y, piece_size);
    }
}

static void draw_pieces() {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            ChessPiece piece = chess_gui.game.board[y][x];
            draw_piece_at_square(x, y, piece);
        }
    }
}

static void draw_help_screen() {
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

/* ============================================================================
 * Menu and button handling
 * ============================================================================
 */

static int execute_menu_action(int index) {
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
            
        case 8:  /* Quit */
            return 0;  /* Signal quit */
    }
    return 1;  /* Continue by default */
}

static int handle_menu_click(int mx, int my) {
    /* Check if clicking menu bar */
    if (my < MENU_BAR_HEIGHT) {
        if (mx < MENU_ITEM_WIDTH) {
            chess_gui.show_menu = !chess_gui.show_menu;
            chess_gui.menu_selected = -1;
            return 1;  /* Continue */
        }
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
            if (item >= 0 && item < NUM_MENU_ITEMS && strlen(menu_items[item]) > 0) {
                int result = execute_menu_action(item);
                chess_gui.show_menu = false;
                return result;  /* Return the result (0=quit, 1=continue) */
            }
        } else {
            /* Clicked outside menu - close it */
            chess_gui.show_menu = false;
            return 1;  /* Continue */
        }
    }
    
    return 1;  /* Continue by default */
}

static bool handle_button_click(int mx, int my) {
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

static void update_menu_selection(int my) {
    if (chess_gui.show_menu) {
        int menu_y = MENU_BAR_HEIGHT;
        int item_h = 20;
        
        if (my >= menu_y && my < menu_y + NUM_MENU_ITEMS * item_h) {
            chess_gui.menu_selected = (my - menu_y) / item_h;
            if (chess_gui.menu_selected >= NUM_MENU_ITEMS || 
                strlen(menu_items[chess_gui.menu_selected]) == 0) {
                chess_gui.menu_selected = -1;
            }
        } else {
            chess_gui.menu_selected = -1;
        }
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
    
    /* Initialize game */
    memset(&chess_gui, 0, sizeof(ChessGUI));  /* Zero out the structure */
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
        
        /* Update mouse position for menu highlighting */
        poll_mouse();
        update_menu_selection(mouse_y);
        
        /* Draw to backbuffer (mouse cursor is not drawn to backbuffer) */
        BITMAP *prev_target = screen;
        screen = backbuffer;
        
        /* Clear backbuffer */
        clear_to_color(backbuffer, COLOR_BLACK);
        
        /* Draw game (no mouse cursor interference) */
        if (chess_gui.show_help) {
            draw_help_screen();
        } else {
            draw_board();
            draw_pieces();
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
            
            /* If showing help, any key returns to game */
            if (chess_gui.show_help) {
                chess_gui.show_help = false;
                continue;
            }
            
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
        
        /* Handle mouse clicks */
        if ((mouse_b & 1) && !(prev_mouse_b & 1)) {  /* Left button just pressed */
            int mx = mouse_x;
            int my = mouse_y;
            
            /* If showing help, click returns to game */
            if (chess_gui.show_help) {
                chess_gui.show_help = false;
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
            if (!ai_should_move && !chess_gui.ai_thinking) {
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
