#!/bin/bash

# Paden instellen
APP_DIR="/home/flark/Applications/VinylRestorationSuite"
ICON_PATH="$APP_DIR/vrs_icon.png"
DESKTOP_FILE="/home/flark/.local/share/applications/vrs-standalone.desktop"

# Zorg dat de map bestaat
mkdir -p "$APP_DIR"

# Kopieer het logo naar de applicatiemap voor het desktop bestand
cp VRSlogo.png "$ICON_PATH"

# Maak het .desktop bestand aan
cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Version=1.6.27
Type=Application
Name=Vinyl Restoration Suite
Comment=Professional Audio Restoration
Exec="$APP_DIR/Vinyl Restoration Suite"
Icon=$ICON_PATH
Terminal=false
Categories=AudioVideo;Audio;
StartupWMClass=Vinyl Restoration Suite
EOF

chmod +x "$DESKTOP_FILE"

echo "Desktop integratie voltooid!"
echo "Je kunt de applicatie nu vinden in je Linux applicatiemenu."
