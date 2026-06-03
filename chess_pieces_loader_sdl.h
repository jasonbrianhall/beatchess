/*
 * chess_pieces_loader_sdl.h - SDL2 replacement for chess_pieces_loader.h
 *
 * Loads the same embedded 24-bit BMP data from chess_pieces.h using SDL2
 * textures instead of Allegro BITMAPs. Green pixels (g > r+30 && g > b+30
 * && g > 200) are treated as transparent, matching the Allegro loader.
 *
 * Also provides sdl_load_splashscreen() for the JPEG splash via stb_image.
 *
 * Usage:
 *   #include "chess_pieces.h"
 *   #include "chess_pieces_loader_sdl.h"
 *   ...
 *   sdl_load_chess_pieces(renderer);
 *   SDL_Texture *t = sdl_get_piece_texture(KING, WHITE);
 *   sdl_draw_piece(renderer, t, cx, cy, size);
 *   sdl_destroy_chess_pieces();
 */

#ifndef CHESS_PIECES_LOADER_SDL_H
#define CHESS_PIECES_LOADER_SDL_H

#include <SDL2/SDL.h>
#include <string.h>
#include <math.h>
#include "beatchess.h"   /* PieceType, ChessColor */
#include "chess_pieces.h"

/* stb_image for JPEG splash — define implementation once here */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

/* ============================================================================
 * BMP loader — 24-bit uncompressed, green = transparent
 * ============================================================================ */

