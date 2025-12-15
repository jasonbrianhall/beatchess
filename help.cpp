#include <gtk/gtk.h>
#include <string.h>
#include <gdk/gdk.h>

// Define fallback version if not provided by Makefile
#ifndef VERSION
#define VERSION "1.0"
#endif

void on_menu_help(GtkMenuItem *menuitem, gpointer user_data) {
    (void)menuitem;
    (void)user_data;  // Suppress unused parameter warnings
    
    // Get screen resolution to adapt dialog size using modern GDK API
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = gdk_display_get_monitor(display, 0);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    int screen_width = geometry.width;
    int screen_height = geometry.height;
    bool use_compact_dialog = (screen_width <= 1024 || screen_height <= 700);
    
    // Create main dialog window
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "BeatChess Help",
        NULL,
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "Close", GTK_RESPONSE_CLOSE,
        NULL
    );
    
    // Set dialog properties based on screen size
    if (use_compact_dialog) {
        gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
        gtk_widget_set_size_request(dialog, 500, 500);
    } else {
        gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
        gtk_widget_set_size_request(dialog, 700, 600);
    }
    
    // Create notebook for tabs
    GtkWidget *notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(notebook), use_compact_dialog ? 5 : 10);
    
    // Get content area and add notebook
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content_area), notebook);
    
    // === ABOUT TAB ===
    GtkWidget *about_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(about_scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *about_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, use_compact_dialog ? 8 : 10);
    gtk_container_set_border_width(GTK_CONTAINER(about_vbox), use_compact_dialog ? 10 : 20);
    
    // Program name and version
    GtkWidget *title_label = gtk_label_new(NULL);
    char version_text[256];
    
    if (use_compact_dialog) {
        snprintf(version_text, sizeof(version_text),
            "<span size='x-large' weight='bold'>BeatChess</span>\n"
            "<span size='medium'>Version %s</span>", VERSION);
    } else {
        snprintf(version_text, sizeof(version_text),
            "<span size='xx-large' weight='bold'>BeatChess</span>\n"
            "<span size='large'>Version %s</span>", VERSION);
    }
    
    gtk_label_set_markup(GTK_LABEL(title_label), version_text);
    gtk_label_set_justify(GTK_LABEL(title_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(about_vbox), title_label, FALSE, FALSE, 0);
    
    // Description
    const char *description = use_compact_dialog ?
        "AI-Powered Chess Engine\n\n"
        "BeatChess is an interactive chess game with an AI opponent. "
        "Play against a customizable computer opponent at various skill levels.\n\n"
        "Features:\n"
        "• 5 difficulty levels (Moronic to Expert)\n"
        "• Player vs AI or AI vs AI modes\n"
        "• Play as White or Black\n"
        "• Board flip for perspective\n"
        "• Undo functionality\n"
        "• Move history and timing\n"
        "• Real-time position evaluation\n"
        "• Beautiful GTK interface"
        :
        "An Interactive Chess Game with AI Opponent\n\n"
        "BeatChess is a fully-functional chess engine with a modern GTK interface. "
        "Play against an AI opponent with adjustable difficulty, or watch two AI players compete.\n\n"
        "Key Features:\n"
        "• 5 difficulty levels from Moronic (2 plies) to Expert (10 plies)\n"
        "• Player vs AI mode for human play\n"
        "• AI vs AI mode to watch computer matches\n"
        "• Play as White or Black against the AI\n"
        "• Flip board to change perspective\n"
        "• Undo moves with full move history\n"
        "• Real-time position evaluation\n"
        "• Move timing and total time tracking\n"
        "• Smooth piece animations\n"
        "• Complete chess rule support (castling, en passant, promotion)\n"
        "• Minimax algorithm with alpha-beta pruning optimization\n";
    
    GtkWidget *desc_label = gtk_label_new(description);
    gtk_label_set_justify(GTK_LABEL(desc_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(desc_label), TRUE);
    gtk_box_pack_start(GTK_BOX(about_vbox), desc_label, TRUE, TRUE, 0);
    
    // Add the vbox to the scrolled window
    if (use_compact_dialog) {
        gtk_container_add(GTK_CONTAINER(about_scrolled), about_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), about_scrolled, 
                                gtk_label_new("About"));
    } else {
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), about_vbox, 
                                gtk_label_new("About"));
    }
    
    // === HOW TO PLAY TAB ===
    GtkWidget *howto_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(howto_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    if (use_compact_dialog) {
        gtk_widget_set_size_request(howto_scrolled, 450, 400);
    } else {
        gtk_widget_set_size_request(howto_scrolled, 600, 450);
    }
    
    GtkWidget *howto_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(howto_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(howto_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(howto_textview), GTK_WRAP_WORD);
    
    GtkTextBuffer *howto_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(howto_textview));
    const char *howto_text = 
        "HOW TO PLAY CHESS\n\n"
        "BASIC RULES:\n"
        "• Each player controls 16 pieces: 1 King, 1 Queen, 2 Rooks, 2 Knights, 2 Bishops, 8 Pawns\n"
        "• White always moves first\n"
        "• Move pieces according to their type (see below)\n"
        "• Goal: Checkmate the opponent's King\n\n"
        
        "PIECE MOVEMENT:\n"
        "• Pawn: Moves forward 1 square (2 on first move), captures diagonally\n"
        "• Knight: Moves in L-shape (2 squares in one direction, 1 perpendicular)\n"
        "• Bishop: Moves any distance diagonally\n"
        "• Rook: Moves any distance horizontally or vertically\n"
        "• Queen: Moves any distance in any direction (diagonal, horizontal, vertical)\n"
        "• King: Moves 1 square in any direction\n\n"
        
        "SPECIAL MOVES:\n"
        "• Castling: King and Rook move together if both haven't moved and path is clear\n"
        "• En Passant: Pawns can capture an enemy pawn that just moved 2 squares forward\n"
        "• Promotion: Pawn reaching the opposite end becomes Queen, Rook, Bishop, or Knight\n\n"
        
        "HOW TO USE BEATCHESS:\n"
        "1. Select difficulty level using the combo box\n"
        "2. Choose to play as White or Black via File menu\n"
        "3. Click on a piece to select it (highlighted in yellow)\n"
        "4. Click on a destination square to move\n"
        "5. AI will respond automatically\n"
        "6. Use Flip Board to change perspective\n"
        "7. Use Undo to take back moves (Player vs AI only)\n"
        "8. Use Reset/New Game to start over\n"
        "9. Switch modes with Two-Player or AI vs AI options\n\n"
        
        "GAME STATUS:\n"
        "• Checkmate: King is under attack and cannot escape (game over)\n"
        "• Stalemate: Current player has no legal moves but is not in check (draw)\n"
        "• Check: King is under attack but can escape";
    
    gtk_text_buffer_set_text(howto_buffer, howto_text, -1);
    gtk_container_add(GTK_CONTAINER(howto_scrolled), howto_textview);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), howto_scrolled, 
                            gtk_label_new("How to Play"));
    
    // === LICENSE TAB ===
    GtkWidget *license_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(license_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    if (use_compact_dialog) {
        gtk_widget_set_size_request(license_scrolled, 450, 350);
    } else {
        gtk_widget_set_size_request(license_scrolled, 600, 450);
    }
    
    GtkWidget *license_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(license_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(license_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(license_textview), GTK_WRAP_WORD);
    
    GtkTextBuffer *license_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(license_textview));
    const char *mit_license = 
        "MIT License\n\n"
        "Copyright (c) 2025 Jason Brian Hall\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy "
        "of this software and associated documentation files (the \"Software\"), to deal "
        "in the Software without restriction, including without limitation the rights "
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
        "copies of the Software, and to permit persons to whom the Software is "
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in all "
        "copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
        "SOFTWARE.";
    
    gtk_text_buffer_set_text(license_buffer, mit_license, -1);
    gtk_container_add(GTK_CONTAINER(license_scrolled), license_textview);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), license_scrolled, 
                            gtk_label_new("License"));
    
    // === SUPPORT TAB ===
    GtkWidget *support_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(support_scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *support_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, use_compact_dialog ? 12 : 15);
    gtk_container_set_border_width(GTK_CONTAINER(support_vbox), use_compact_dialog ? 15 : 25);
    
    // Support heading
    GtkWidget *support_heading = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(support_heading), 
        "<span size='x-large' weight='bold'>Support BeatChess</span>");
    gtk_label_set_justify(GTK_LABEL(support_heading), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(support_vbox), support_heading, FALSE, FALSE, 0);
    
    // Support message
    GtkWidget *support_message = gtk_label_new(
        "If you enjoy using BeatChess and would like to support "
        "its development, consider buying the developer a coffee!");
    gtk_label_set_justify(GTK_LABEL(support_message), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(support_message), TRUE);
    gtk_box_pack_start(GTK_BOX(support_vbox), support_message, FALSE, FALSE, 0);
    
    // Coffee emoji
    GtkWidget *coffee_emoji = gtk_label_new("☕");
    GtkWidget *emoji_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(emoji_container), FALSE);
    gtk_box_pack_start(GTK_BOX(emoji_container), coffee_emoji, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(support_vbox), emoji_container, FALSE, FALSE, 0);
    
    // Buy Me a Coffee link
    GtkWidget *bmac_link = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(bmac_link),
        "<b>Buy Me a Coffee</b>\n\n"
        "<a href=\"https://buymeacoffee.com/jasonbrianhall\">"
        "https://buymeacoffee.com/jasonbrianhall</a>");
    gtk_label_set_justify(GTK_LABEL(bmac_link), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(bmac_link), TRUE);
    gtk_box_pack_start(GTK_BOX(support_vbox), bmac_link, FALSE, FALSE, 0);
    
    // Optional: Support message for GitHub
    GtkWidget *github_link = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(github_link),
        "\n<b>View on GitHub</b>\n\n"
        "<a href=\"https://github.com/jasonbrianhall/beatchess\">"
        "github.com/jasonbrianhall/beatchess</a>");
    gtk_label_set_justify(GTK_LABEL(github_link), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(github_link), TRUE);
    gtk_box_pack_start(GTK_BOX(support_vbox), github_link, FALSE, FALSE, 0);
    
    // Disclaimer
    GtkWidget *disclaimer = gtk_label_new(
        "This project is independent and not affiliated with any chess organization.");
    gtk_label_set_justify(GTK_LABEL(disclaimer), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(disclaimer), TRUE);
    gtk_box_pack_start(GTK_BOX(support_vbox), disclaimer, FALSE, FALSE, use_compact_dialog ? 10 : 15);
    
    // Add spacer to push content to top
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(support_vbox), spacer, TRUE, TRUE, 0);
    
    // Add vbox to scrolled window
    gtk_container_add(GTK_CONTAINER(support_scrolled), support_vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), support_scrolled, 
                            gtk_label_new("Support"));
    
    // Show all widgets
    gtk_widget_show_all(dialog);
    
    // Run dialog and clean up
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
