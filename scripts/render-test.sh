#!/bin/bash
# Flynn comprehensive rendering diagnostic
# Run on the remote host via Flynn terminal session
# Tests all SGR attributes, colors, glyphs, emoji, box drawing,
# CP437 characters, braille, and combinations.
# Works on both monochrome (Mac Plus) and color (System 7) systems.

ESC="\033"
RST="${ESC}[0m"

section() {
    printf "\n${ESC}[1m=== %s ===${RST}\n" "$1"
}

subsection() {
    printf "\n--- %s ---\n" "$1"
}

# ==========================================================
section "Environment"
# ==========================================================
echo "TERM=$TERM"
tput colors 2>/dev/null && echo "tput colors: $(tput colors)" || echo "tput colors: N/A"
echo "Locale: $LANG"

# ==========================================================
section "SGR Text Attributes"
# ==========================================================

subsection "Individual Attributes"
printf "${ESC}[1mBold${RST}  "
printf "${ESC}[2mDim/Faint${RST}  "
printf "${ESC}[3mItalic${RST}  "
printf "${ESC}[4mUnderline${RST}  "
printf "${ESC}[5mBlink${RST}  "
printf "${ESC}[7mInverse${RST}  "
printf "${ESC}[9mStrikethrough${RST}\n"

subsection "Attribute Combinations"
printf "${ESC}[1;3mBold+Italic${RST}  "
printf "${ESC}[1;4mBold+Under${RST}  "
printf "${ESC}[3;4mItalic+Under${RST}  "
printf "${ESC}[1;3;4mBold+Ital+Under${RST}\n"
printf "${ESC}[1;9mBold+Strike${RST}  "
printf "${ESC}[3;9mItalic+Strike${RST}  "
printf "${ESC}[4;9mUnder+Strike${RST}  "
printf "${ESC}[1;3;4;9mAll Four${RST}\n"
printf "${ESC}[7;1mInverse+Bold${RST}  "
printf "${ESC}[7;3mInverse+Italic${RST}  "
printf "${ESC}[7;9mInverse+Strike${RST}  "
printf "${ESC}[2;3mDim+Italic${RST}\n"

subsection "SGR Reset Sequences"
printf "${ESC}[1mBold${ESC}[22m Normal${RST}  "
printf "${ESC}[3mItalic${ESC}[23m Normal${RST}  "
printf "${ESC}[4mUnder${ESC}[24m Normal${RST}  "
printf "${ESC}[9mStrike${ESC}[29m Normal${RST}\n"
printf "${ESC}[1;3;4;9mAll on${ESC}[0m All off (SGR 0)${RST}\n"

# ==========================================================
section "ANSI Foreground Colors (30-37, 90-97)"
# ==========================================================

subsection "Standard (30-37)"
for i in 30 31 32 33 34 35 36 37; do
    printf "${ESC}[${i}m ${i} "
done
printf "${RST}\n"

subsection "Bright (90-97)"
for i in 90 91 92 93 94 95 96 97; do
    printf "${ESC}[${i}m ${i} "
done
printf "${RST}\n"

# ==========================================================
section "ANSI Background Colors (40-47, 100-107)"
# ==========================================================

subsection "Standard (40-47)"
for i in 40 41 42 43 44 45 46 47; do
    printf "${ESC}[${i}m ${i} "
done
printf "${RST}\n"

subsection "Bright (100-107)"
for i in 100 101 102 103 104 105 106 107; do
    printf "${ESC}[${i}m${i} "
done
printf "${RST}\n"

# ==========================================================
section "FG + BG Combined"
# ==========================================================
printf "${ESC}[31;42m RED/GRN ${RST} "
printf "${ESC}[33;44m YEL/BLU ${RST} "
printf "${ESC}[37;41m WHT/RED ${RST} "
printf "${ESC}[30;47m BLK/WHT ${RST}\n"

# ==========================================================
section "Attributes + Color"
# ==========================================================

subsection "Bold + Color"
printf "${ESC}[1;31mBold Red${RST} ${ESC}[1;32mBold Green${RST} ${ESC}[1;34mBold Blue${RST} ${ESC}[1;33mBold Yellow${RST}\n"

subsection "Italic + Color"
printf "${ESC}[3;31mItalic Red${RST} ${ESC}[3;32mItalic Grn${RST} ${ESC}[3;34mItalic Blu${RST} ${ESC}[3;33mItalic Yel${RST}\n"

subsection "Underline + Color"
printf "${ESC}[4;31mUnder Red${RST} ${ESC}[4;32mUnder Grn${RST} ${ESC}[4;34mUnder Blu${RST} ${ESC}[4;33mUnder Yel${RST}\n"

