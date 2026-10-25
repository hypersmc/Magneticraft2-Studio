#!/usr/bin/env sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$HOME/.local/share/applications"
file="$HOME/.local/share/applications/magneticraft2-studio.desktop"
cat > "$file" <<EOF2
[Desktop Entry]
Version=1.0
Type=Application
Name=Magneticraft2 Studio (Alpha)
Comment=Local multiblock structure and formed-model editor
Exec="$here/run-linux.sh"
Icon=$here/studio-icon.svg
Terminal=false
Categories=Graphics;Development;
StartupNotify=true
StartupWMClass=Magneticraft2Studio
EOF2
chmod +x "$file"
printf 'Installed Linux application launcher: %s\n' "$file"