static SDL_Texture *sdl_load_bmp_from_memory(SDL_Renderer *renderer,
                                               const unsigned char *data,
                                               unsigned int len) {
    if (len < 54 || data[0] != 'B' || data[1] != 'M') return NULL;

    int data_offset = data[10] | (data[11]<<8) | (data[12]<<16) | (data[13]<<24);
    int width  = data[18] | (data[19]<<8) | (data[20]<<16) | (data[21]<<24);
    int height = data[22] | (data[23]<<8) | (data[24]<<16) | (data[25]<<24);
    int bpp    = data[28] | (data[29]<<8);

    if (height < 0) height = -height;
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) return NULL;
    if (bpp != 24) return NULL;

    SDL_Surface *surf = SDL_CreateRGBSurface(0, width, height, 32,
                                              0x00FF0000, 0x0000FF00,
                                              0x000000FF, 0xFF000000);
    if (!surf) return NULL;

    int bytes_per_row = ((width * 3 + 3) / 4) * 4;
    const unsigned char *pixel_data = data + data_offset;

    SDL_LockSurface(surf);
    Uint32 *pixels = (Uint32 *)surf->pixels;

    for (int row = 0; row < height; row++) {
        const unsigned char *src = pixel_data + (height - 1 - row) * bytes_per_row;
        Uint32 *dst = pixels + row * width;
        for (int col = 0; col < width; col++) {
            unsigned char b = src[col*3 + 0];
            unsigned char g = src[col*3 + 1];
            unsigned char r = src[col*3 + 2];
            /* Exact pure-green mask: (0,255,0) only — matches Allegro mask color */
            if (r == 0 && g == 255 && b == 0) {
                dst[col] = 0x00000000;  /* fully transparent */
            } else {
                dst[col] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }

    SDL_UnlockSurface(surf);

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

/* ============================================================================
 * Piece texture storage
 * ============================================================================ */

typedef struct {
    SDL_Texture *white_king,   *white_queen,  *white_rook;
    SDL_Texture *white_bishop, *white_knight, *white_pawn;
    SDL_Texture *black_king,   *black_queen,  *black_rook;
    SDL_Texture *black_bishop, *black_knight, *black_pawn;
} SdlPieceTextures;

static SdlPieceTextures sdl_piece_textures;

static int sdl_load_chess_pieces(SDL_Renderer *renderer) {
    sdl_piece_textures.white_king   = sdl_load_bmp_from_memory(renderer, white_king_bmp,   white_king_bmp_len);
    sdl_piece_textures.white_queen  = sdl_load_bmp_from_memory(renderer, white_queen_bmp,  white_queen_bmp_len);
    sdl_piece_textures.white_rook   = sdl_load_bmp_from_memory(renderer, white_rook_bmp,   white_rook_bmp_len);
    sdl_piece_textures.white_bishop = sdl_load_bmp_from_memory(renderer, white_bishop_bmp, white_bishop_bmp_len);
    sdl_piece_textures.white_knight = sdl_load_bmp_from_memory(renderer, white_knight_bmp, white_knight_bmp_len);
    sdl_piece_textures.white_pawn   = sdl_load_bmp_from_memory(renderer, white_pawn_bmp,   white_pawn_bmp_len);
    sdl_piece_textures.black_king   = sdl_load_bmp_from_memory(renderer, black_king_bmp,   black_king_bmp_len);
    sdl_piece_textures.black_queen  = sdl_load_bmp_from_memory(renderer, black_queen_bmp,  black_queen_bmp_len);
    sdl_piece_textures.black_rook   = sdl_load_bmp_from_memory(renderer, black_rook_bmp,   black_rook_bmp_len);
    sdl_piece_textures.black_bishop = sdl_load_bmp_from_memory(renderer, black_bishop_bmp, black_bishop_bmp_len);
    sdl_piece_textures.black_knight = sdl_load_bmp_from_memory(renderer, black_knight_bmp, black_knight_bmp_len);
    sdl_piece_textures.black_pawn   = sdl_load_bmp_from_memory(renderer, black_pawn_bmp,   black_pawn_bmp_len);

    if (!sdl_piece_textures.white_king   || !sdl_piece_textures.white_queen  ||
        !sdl_piece_textures.white_rook   || !sdl_piece_textures.white_bishop ||
        !sdl_piece_textures.white_knight || !sdl_piece_textures.white_pawn   ||
        !sdl_piece_textures.black_king   || !sdl_piece_textures.black_queen  ||
        !sdl_piece_textures.black_rook   || !sdl_piece_textures.black_bishop ||
        !sdl_piece_textures.black_knight || !sdl_piece_textures.black_pawn) {
        fprintf(stderr, "Failed to load one or more piece sprites\n");
        return -1;
    }
    return 0;
}

static SDL_Texture *sdl_get_piece_texture(PieceType type, ChessColor color) {
    if (color == WHITE) {
        switch (type) {
            case KING:   return sdl_piece_textures.white_king;
            case QUEEN:  return sdl_piece_textures.white_queen;
            case ROOK:   return sdl_piece_textures.white_rook;
            case BISHOP: return sdl_piece_textures.white_bishop;
            case KNIGHT: return sdl_piece_textures.white_knight;
            case PAWN:   return sdl_piece_textures.white_pawn;
            default:     return NULL;
        }
    } else {
        switch (type) {
            case KING:   return sdl_piece_textures.black_king;
            case QUEEN:  return sdl_piece_textures.black_queen;
            case ROOK:   return sdl_piece_textures.black_rook;
            case BISHOP: return sdl_piece_textures.black_bishop;
            case KNIGHT: return sdl_piece_textures.black_knight;
            case PAWN:   return sdl_piece_textures.black_pawn;
            default:     return NULL;
        }
    }
}

/* Draw a piece texture centered at (cx, cy), scaled to size×size */
static void sdl_draw_piece(SDL_Renderer *renderer, SDL_Texture *tex,
                            int cx, int cy, int size) {
    if (!tex) return;
    SDL_Rect dst = { cx - size/2, cy - size/2, size, size };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
}

static void sdl_destroy_chess_pieces(void) {
    SDL_Texture **all[] = {
        &sdl_piece_textures.white_king,   &sdl_piece_textures.white_queen,
        &sdl_piece_textures.white_rook,   &sdl_piece_textures.white_bishop,
        &sdl_piece_textures.white_knight, &sdl_piece_textures.white_pawn,
        &sdl_piece_textures.black_king,   &sdl_piece_textures.black_queen,
        &sdl_piece_textures.black_rook,   &sdl_piece_textures.black_bishop,
        &sdl_piece_textures.black_knight, &sdl_piece_textures.black_pawn,
    };
    for (int i = 0; i < 12; i++) {
        if (*all[i]) { SDL_DestroyTexture(*all[i]); *all[i] = NULL; }
    }
}

/* ============================================================================
 * Splash screen — JPEG via stb_image
 * ============================================================================ */

#include "splashscreen.h"   /* splashscreen_jpg[] and splashscreen_jpg_len */

/*
 * Show the splash screen and wait for a keypress, click, or 10-second timeout.
 * Call after SDL_Init and renderer creation, before the main loop.
 */
static void sdl_show_splashscreen(SDL_Renderer *renderer, int logical_w, int logical_h) {
    int width, height, channels;
    unsigned char *pixels = stbi_load_from_memory(
        splashscreen_jpg, (int)splashscreen_jpg_len,
        &width, &height, &channels, 4);  /* force RGBA */

    if (!pixels) {
        fprintf(stderr, "Warning: could not decode splash screen\n");
        SDL_Delay(500);
        return;
    }

    SDL_Texture *tex = SDL_CreateTexture(renderer,
                                          SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STATIC,
                                          width, height);
    if (!tex) {
        stbi_image_free(pixels);
        SDL_Delay(500);
        return;
    }

    SDL_UpdateTexture(tex, NULL, pixels, width * 4);
    stbi_image_free(pixels);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);

    /* Centre the image in the logical resolution */
    SDL_Rect dst;
    dst.w = width  < logical_w ? width  : logical_w;
    dst.h = height < logical_h ? height : logical_h;
    dst.x = (logical_w - dst.w) / 2;
    dst.y = (logical_h - dst.h) / 2;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(tex);

    /* Wait up to 10 seconds for keypress or click */
    Uint32 deadline = SDL_GetTicks() + 10000;
    SDL_Event ev;
    while (SDL_GetTicks() < deadline) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_KEYDOWN ||
                ev.type == SDL_MOUSEBUTTONDOWN ||
                ev.type == SDL_QUIT) {
                return;
            }
        }
        SDL_Delay(10);
    }
}

#endif /* CHESS_PIECES_LOADER_SDL_H */
