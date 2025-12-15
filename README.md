# Beat Chess

A Zenamp visualization plugin where chess moves are forced by the beat of your music. Since beats happen fast and the AI barely has time to think, expect chaotic gameplay with frequent blunders!

## Overview

Beat Chess is a visualization mode for Zenamp that runs chess games synchronized to your music. Fast beats mean the AI gets interrupted constantly, forcing whatever half-baked move it's calculated. This intentionally creates messy, entertaining games rather than good chess.

## Features

- **Music-Reactive Gameplay**: Moves are triggered by beats detected in your audio
- **Real-time AI Thinking**: Background AI continuously calculates moves using minimax with alpha-beta pruning
- **Move Evaluation**: Visual feedback shows whether moves were brilliant, good, mistakes, or blunders
- **Smooth Animations**: Pieces slide across the board with music-synchronized dancing
- **Evaluation Bar**: Track which side is winning in real-time
- **Auto-Restart**: Games automatically restart after 2 beats when finished

## Building

### Prerequisites

- C compiler (gcc/clang)
- GTK+ 3.0
- Cairo graphics library
- pthread

This module integrates with Zenamp's existing audio capture system, so you don't need separate PulseAudio or FFTW3 libraries for the visualization mode.

### Compilation

```bash
# Integrated into Zenamp build system
# (See main Zenamp README for full build instructions)

# For the standalone playable chess game (testing only)
g++ -o chess chess_main.cpp beatchess.cpp \
    `pkg-config --cflags --libs gtk+-3.0` \
    -lm -lpthread
```

## Usage

### As a Zenamp Visualization

This is designed to be integrated into the Zenamp music visualizer framework. Select "Beat Chess" from the visualization modes in Zenamp. The game will progress automatically based on beats detected in your playing music.

### Standalone Chess Game (For Testing)

You can also compile and run the standalone version for testing without audio:

```bash
# Play as white against AI
./chess

# Play as black against AI
./chess --black

# Watch AI vs AI
./chess --zero-players
```

## How It Works

### Beat Detection

The system analyzes audio volume over a rolling window, detecting spikes that exceed the average by a threshold factor. When a beat is detected:

1. The AI's thinking is immediately stopped
2. The best move found so far is executed
3. Thinking restarts for the next player

### Chess Engine

- **Minimax Algorithm**: Searches the game tree with alpha-beta pruning
- **Iterative Deepening**: Progressively searches deeper, ensuring a move is always available
- **Evaluation Function**: Considers material, position, mobility, king safety, tactical threats
- **Maximum Depth**: 4 ply (adjustable via `MAX_CHESS_DEPTH`)

### Visual Elements

- **Board**: Classic checkered pattern with coordinate labels
- **Pieces**: Stylized geometric representations (white pieces and gold for black)
- **Animations**: Smooth piece movement with music-synchronized bouncing
- **Highlights**: Last move shown with yellow overlay, fading over time
- **Status Display**: Shows move notation, thinking depth, and move quality

## Game Rules

Standard chess rules with full support for:

- Castling (kingside and queenside)
- En passant captures
- Pawn promotion (90% queen, 10% knight for variety)

Draw conditions:

- Stalemate
- 150-move limit (prevents infinite games)

## Configuration

Key constants in `beatchess.h`:

- `MAX_CHESS_DEPTH`: AI search depth (default: 4)
- `BEAT_HISTORY_SIZE`: Rolling window for beat detection (default: 10)

In `beatchess.cpp`:

- `MAX_MOVES_BEFORE_DRAW`: Move limit for draws (default: 150)
- `beat_threshold`: Sensitivity for beat detection (default: 1.3)

## Performance Notes

- **Fast beats = bad chess**: The AI runs in a background thread but gets interrupted constantly by music beats
- Moves are forced immediately when beats occur, often at depth 1-2 search
- This is intentional - it creates entertaining chaos rather than good gameplay
- Uptempo songs (120+ BPM) will produce especially frantic and mistake-filled games
- The AI will make better moves during quiet sections of songs when it has more time to think

## Known Limitations

- **This creates bad chess on purpose** - fast beats don't allow time for deep thinking
- Beat detection quality depends on audio source and volume levels
- Very fast beats may cause moves at depth 1 (basically random legal moves)
- The 150-move limit prevents games from running forever
- You'll see lots of blunders, hanging pieces, and missed tactics - this is expected!

## Credits

A Zenamp visualization plugin combining audio-reactive gameplay with chess AI. The intentionally chaotic nature comes from forcing move decisions on the beat rather than allowing proper calculation time.
