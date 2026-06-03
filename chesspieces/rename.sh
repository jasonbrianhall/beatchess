#!/bin/bash

for f in Chess_*t45.svg; do
    base=$(basename "$f" .svg)

    # piece letter (k,q,r,b,n,p)
    piece=${base:6:1}

    # uppercase piece letter
    piece_upper=$(echo "$piece" | tr '[:lower:]' '[:upper:]')

    # color: d = black, l = white
    color=${base:7:1}

    if [[ "$color" == "d" ]]; then
        prefix="b"
    else
        prefix="w"
    fi

    newname="${prefix}${piece_upper}.svg"
    echo "$f → $newname"
    mv "$f" "$newname"
done

