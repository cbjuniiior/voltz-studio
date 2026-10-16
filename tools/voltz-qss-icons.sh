#!/usr/bin/env bash
# Gera SVGs pre-coloridos para regras QSS (image:/qproperty-icon: nao passam pelo PhosphorIconEngine,
# entao o currentColor dos SVGs Phosphor precisa vir trocado pela cor final).
# Uso (da raiz do repo): bash tools/voltz-qss-icons.sh   -> sobrescreve frontend/voltz/shell/icons/qss/
set -euo pipefail
SRC=frontend/voltz/shell/icons
DEST="$SRC/qss"
mkdir -p "$DEST"
# icone peso nome-da-cor cor
while read -r name weight cname hex; do
  [ -z "$name" ] && continue
  if [ "$weight" = fill ]; then in="$SRC/fill/$name-fill.svg"; else in="$SRC/regular/$name.svg"; fi
  sed "s/currentColor/$hex/g" "$in" > "$DEST/$name-$cname.svg"
done <<'EOT'
caret-down regular muted #82828C
caret-down regular accent #D4F53C
caret-up regular accent #D4F53C
speaker-high regular text #F2F2F3
speaker-slash regular rec #FF4D4D
warning-circle regular warn #F5C83C
headphones regular muted #82828C
headphones fill accent #D4F53C
EOT
ls "$DEST"
