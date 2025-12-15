#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define BOARD_SIZE 8
#define MAX_DEPTH 4

typedef enum { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING } PieceType;
typedef enum { NONE, WHITE, BLACK } Color;

typedef struct {
    PieceType type;
    Color color;
} Piece;

typedef struct {
    int from_row, from_col;
    int to_row, to_col;
    int score;
} Move;

typedef struct {
    Piece board[BOARD_SIZE][BOARD_SIZE];
    Color turn;
    bool white_king_moved, black_king_moved;
    bool white_rook_a_moved, white_rook_h_moved;
    bool black_rook_a_moved, black_rook_h_moved;
} GameState;

typedef struct {
    GameState game;
    Move best_move;
    int best_score;
    bool has_move;
    bool thinking;
    pthread_mutex_t lock;
    pthread_t thread;
} ThinkingState;

void init_board(GameState *game) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            game->board[r][c].type = EMPTY;
            game->board[r][c].color = NONE;
        }
    }
    
    PieceType back_row[] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    
    for (int c = 0; c < BOARD_SIZE; c++) {
        game->board[0][c].type = back_row[c];
        game->board[0][c].color = BLACK;
        game->board[1][c].type = PAWN;
        game->board[1][c].color = BLACK;
        
        game->board[6][c].type = PAWN;
        game->board[6][c].color = WHITE;
        game->board[7][c].type = back_row[c];
        game->board[7][c].color = WHITE;
    }
    
    game->turn = WHITE;
    game->white_king_moved = false;
    game->black_king_moved = false;
    game->white_rook_a_moved = false;
    game->white_rook_h_moved = false;
    game->black_rook_a_moved = false;
    game->black_rook_h_moved = false;
}

void print_board(GameState *game) {
    const char *symbols[7][2] = {
        {" ", " "}, {"♙", "♟"}, {"♘", "♞"}, {"♗", "♝"}, 
        {"♖", "♜"}, {"♕", "♛"}, {"♔", "♚"}
    };
    
    printf("\n  a b c d e f g h\n");
    for (int r = 0; r < BOARD_SIZE; r++) {
        printf("%d ", 8 - r);
        for (int c = 0; c < BOARD_SIZE; c++) {
            Piece p = game->board[r][c];
            if (p.type == EMPTY) {
                printf("· ");
            } else {
                printf("%s ", symbols[p.type][p.color == BLACK ? 1 : 0]);
            }
        }
        printf("%d\n", 8 - r);
    }
    printf("  a b c d e f g h\n\n");
}

bool is_in_bounds(int r, int c) {
    return r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE;
}

bool is_path_clear(GameState *game, int fr, int fc, int tr, int tc) {
    int dr = (tr > fr) ? 1 : (tr < fr) ? -1 : 0;
    int dc = (tc > fc) ? 1 : (tc < fc) ? -1 : 0;
    
    int r = fr + dr;
    int c = fc + dc;
    
    while (r != tr || c != tc) {
        if (game->board[r][c].type != EMPTY) return false;
        r += dr;
        c += dc;
    }
    return true;
}

bool is_valid_move(GameState *game, int fr, int fc, int tr, int tc) {
    if (!is_in_bounds(fr, fc) || !is_in_bounds(tr, tc)) return false;
    if (fr == tr && fc == tc) return false;
    
    Piece piece = game->board[fr][fc];
    Piece target = game->board[tr][tc];
    
    if (piece.type == EMPTY || piece.color != game->turn) return false;
    if (target.color == piece.color) return false;
    
    int dr = tr - fr;
    int dc = tc - fc;
    
    switch (piece.type) {
        case PAWN: {
            int direction = (piece.color == WHITE) ? -1 : 1;
            int start_row = (piece.color == WHITE) ? 6 : 1;
            
            if (dc == 0 && target.type == EMPTY) {
                if (dr == direction) return true;
                if (fr == start_row && dr == 2 * direction && 
                    game->board[fr + direction][fc].type == EMPTY) return true;
            }
            if (abs(dc) == 1 && dr == direction && target.type != EMPTY) return true;
            return false;
        }
        
        case KNIGHT:
            return (abs(dr) == 2 && abs(dc) == 1) || (abs(dr) == 1 && abs(dc) == 2);
        
        case BISHOP:
            return abs(dr) == abs(dc) && is_path_clear(game, fr, fc, tr, tc);
        
        case ROOK:
            return (dr == 0 || dc == 0) && is_path_clear(game, fr, fc, tr, tc);
        
        case QUEEN:
            return ((dr == 0 || dc == 0) || (abs(dr) == abs(dc))) && 
                   is_path_clear(game, fr, fc, tr, tc);
        
        case KING:
            return abs(dr) <= 1 && abs(dc) <= 1;
        
        default:
            return false;
    }
}

