#!/usr/bin/env python
"""
Convert BMP chess pieces to C header file with embedded data
Usage: python3 bmp_to_header.py images_directory output.h target_size
"""

import sys
import os
from PIL import Image
import io

def bmp_to_bytes(bmp_path, target_size=46):
    """Load BMP, resize, and return as bytes"""
    img = Image.open(bmp_path)
    
    # Resize if needed
    if img.size != (target_size, target_size):
        img = img.resize((target_size, target_size), Image.Resampling.LANCZOS)
        print(f"  Resized from {Image.open(bmp_path).size} to {target_size}x{target_size}")
    
    # Convert to RGB if needed
    if img.mode == 'RGBA':
        # Create BRIGHT GREEN background (0, 255, 0) - will be our transparency key
        background = Image.new('RGB', img.size, (0, 255, 0))
        background.paste(img, mask=img.split()[3])  # Use alpha as mask
        img = background
    elif img.mode != 'RGB':
        img = img.convert('RGB')
    
    # Save to BMP in memory
    bmp_io = io.BytesIO()
    img.save(bmp_io, format='BMP')
    return bmp_io.getvalue()

def bytes_to_c_array(data, name):
    """Convert bytes to C array definition"""
    lines = []
    lines.append(f'static const unsigned char {name}_bmp[] = {{')
    
    # Split into lines of 12 bytes for readability
    chunk_size = 12
    for i in range(0, len(data), chunk_size):
        chunk = data[i:i+chunk_size]
        hex_line = ', '.join(f'0x{b:02x}' for b in chunk)
        lines.append(f'    {hex_line},')
    
    # Remove trailing comma from last line
    lines[-1] = lines[-1].rstrip(',')
    lines.append('};')
    lines.append(f'static const unsigned int {name}_bmp_len = {len(data)};')
    
    return '\n'.join(lines)

def generate_header(images_dir, output_file, piece_size=46):
    """Generate C header file with all chess pieces"""
    
    # Piece names mapping for your BMP files
    piece_patterns = {
        'white_king': 'w_king',
        'white_queen': 'w_queen',
        'white_rook': 'w_rook',
        'white_bishop': 'w_bishop',
        'white_knight': 'w_knight',
        'white_pawn': 'w_pawn',
        'black_king': 'b_king',
        'black_queen': 'b_queen',
        'black_rook': 'b_rook',
        'black_bishop': 'b_bishop',
        'black_knight': 'b_knight',
        'black_pawn': 'b_pawn',
    }
    
    print(f"Scanning directory: {images_dir}")
    print(f"Target size: {piece_size}x{piece_size} pixels\n")
    
    pieces_data = {}
    
    # Find and convert all pieces
    for piece_name, pattern in piece_patterns.items():
        found = False
        
        for filename in os.listdir(images_dir):
            if not filename.lower().endswith('.bmp'):
                continue
            
            # Check if filename contains the pattern
            if pattern in filename:
                bmp_path = os.path.join(images_dir, filename)
                print(f"Converting {filename} -> {piece_name}")
                
                try:
                    bmp_bytes = bmp_to_bytes(bmp_path, piece_size)
                    pieces_data[piece_name] = bmp_bytes
                    found = True
                except Exception as e:
                    print(f"  ERROR: {e}")
                break
        
        if not found:
            print(f"WARNING: Could not find {piece_name}")
    
    if len(pieces_data) != 12:
        print(f"\nERROR: Only found {len(pieces_data)} out of 12 pieces!")
        return
    
    # Generate header file
    print(f"\nGenerating header file: {output_file}")
    
    with open(output_file, 'w') as f:
        f.write("/*\n")
        f.write(" * Chess Pieces - Embedded BMP Data\n")
        f.write(f" * Auto-generated from BMP files ({piece_size}x{piece_size} pixels)\n")
        f.write(" */\n\n")
        f.write("#ifndef CHESS_PIECES_H\n")
        f.write("#define CHESS_PIECES_H\n\n")
        
        # Write all piece arrays
        for piece_name, bmp_bytes in sorted(pieces_data.items()):
            f.write(f"/* {piece_name}.bmp ({len(bmp_bytes)} bytes, {piece_size}x{piece_size} px) */\n")
            f.write(bytes_to_c_array(bmp_bytes, piece_name))
            f.write("\n\n")
        
        f.write("#endif /* CHESS_PIECES_H */\n")
    
    total_size = sum(len(d) for d in pieces_data.values())
    print(f"\n✓ Done! Generated {len(pieces_data)} pieces")
    print(f"✓ Total embedded size: {total_size:,} bytes ({total_size/1024:.1f} KB)")
    print(f"✓ Output: {output_file}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python3 bmp_to_header.py <images_directory> <output.h> [target_size]")
        print("\nExample:")
        print("  python3 bmp_to_header.py images chess_pieces.h 46")
        print("\nThis will:")
        print("  - Load all BMP files from 'images' directory")
        print("  - Resize them to 46x46 pixels")
        print("  - Embed them in 'chess_pieces.h' as C arrays")
        sys.exit(1)
    
    images_dir = sys.argv[1]
    output_file = sys.argv[2]
    piece_size = int(sys.argv[3]) if len(sys.argv) > 3 else 46
    
    if not os.path.isdir(images_dir):
        print(f"Error: {images_dir} is not a directory")
        sys.exit(1)
    
    generate_header(images_dir, output_file, piece_size)