subsection "Strikethrough + Color"
printf "${ESC}[9;31mStrike Red${RST} ${ESC}[9;32mStrike Grn${RST} ${ESC}[9;34mStrike Blu${RST} ${ESC}[9;33mStrike Yel${RST}\n"

subsection "Dim + Color"
printf "${ESC}[2;31mDim Red${RST} ${ESC}[2;32mDim Green${RST} ${ESC}[2;34mDim Blue${RST} ${ESC}[2;33mDim Yellow${RST}\n"

subsection "Inverse + Color"
printf "${ESC}[7;31mInv Red${RST} ${ESC}[7;32mInv Green${RST} ${ESC}[7;34mInv Blue${RST} ${ESC}[7;33mInv Yellow${RST}\n"

subsection "Blink + Background (bright bg on color, bold on mono)"
printf "${ESC}[5;41mBlink Red BG${RST} ${ESC}[5;42mBlink Grn BG${RST} ${ESC}[5;44mBlink Blu BG${RST}\n"

subsection "Bold + Italic + Color + Background"
printf "${ESC}[1;3;31;42m BI Red/Grn ${RST} "
printf "${ESC}[1;3;33;44m BI Yel/Blu ${RST} "
printf "${ESC}[1;3;9;37;41m BIS Wht/Red ${RST}\n"

# ==========================================================
section "256-Color Foreground (38;5;N)"
# ==========================================================

subsection "Standard 16"
for i in $(seq 0 7); do
    printf "${ESC}[38;5;${i}m %3d" "$i"
done
printf "${RST}\n"
for i in $(seq 8 15); do
    printf "${ESC}[38;5;${i}m %3d" "$i"
done
printf "${RST}\n"

subsection "Color Cube Sample (6x6 slice)"
for r in 0 1 2 3 4 5; do
    for g in 0 1 2 3 4 5; do
        idx=$((16 + r*36 + g*6 + 3))
        printf "${ESC}[48;5;${idx}m  "
    done
    printf "${RST} "
done
printf "${RST}\n"

subsection "Grayscale Ramp (232-255)"
for i in $(seq 232 255); do
    printf "${ESC}[48;5;${i}m "
done
printf "${RST}\n"

# ==========================================================
section "Truecolor (38;2;R;G;B) -> 256-color downgrade"
# ==========================================================
printf "${ESC}[38;2;255;0;0mTCRed${RST} "
printf "${ESC}[38;2;0;255;0mTCGrn${RST} "
printf "${ESC}[38;2;0;0;255mTCBlu${RST} "
printf "${ESC}[38;2;255;165;0mTCOra${RST} "
printf "${ESC}[38;2;128;0;255mTCPur${RST} "
printf "${ESC}[38;2;0;255;255mTCCyn${RST}\n"

# ==========================================================
section "Box Drawing - Single Line (Unicode)"
# ==========================================================
printf "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xac\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x90\n"
printf "\xe2\x94\x82 Top \xe2\x94\x82 Box \xe2\x94\x82\n"
printf "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xbc\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xa4\n"
printf "\xe2\x94\x82 Mid \xe2\x94\x82  +  \xe2\x94\x82\n"
printf "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xb4\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x98\n"

# ==========================================================
section "Box Drawing - Double Line (Unicode)"
# ==========================================================
# ╔═════╦═════╗
# ║ Top ║ Box ║
# ╠═════╬═════╣
# ║ Mid ║  +  ║
# ╚═════╩═════╝
printf "\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\xa6\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97\n"
printf "\xe2\x95\x91 Top \xe2\x95\x91 Box \xe2\x95\x91\n"
printf "\xe2\x95\xa0\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\xac\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\xa3\n"
printf "\xe2\x95\x91 Mid \xe2\x95\x91  +  \xe2\x95\x91\n"
printf "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\xa9\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\n"

# ==========================================================
section "Box Drawing - Mixed Single/Double (Unicode)"
# ==========================================================
# ╒═══╕  ╓───╖  ╞═══╡  ╟───╢
# │   │  ║   ║  │   │  ║   ║
# ╘═══╛  ╙───╜  ╞═══╡  ╟───╢
printf "\xe2\x95\x92\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x95  \xe2\x95\x93\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x95\x96  "
printf "\xe2\x95\x9e\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\xa1  \xe2\x95\x9f\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x95\xa2\n"
printf "\xe2\x94\x82   \xe2\x94\x82  \xe2\x95\x91   \xe2\x95\x91  "
printf "\xe2\x94\x82   \xe2\x94\x82  \xe2\x95\x91   \xe2\x95\x91\n"
printf "\xe2\x95\x98\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9b  \xe2\x95\x99\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x95\x9c  "
printf "\xe2\x95\x9e\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\xa1  \xe2\x95\x9f\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x95\xa2\n"