bool is_in_check(GameState *game, Color color) {
    int king_r = -1, king_c = -1;
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->board[r][c].type == KING && game->board[r][c].color == color) {
                king_r = r;
                king_c = c;
                break;
            }
        }
        if (king_r != -1) break;
    }
    
    if (king_r == -1) return false;
    
    Color opponent = (color == WHITE) ? BLACK : WHITE;
    Color original_turn = game->turn;
    game->turn = opponent;
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->board[r][c].color == opponent) {
                if (is_valid_move(game, r, c, king_r, king_c)) {
                    game->turn = original_turn;
                    return true;
                }
            }
        }
    }
    
    game->turn = original_turn;
    return false;
}

void make_move(GameState *game, Move move) {
    Piece piece = game->board[move.from_row][move.from_col];
    
    game->board[move.to_row][move.to_col] = piece;
    game->board[move.from_row][move.from_col].type = EMPTY;
    game->board[move.from_row][move.from_col].color = NONE;
    
    if (piece.type == KING) {
        if (piece.color == WHITE) game->white_king_moved = true;
        else game->black_king_moved = true;
    }
    if (piece.type == ROOK) {
        if (piece.color == WHITE) {
            if (move.from_col == 0) game->white_rook_a_moved = true;
            if (move.from_col == 7) game->white_rook_h_moved = true;
        } else {
            if (move.from_col == 0) game->black_rook_a_moved = true;
            if (move.from_col == 7) game->black_rook_h_moved = true;
        }
    }
    
    game->turn = (game->turn == WHITE) ? BLACK : WHITE;
}

int evaluate_position(GameState *game) {
    int piece_values[] = {0, 100, 320, 330, 500, 900, 20000};
    int score = 0;
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            Piece p = game->board[r][c];
            if (p.type != EMPTY) {
                int value = piece_values[p.type];
                score += (p.color == WHITE) ? value : -value;
            }
        }
    }
    
    score += (rand() % 10) - 5;
    
    return score;
}

