for f in wK wQ wR wB wN wP bK bQ bR bB bN bP; do
    xxd -i ${f}.svg | sed "s/unsigned char/static const unsigned char/;s/unsigned int/static const unsigned int/" >> chess_pieces_svg.h
done