# ==========================================================
section "Colored Box Drawing"
# ==========================================================
printf "${ESC}[31m\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97${RST}\n"
printf "${ESC}[32m\xe2\x95\x91${RST}${ESC}[33m Hi! ${RST}${ESC}[32m\xe2\x95\x91${RST}\n"
printf "${ESC}[34m\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d${RST}\n"

# ==========================================================
section "Block Elements"
# ==========================================================
# U+2588 full, U+2580 upper, U+2584 lower, U+258C left, U+2590 right
printf "Full:\xe2\x96\x88 Upper:\xe2\x96\x80 Lower:\xe2\x96\x84 Left:\xe2\x96\x8c Right:\xe2\x96\x90\n"

subsection "Shades"
# U+2591 light, U+2592 medium, U+2593 dark
printf "Light:\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91 Med:\xe2\x96\x92\xe2\x96\x92\xe2\x96\x92 Dark:\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93 Full:\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\n"

subsection "Quadrants"
# U+2596 LL, U+2597 LR, U+2598 UL, U+259D UR, U+259B UL+UR+LL, U+259C UL+UR+LR
printf "UL:\xe2\x96\x98 UR:\xe2\x96\x9d LL:\xe2\x96\x96 LR:\xe2\x96\x97 3/4:\xe2\x96\x9b 3/4:\xe2\x96\x9c\n"

subsection "Colored Blocks"
printf "${ESC}[31m\xe2\x96\x88\xe2\x96\x88${RST}"
printf "${ESC}[32m\xe2\x96\x88\xe2\x96\x88${RST}"
printf "${ESC}[33m\xe2\x96\x88\xe2\x96\x88${RST}"
printf "${ESC}[34m\xe2\x96\x88\xe2\x96\x88${RST}"
printf "${ESC}[35m\xe2\x96\x88\xe2\x96\x88${RST}"
printf "${ESC}[36m\xe2\x96\x88\xe2\x96\x88${RST}"
printf "${ESC}[37m\xe2\x96\x88\xe2\x96\x88${RST}\n"

# ==========================================================
section "Card Suits & Symbols"
# ==========================================================
# U+2660 spade, U+2665 heart, U+2666 diamond, U+2663 club
printf "Spade:\xe2\x99\xa0 Heart:\xe2\x99\xa5 Diamond:\xe2\x99\xa6 Club:\xe2\x99\xa3\n"
# U+266A note, U+266B beamed notes
printf "Note:\xe2\x99\xaa Notes:\xe2\x99\xab\n"
# U+2605 filled star, U+2606 empty star
printf "Star:\xe2\x98\x85\xe2\x98\x86 "
# U+25CF filled circle, U+25CB empty circle
printf "Circle:\xe2\x97\x8f\xe2\x97\x8b "
# U+25A0 filled square, U+25A1 empty square
printf "Square:\xe2\x96\xa0\xe2\x96\xa1\n"

# ==========================================================
section "Arrows"
# ==========================================================
# U+2190 left, U+2191 up, U+2192 right, U+2193 down
printf "Left:\xe2\x86\x90 Up:\xe2\x86\x91 Right:\xe2\x86\x92 Down:\xe2\x86\x93\n"
# U+2194 left-right, U+2195 up-down
printf "L-R:\xe2\x86\x94 U-D:\xe2\x86\x95\n"

# ==========================================================
section "Triangles"
# ==========================================================
# Filled: U+25B2 up, U+25B6 right, U+25BC down, U+25C0 left
printf "Filled: \xe2\x96\xb2 \xe2\x96\xb6 \xe2\x96\xbc \xe2\x97\x80  "
# Empty: U+25B3 up, U+25B7 right, U+25BD down, U+25C1 left
printf "Empty: \xe2\x96\xb3 \xe2\x96\xb7 \xe2\x96\xbd \xe2\x97\x81  "
# Small: U+25B8 right, U+25C2 left
printf "Small: \xe2\x96\xb8 \xe2\x96\xb2\n"

# ==========================================================
section "Diamonds & Circles"
# ==========================================================
# U+25C6 filled diamond, U+25C7 empty, U+25CA lozenge
printf "Diamond:\xe2\x97\x86\xe2\x97\x87 Lozenge:\xe2\x97\x8a  "
# U+25D0-D3 half circles, U+25C9 fisheye
printf "Half:\xe2\x97\x90\xe2\x97\x91\xe2\x97\x92\xe2\x97\x93 Fisheye:\xe2\x97\x89\n"

