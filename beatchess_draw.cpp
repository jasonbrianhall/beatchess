#include "beatchess.h"
#include "visualization.h"
#include "chess_pieces.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>

// ============================================================================
// GLOBAL RENDERING MODE
// ============================================================================

static bool use_sprites = false;  // Default to geometric shapes

void set_rendering_mode(bool sprites) {
    use_sprites = sprites;
}

bool get_rendering_mode() {
    return use_sprites;
}

// ============================================================================
// SPRITE RENDERING FUNCTIONS
// ============================================================================

// Helper function to load BMP data into a cairo surface
static cairo_surface_t* create_surface_from_bmp_data(const unsigned char* bmp_data, size_t data_size) {
    // Create a surface from the BMP data
    // BMP files start with 0x424D ("BM")
    if (data_size < 54 || bmp_data[0] != 0x42 || bmp_data[1] != 0x4D) {
        return NULL;
    }
    
    // Parse BMP header
    uint32_t offset = *(uint32_t*)(bmp_data + 10);  // Pixel data offset
    int32_t width = *(int32_t*)(bmp_data + 18);
    int32_t height = *(int32_t*)(bmp_data + 22);
    uint16_t bits_per_pixel = *(uint16_t*)(bmp_data + 28);
    
    if (width <= 0 || height <= 0 || (bits_per_pixel != 24 && bits_per_pixel != 32)) {
        return NULL;
    }
    
    // Create image surface (RGBA)
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, abs(height));
    if (!surface) {
        return NULL;
    }
    
    unsigned char* surface_data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    
    // Convert BMP to RGBA
    // BMP is BGR bottom-up, Cairo is ARGB top-down
    int bytes_per_pixel_bmp = bits_per_pixel / 8;
    int scan_line_size = ((width * bits_per_pixel + 31) / 32) * 4;  // BMP scanlines are padded to 4-byte boundary
    
    // Process each pixel
    for (int y = 0; y < abs(height); y++) {
        for (int x = 0; x < width; x++) {
            // BMP coordinates (bottom-up)
            int bmp_y = (height < 0) ? y : (abs(height) - 1 - y);
            const unsigned char* bmp_pixel = bmp_data + offset + (bmp_y * scan_line_size) + (x * bytes_per_pixel_bmp);
            unsigned char* cairo_pixel = surface_data + (y * stride) + (x * 4);
            
            unsigned char b = bmp_pixel[0];
            unsigned char g = bmp_pixel[1];
            unsigned char r = bmp_pixel[2];
            unsigned char a = 0xFF;
            
            // Detect green pixels as transparent (like DOS version)
            // Green channel significantly higher than red and blue, and g > 200
            if (g > r + 30 && g > b + 30 && g > 200) {
                // Transparent pixel - green background
                a = 0x00;
                r = 0;
                g = 0;
                b = 0;
            }
            
            // Store in ARGB format (pre-multiplied alpha for Cairo)
            if (a == 0x00) {
                // Fully transparent - pre-multiply gives 0
                cairo_pixel[0] = 0;  // B
                cairo_pixel[1] = 0;  // G
                cairo_pixel[2] = 0;  // R
                cairo_pixel[3] = 0;  // A
            } else {
                // Opaque pixel
                cairo_pixel[0] = b;  // B
                cairo_pixel[1] = g;  // G
                cairo_pixel[2] = r;  // R
                cairo_pixel[3] = a;  // A
            }
        }
    }
    
    cairo_surface_mark_dirty(surface);
    return surface;
}

// Cache for sprite surfaces
static struct {
    cairo_surface_t* surfaces[12];  // 6 piece types × 2 colors
    bool initialized;
} sprite_cache = { {NULL}, false };



// Get sprite surface for a piece
static cairo_surface_t* get_sprite_surface(PieceType type, ChessColor color) {
    if (!sprite_cache.initialized) {
        return NULL;
    }
    
    // Index mapping: piece_bmps array has BLACK first, WHITE second for each type
    // So: index = (type - 1) * 2 + (color == WHITE ? 1 : 0)
    int index = (type - 1) * 2 + (color == WHITE ? 1 : 0);
    if (index < 0 || index >= 12) {
        return NULL;
    }
    
    return sprite_cache.surfaces[index];
}