int get_all_moves(GameState *game, Color color, Move *moves) {
    int count = 0;
    
    for (int fr = 0; fr < BOARD_SIZE; fr++) {
        for (int fc = 0; fc < BOARD_SIZE; fc++) {
            if (game->board[fr][fc].color == color) {
                for (int tr = 0; tr < BOARD_SIZE; tr++) {
                    for (int tc = 0; tc < BOARD_SIZE; tc++) {
                        if (is_valid_move(game, fr, fc, tr, tc)) {
                            GameState temp = *game;
                            Move m = {fr, fc, tr, tc, 0};
                            make_move(&temp, m);
                            
                            if (!is_in_check(&temp, color)) {
                                moves[count++] = m;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return count;
}

int minimax(GameState *game, int depth, int alpha, int beta, bool maximizing) {
    Move moves[256];
    int move_count = get_all_moves(game, game->turn, moves);
    
    if (move_count == 0) {
        if (is_in_check(game, game->turn)) {
            return maximizing ? (-1000000 + depth) : (1000000 - depth);
        }
        return maximizing ? -50 : 50;
    }
    
    if (depth == 0) {
        return evaluate_position(game);
    }
    
    if (maximizing) {
        int max_eval = INT_MIN;
        for (int i = 0; i < move_count; i++) {
            GameState temp = *game;
            make_move(&temp, moves[i]);
            int eval = minimax(&temp, depth - 1, alpha, beta, false);
            max_eval = (eval > max_eval) ? eval : max_eval;
            alpha = (alpha > eval) ? alpha : eval;
            if (beta <= alpha) break;
        }
        return max_eval;
    } else {
        int min_eval = INT_MAX;
        for (int i = 0; i < move_count; i++) {
            GameState temp = *game;
            make_move(&temp, moves[i]);
            int eval = minimax(&temp, depth - 1, alpha, beta, true);
            min_eval = (eval < min_eval) ? eval : min_eval;
            beta = (beta < eval) ? beta : eval;
            if (beta <= alpha) break;
        }
        return min_eval;
    }
}

void* think_continuously(void* arg) {
    ThinkingState *ts = (ThinkingState*)arg;
    
    while (true) {
        pthread_mutex_lock(&ts->lock);
        if (!ts->thinking) {
            pthread_mutex_unlock(&ts->lock);
            usleep(10000);
            continue;
        }
        
        GameState game_copy = ts->game;
        pthread_mutex_unlock(&ts->lock);
        
        Move moves[256];
        int move_count = get_all_moves(&game_copy, game_copy.turn, moves);
        
        if (move_count == 0) {
            pthread_mutex_lock(&ts->lock);
            ts->has_move = false;
            ts->thinking = false;
            pthread_mutex_unlock(&ts->lock);
            continue;
        }
        
        for (int depth = 1; depth <= MAX_DEPTH; depth++) {
            Move best_moves[256];
            int best_move_count = 0;
            int best_score = (game_copy.turn == WHITE) ? INT_MIN : INT_MAX;
            
            for (int i = 0; i < move_count; i++) {
                pthread_mutex_lock(&ts->lock);
                bool should_stop = !ts->thinking;
                pthread_mutex_unlock(&ts->lock);
                
                if (should_stop) break;
                
                GameState temp = game_copy;
                make_move(&temp, moves[i]);
                int score = minimax(&temp, depth - 1, INT_MIN, INT_MAX, game_copy.turn == BLACK);
                
                if (game_copy.turn == WHITE) {
                    if (score > best_score) {
                        best_score = score;
                        best_moves[0] = moves[i];
                        best_move_count = 1;
                    } else if (score == best_score) {
                        best_moves[best_move_count++] = moves[i];
                    }
                } else {
                    if (score < best_score) {
                        best_score = score;
                        best_moves[0] = moves[i];
                        best_move_count = 1;
                    } else if (score == best_score) {
                        best_moves[best_move_count++] = moves[i];
                    }
                }
            }
            
            pthread_mutex_lock(&ts->lock);
            if (ts->thinking && best_move_count > 0) {
                ts->best_move = best_moves[rand() % best_move_count];
                ts->best_score = best_score;
                ts->has_move = true;
            }
            pthread_mutex_unlock(&ts->lock);
        }
        
        pthread_mutex_lock(&ts->lock);
        ts->thinking = false;
        pthread_mutex_unlock(&ts->lock);
    }
    
    return NULL;
}

void start_thinking(ThinkingState *ts, GameState *game) {
    pthread_mutex_lock(&ts->lock);
    ts->game = *game;
    ts->thinking = true;
    ts->has_move = false;
    pthread_mutex_unlock(&ts->lock);
}

Move get_best_move_now(ThinkingState *ts) {
    pthread_mutex_lock(&ts->lock);
    Move move = ts->best_move;
    bool has_move = ts->has_move;
    pthread_mutex_unlock(&ts->lock);
    
    if (!has_move) {
        Move moves[256];
        int count = get_all_moves(&ts->game, ts->game.turn, moves);
        if (count > 0) {
            move = moves[rand() % count];
        }
    }
    
    return move;
}

void print_help() {
    printf("Beat-Synchronized Chess Engine\n");
    printf("Usage: chess [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -b, --beat-sync         Enable beat synchronization mode (AI vs AI)\n");
    printf("  --bpm <value>           Set beats per minute (default: 120)\n");
    printf("  -s, --self-play         AI vs AI in manual mode\n");
    printf("  --delay <ms>            Delay between moves in manual self-play (default: 1000)\n");
    printf("\nExamples:\n");
    printf("  chess --beat-sync --bpm 140    # AI plays itself at 140 BPM\n");
    printf("  chess --self-play              # AI plays itself, press ENTER for moves\n");
    printf("  chess                          # Play against AI (you are White)\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    
    GameState game;
    init_board(&game);
    
    ThinkingState thinking_state;
    thinking_state.thinking = false;
    thinking_state.has_move = false;
    pthread_mutex_init(&thinking_state.lock, NULL);
    
    pthread_create(&thinking_state.thread, NULL, think_continuously, &thinking_state);
    
    bool beat_sync_mode = false;
    bool self_play = false;
    double bpm = 120.0;
    int delay_ms = 1000;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
        }
        if (strcmp(argv[i], "--beat-sync") == 0 || strcmp(argv[i], "-b") == 0) {
            beat_sync_mode = true;
            self_play = true;
        }
        if (strcmp(argv[i], "--self-play") == 0 || strcmp(argv[i], "-s") == 0) {
            self_play = true;
        }
        if (strcmp(argv[i], "--bpm") == 0 && i + 1 < argc) {
            bpm = atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            delay_ms = atoi(argv[i + 1]);
        }
    }
    
    printf("Beat-Synchronized Chess Engine\n");
    if (beat_sync_mode) {
        printf("♪ Beat sync mode: %.1f BPM (AI vs AI)\n", bpm);
        printf("Moves will happen on each beat!\n\n");
    } else if (self_play) {
        printf("Self-play mode: AI vs AI\n");
        printf("Press ENTER for next move\n\n");
    } else {
        printf("You are White (♙). Computer is Black (♟).\n");
        printf("Enter moves as: e2 e4\n\n");
    }
    
    int move_number = 1;
    double beat_interval = 60.0 / bpm;
    struct timespec last_beat = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &last_beat);
    
    start_thinking(&thinking_state, &game);
    
    while (true) {
        print_board(&game);
        
        Move moves[256];
        int move_count = get_all_moves(&game, game.turn, moves);
        
        if (move_count == 0) {
            if (is_in_check(&game, game.turn)) {
                printf("Checkmate! %s wins!\n", game.turn == WHITE ? "Black" : "White");
            } else {
                printf("Stalemate!\n");
            }
            break;
        }
        
        if (is_in_check(&game, game.turn)) {
            printf("Check!\n");
        }
        
        if (beat_sync_mode) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double elapsed = (now.tv_sec - last_beat.tv_sec) + 
                           (now.tv_nsec - last_beat.tv_nsec) / 1e9;
            
            if (elapsed < beat_interval) {
                usleep((beat_interval - elapsed) * 1e6);
            }
            
            clock_gettime(CLOCK_MONOTONIC, &last_beat);
            
            Move ai_move = get_best_move_now(&thinking_state);
            printf("♪ Move %d: %s plays %c%d→%c%d\n", 
                   move_number++,
                   game.turn == WHITE ? "White" : "Black",
                   'a' + ai_move.from_col, 8 - ai_move.from_row,
                   'a' + ai_move.to_col, 8 - ai_move.to_row);
            
            make_move(&game, ai_move);
            start_thinking(&thinking_state, &game);
        } else if (self_play) {
            printf("Press ENTER for next move...");
            getchar();
            
            Move ai_move = get_best_move_now(&thinking_state);
            printf("Move %d: %s plays %c%d→%c%d\n", 
                   move_number++,
                   game.turn == WHITE ? "White" : "Black",
                   'a' + ai_move.from_col, 8 - ai_move.from_row,
                   'a' + ai_move.to_col, 8 - ai_move.to_row);
            
            make_move(&game, ai_move);
            start_thinking(&thinking_state, &game);
        } else {
            if (game.turn == WHITE) {
                char from[3], to[3];
                printf("Your move (e.g., e2 e4): ");
                if (scanf("%s %s", from, to) != 2) {
                    printf("Invalid input!\n");
                    while (getchar() != '\n');
                    continue;
                }
                
                int fc = from[0] - 'a';
                int fr = 8 - (from[1] - '0');
                int tc = to[0] - 'a';
                int tr = 8 - (to[1] - '0');
                
                Move player_move = {fr, fc, tr, tc, 0};
                
                if (is_valid_move(&game, fr, fc, tr, tc)) {
                    GameState temp = game;
                    make_move(&temp, player_move);
                    
                    if (!is_in_check(&temp, WHITE)) {
                        make_move(&game, player_move);
                        start_thinking(&thinking_state, &game);
                    } else {
                        printf("That move would put you in check!\n");
                    }
                } else {
                    printf("Invalid move!\n");
                }
            } else {
                printf("Black thinking...\n");
                usleep(500000);
                Move ai_move = get_best_move_now(&thinking_state);
                printf("Move %d: Black plays %c%d→%c%d\n", 
                       move_number++,
                       'a' + ai_move.from_col, 8 - ai_move.from_row,
                       'a' + ai_move.to_col, 8 - ai_move.to_row);
                
                make_move(&game, ai_move);
                start_thinking(&thinking_state, &game);
            }
        }
    }
    
    return 0;
}