# ==========================================================
section "Check / Cross Marks"
# ==========================================================
# U+2713 check, U+2714 heavy check, U+2717 cross, U+2718 heavy cross
printf "Check:\xe2\x9c\x93 Heavy:\xe2\x9c\x94 Cross:\xe2\x9c\x97 Heavy:\xe2\x9c\x98\n"
printf "${ESC}[32mGreen Check:\xe2\x9c\x93${RST}  ${ESC}[31mRed Cross:\xe2\x9c\x97${RST}\n"

# ==========================================================
section "Stars & Asterisks"
# ==========================================================
# U+2605 filled, U+2606 empty, U+2736 six-pointed
printf "Filled:\xe2\x98\x85 Empty:\xe2\x98\x86 Six:\xe2\x9c\xb6  "
# U+273B teardrop, U+2722 four, U+273D heavy, U+2733 eight, U+2217 operator
printf "Tear:\xe2\x9c\xbb Four:\xe2\x9c\xa2 Heavy:\xe2\x9c\xbd Eight:\xe2\x9c\xb3 Op:\xe2\x88\x97\n"

# ==========================================================
section "Circled Operators"
# ==========================================================
# U+2295 plus, U+2296 minus, U+2297 times, U+2299 dot
printf "Plus:\xe2\x8a\x95 Minus:\xe2\x8a\x96 Times:\xe2\x8a\x97 Dot:\xe2\x8a\x99\n"

# ==========================================================
section "Medium & Small Squares"
# ==========================================================
# U+25FC med filled, U+25FB med empty, U+25AA sm filled, U+25AB sm empty
printf "Med:\xe2\x97\xbc\xe2\x97\xbb  Small:\xe2\x96\xaa\xe2\x96\xab  Dot:\xc2\xb7\n"

# ==========================================================
section "Superscript & Subscript Digits"
# ==========================================================
# Superscript U+2070,00B9,00B2,00B3,2074-2079
printf "Super: x"
printf "\xc2\xb9\xc2\xb2\xc2\xb3"
printf "\xe2\x81\xb0\xe2\x81\xb4\xe2\x81\xb5\xe2\x81\xb6\xe2\x81\xb7\xe2\x81\xb8\xe2\x81\xb9"
printf "\n"
# Subscript U+2080-2089
printf "Sub:   x"
printf "\xe2\x82\x80\xe2\x82\x81\xe2\x82\x82\xe2\x82\x83\xe2\x82\x84\xe2\x82\x85\xe2\x82\x86\xe2\x82\x87\xe2\x82\x88\xe2\x82\x89"
printf "\n"

# ==========================================================
section "CP437 Symbols (via Unicode equivalents)"
# ==========================================================

subsection "Smileys, Gender, Sun, House"
# U+263A smiley, U+263B inv smiley, U+2642 male, U+2640 female, U+263C sun, U+2302 house
printf "Smiley:\xe2\x98\xba\xe2\x98\xbb Male:\xe2\x99\x82 Female:\xe2\x99\x80 Sun:\xe2\x98\xbc House:\xe2\x8c\x82\n"

subsection "Math & Greek"
# U+0393 Gamma, U+0398 Theta, U+03A6 Phi, U+03B4 delta
printf "Gamma:\xce\x93 Theta:\xce\x98 Phi:\xce\xa6 delta:\xce\xb4  "
# U+221E infinity, U+2229 intersection, U+2261 identical, U+2248 approx
printf "Inf:\xe2\x88\x9e Int:\xe2\x88\xa9 Ident:\xe2\x89\xa1 Approx:\xe2\x89\x88\n"
# U+221A sqrt, U+00BD half, U+00BC quarter, U+207F super-n
printf "Sqrt:\xe2\x88\x9a Half:\xc2\xbd Quarter:\xc2\xbc Super-n:\xe2\x81\xbf\n"

subsection "Misc CP437"
# U+2310 reversed not, U+25AC bar, U+2195 updown, U+21A8 updown+base, U+221F right angle
printf "RevNot:\xe2\x8c\x90 Bar:\xe2\x96\xac UpDn:\xe2\x86\x95 UpDn+:\xe2\x86\xa8 Angle:\xe2\x88\x9f\n"