// Initialize sprite cache
void init_sprite_cache() {
    // Map piece types to their BMP data
    const struct {
        const unsigned char* data;
        size_t size;
    } piece_bmps[] = {
        {black_pawn_bmp, sizeof(black_pawn_bmp)},
        {white_pawn_bmp, sizeof(white_pawn_bmp)},
        {black_knight_bmp, sizeof(black_knight_bmp)},
        {white_knight_bmp, sizeof(white_knight_bmp)},
        {black_bishop_bmp, sizeof(black_bishop_bmp)},
        {white_bishop_bmp, sizeof(white_bishop_bmp)},
        {black_rook_bmp, sizeof(black_rook_bmp)},
        {white_rook_bmp, sizeof(white_rook_bmp)},
        {black_queen_bmp, sizeof(black_queen_bmp)},
        {white_queen_bmp, sizeof(white_queen_bmp)},
        {black_king_bmp, sizeof(black_king_bmp)},
        {white_king_bmp, sizeof(white_king_bmp)},
    };
    
    for (int i = 0; i < 12; i++) {
        sprite_cache.surfaces[i] = create_surface_from_bmp_data(
            piece_bmps[i].data,
            piece_bmps[i].size
        );
    }
    
    sprite_cache.initialized = true;
}

// Clean up sprite cache
void cleanup_sprite_cache() {
    for (int i = 0; i < 12; i++) {
        if (sprite_cache.surfaces[i]) {
            cairo_surface_destroy(sprite_cache.surfaces[i]);
            sprite_cache.surfaces[i] = NULL;
        }
    }
    sprite_cache.initialized = false;
}

// Draw sprite for a piece
static void draw_sprite_piece(cairo_t *cr, PieceType type, ChessColor color, 
                             double x, double y, double size) {
    cairo_surface_t* surface = get_sprite_surface(type, color);
    if (!surface) {
        return;
    }
    
    int sprite_width = cairo_image_surface_get_width(surface);
    int sprite_height = cairo_image_surface_get_height(surface);
    
    // Scale to 75% of cell size to leave some margin
    double scale = (size * 0.75) / sprite_width;
    
    cairo_save(cr);
    cairo_translate(cr, x + size / 2, y + size / 2);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -sprite_width / 2, -sprite_height / 2);
    
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    
    cairo_restore(cr);
}

// ============================================================================
// GEOMETRIC DRAWING FUNCTIONS (Original implementation)
// ============================================================================

static void draw_geometric_piece(cairo_t *cr, PieceType type, ChessColor color, double x, double y, double size, double dance_offset) {
    double cx = x + size / 2;
    double cy = y + size / 2;
    double s = size * 0.4;  // Scale factor
    
    // Apply dance offset (vertical bounce)
    cy += dance_offset;
    
    // Set colors
    if (color == WHITE) {
        cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    } else {
        // Gold color for black pieces
        cairo_set_source_rgb(cr, 0.85, 0.65, 0.13);
    }
    
    switch (type) {
        case PAWN:
            // Circle on small rectangle
            cairo_arc(cr, cx, cy - s * 0.15, s * 0.25, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_rectangle(cr, cx - s * 0.2, cy + s * 0.1, s * 0.4, s * 0.3);
            cairo_fill(cr);
            break;
            
        case KNIGHT:
            // Crude horse head - blocky and angular
            // Base/neck
            cairo_rectangle(cr, cx - s * 0.15, cy, s * 0.3, s * 0.4);
            cairo_fill(cr);
            // Head (off-center rectangle)
            cairo_rectangle(cr, cx - s * 0.1, cy - s * 0.4, s * 0.35, s * 0.4);
            cairo_fill(cr);
            // Snout (small rectangle sticking out)
            cairo_rectangle(cr, cx + s * 0.15, cy - s * 0.25, s * 0.2, s * 0.15);
            cairo_fill(cr);
            // Ear (triangle on top)
            cairo_move_to(cr, cx + s * 0.05, cy - s * 0.4);
            cairo_line_to(cr, cx + s * 0.15, cy - s * 0.55);
            cairo_line_to(cr, cx + s * 0.2, cy - s * 0.35);
            cairo_fill(cr);
            break;
            
        case BISHOP:
            // Triangle with circle on top
            cairo_move_to(cr, cx, cy - s * 0.5);
            cairo_line_to(cr, cx - s * 0.25, cy + s * 0.4);
            cairo_line_to(cr, cx + s * 0.25, cy + s * 0.4);
            cairo_close_path(cr);
            cairo_fill(cr);
            cairo_arc(cr, cx, cy - s * 0.5, s * 0.12, 0, 2 * M_PI);
            cairo_fill(cr);
            break;
            
        case ROOK:
            // Castle tower
            cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.1, s * 0.6, s * 0.5);
            cairo_fill(cr);
            // Crenellations
            cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.5, s * 0.15, s * 0.35);
            cairo_fill(cr);
            cairo_rectangle(cr, cx - s * 0.05, cy - s * 0.5, s * 0.1, s * 0.35);
            cairo_fill(cr);
            cairo_rectangle(cr, cx + s * 0.15, cy - s * 0.5, s * 0.15, s * 0.35);
            cairo_fill(cr);
            break;
            
        case QUEEN:
            // Crown with multiple points
            cairo_move_to(cr, cx, cy - s * 0.5);
            cairo_line_to(cr, cx - s * 0.15, cy - s * 0.2);
            cairo_line_to(cr, cx - s * 0.3, cy - s * 0.4);
            cairo_line_to(cr, cx - s * 0.3, cy + s * 0.4);
            cairo_line_to(cr, cx + s * 0.3, cy + s * 0.4);
            cairo_line_to(cr, cx + s * 0.3, cy - s * 0.4);
            cairo_line_to(cr, cx + s * 0.15, cy - s * 0.2);
            cairo_close_path(cr);
            cairo_fill(cr);
            // Center ball
            cairo_arc(cr, cx, cy - s * 0.5, s * 0.1, 0, 2 * M_PI);
            cairo_fill(cr);
            break;
            
        case KING:
            // Crown with cross
            cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.1, s * 0.6, s * 0.5);
            cairo_fill(cr);
            // Cross on top
            cairo_rectangle(cr, cx - s * 0.05, cy - s * 0.6, s * 0.1, s * 0.5);
            cairo_fill(cr);
            cairo_rectangle(cr, cx - s * 0.25, cy - s * 0.45, s * 0.5, s * 0.1);
            cairo_fill(cr);
            break;
            
        default:
            break;
    }
    
    // Outline for all pieces
    if (type != EMPTY) {
        if (color == WHITE) {
            cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        } else {
            // Darker gold outline for gold pieces
            cairo_set_source_rgb(cr, 0.5, 0.35, 0.05);
        }
        cairo_set_line_width(cr, 1.5);
        
        switch (type) {
            case PAWN:
                cairo_arc(cr, cx, cy - s * 0.15, s * 0.25, 0, 2 * M_PI);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.2, cy + s * 0.1, s * 0.4, s * 0.3);
                cairo_stroke(cr);
                break;
            case KNIGHT:
                cairo_rectangle(cr, cx - s * 0.15, cy, s * 0.3, s * 0.4);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.1, cy - s * 0.4, s * 0.35, s * 0.4);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx + s * 0.15, cy - s * 0.25, s * 0.2, s * 0.15);
                cairo_stroke(cr);
                cairo_move_to(cr, cx + s * 0.05, cy - s * 0.4);
                cairo_line_to(cr, cx + s * 0.15, cy - s * 0.55);
                cairo_line_to(cr, cx + s * 0.2, cy - s * 0.35);
                cairo_stroke(cr);
                break;
            case BISHOP:
                cairo_move_to(cr, cx, cy - s * 0.5);
                cairo_line_to(cr, cx - s * 0.25, cy + s * 0.4);
                cairo_line_to(cr, cx + s * 0.25, cy + s * 0.4);
                cairo_close_path(cr);
                cairo_stroke(cr);
                cairo_arc(cr, cx, cy - s * 0.5, s * 0.12, 0, 2 * M_PI);
                cairo_stroke(cr);
                break;
            case ROOK:
                cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.1, s * 0.6, s * 0.5);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.5, s * 0.15, s * 0.35);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.05, cy - s * 0.5, s * 0.1, s * 0.35);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx + s * 0.15, cy - s * 0.5, s * 0.15, s * 0.35);
                cairo_stroke(cr);
                break;
            case QUEEN:
                cairo_move_to(cr, cx, cy - s * 0.5);
                cairo_line_to(cr, cx - s * 0.15, cy - s * 0.2);
                cairo_line_to(cr, cx - s * 0.3, cy - s * 0.4);
                cairo_line_to(cr, cx - s * 0.3, cy + s * 0.4);
                cairo_line_to(cr, cx + s * 0.3, cy + s * 0.4);
                cairo_line_to(cr, cx + s * 0.3, cy - s * 0.4);
                cairo_line_to(cr, cx + s * 0.15, cy - s * 0.2);
                cairo_close_path(cr);
                cairo_stroke(cr);
                cairo_arc(cr, cx, cy - s * 0.5, s * 0.1, 0, 2 * M_PI);
                cairo_stroke(cr);
                break;
            case KING:
                cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.1, s * 0.6, s * 0.5);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.05, cy - s * 0.6, s * 0.1, s * 0.5);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.25, cy - s * 0.45, s * 0.5, s * 0.1);
                cairo_stroke(cr);
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// PUBLIC DRAWING FUNCTION (with toggle support)
// ============================================================================

void draw_piece(cairo_t *cr, PieceType type, ChessColor color, double x, double y, double size, double dance_offset) {
    if (type == EMPTY) {
        return;
    }
    
    if (use_sprites) {
        // Try to draw sprite, fall back to geometric if not available
        draw_sprite_piece(cr, type, color, x, y, size);
    } else {
        // Draw geometric piece
        draw_geometric_piece(cr, type, color, x, y, size, dance_offset);
    }
}