# ==========================================================
section "Bitmap Emoji"
# ==========================================================
# These use CopyBits rendering (10x10 monochrome bitmaps)
printf "Grin:\xf0\x9f\x98\x80 Heart:\xe2\x9d\xa4 Thumb:\xf0\x9f\x91\x8d Fire:\xf0\x9f\x94\xa5 Star:\xe2\xad\x90\n"
printf "Check:\xe2\x9c\x85 Cross:\xe2\x9d\x8c Rocket:\xf0\x9f\x9a\x80 Folder:\xf0\x9f\x93\x81 Bulb:\xf0\x9f\x92\xa1\n"
printf "Globe:\xf0\x9f\x8c\x90 Wrench:\xf0\x9f\x94\xa7 Package:\xf0\x9f\x93\xa6 Snake:\xf0\x9f\x90\x8d Crab:\xf0\x9f\xa6\x80\n"

subsection "Colored Emoji"
printf "${ESC}[31m\xf0\x9f\x94\xa5Fire${RST} "
printf "${ESC}[32m\xe2\x9c\x85Check${RST} "
printf "${ESC}[34m\xf0\x9f\x8c\x90Globe${RST} "
printf "${ESC}[33m\xe2\xad\x90Star${RST}\n"

# ==========================================================
section "Braille Patterns (U+2800-U+28FF)"
# ==========================================================
# A selection of braille patterns
printf "\xe2\xa0\x81\xe2\xa0\x83\xe2\xa0\x87\xe2\xa0\x8f\xe2\xa0\x9f\xe2\xa0\xbf "
printf "\xe2\xa3\xbf\xe2\xa3\xb7\xe2\xa3\xaf\xe2\xa3\x9f\xe2\xa3\x8f\xe2\xa3\x87 "
printf "\xe2\xa0\x89\xe2\xa0\x91\xe2\xa0\xa1\xe2\xa0\x82\xe2\xa0\x84\xe2\xa0\x88\n"
printf "Braille art: \xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x80\xe2\xa3\xbf\xe2\xa0\x80\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\n"

# ==========================================================
section "DEC Special Graphics (ESC(0)"
# ==========================================================
# Switch to DEC graphics charset, draw box, switch back
printf "${ESC}(0"
printf "lqqqqqwqqqqqk\n"
printf "x Top x Box x\n"
printf "tqqqqqnqqqqqu\n"
printf "x Mid x  +  x\n"
printf "mqqqqqvqqqqqj\n"
printf "${ESC}(B"

# ==========================================================
section "Mixed: Attributes + Glyphs + Colors"
# ==========================================================

subsection "Colored glyphs with attributes"
printf "${ESC}[1;31m\xe2\x99\xa5${RST} "
printf "${ESC}[3;32m\xe2\x9c\x93 pass${RST} "
printf "${ESC}[9;31m\xe2\x9c\x97 fail${RST} "
printf "${ESC}[1;33m\xe2\x98\x85 star${RST} "
printf "${ESC}[4;34m\xe2\x86\x92 next${RST}\n"

subsection "Box with colored content"
printf "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x90\n"
printf "\xe2\x94\x82 ${ESC}[1mBold${RST} ${ESC}[3mItalic${RST} ${ESC}[9mStrike${RST} \xe2\x94\x82\n"
printf "\xe2\x94\x82 ${ESC}[31mRed${RST}  ${ESC}[32mGreen${RST}  ${ESC}[34mBlue${RST}   \xe2\x94\x82\n"
printf "\xe2\x94\x82 \xe2\x99\xa0\xe2\x99\xa5\xe2\x99\xa6\xe2\x99\xa3  \xe2\x98\x85\xe2\x98\x86  \xe2\x9c\x93\xe2\x9c\x97  \xe2\x86\x90\xe2\x86\x91\xe2\x86\x92\xe2\x86\x93 \xe2\x94\x82\n"
printf "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x98\n"

subsection "Progress bar"
printf "["
printf "${ESC}[32m\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88${RST}"
printf "${ESC}[90m\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91${RST}"
printf "] 75%%\n"

# ==========================================================
section "SGR Default Color Reset"
# ==========================================================
printf "${ESC}[31mRED ${ESC}[0mRESET ${ESC}[32mGRN ${ESC}[39mDEF_FG ${ESC}[49mDEF_BG${RST}\n"

# ==========================================================
section "Single Colored Characters (isolation test)"
# ==========================================================
printf "[${ESC}[31mR${RST}] [${ESC}[32mG${RST}] [${ESC}[34mB${RST}] [${ESC}[33mY${RST}] "
printf "[${ESC}[35mM${RST}] [${ESC}[36mC${RST}] [${ESC}[37mW${RST}]\n"

printf "\n=== End Diagnostic ===\n"
