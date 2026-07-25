#!/bin/bash
# Generic deployment script for muOS
set -e

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: $0 <path_to_project> <mtp_base_path> [executable_name]"
    echo "Example: $0 ../weird-golfing mtp:/RG35XX-H/SD2"
    exit 1
fi

PROJECT_DIR=$(realpath "$1")
MTP_BASE="$2"
WEIRD_ENGINE_DIR=$(realpath "$(dirname "$0")/../..")

cd "$PROJECT_DIR"

PROJECT_ID=$(basename "$PROJECT_DIR")
PROJECT_NAME=$(grep -oP '(?<=project\()[a-zA-Z0-9_-]+' CMakeLists.txt | head -1)

if [ -z "$PROJECT_NAME" ]; then
    echo "Could not find project name in CMakeLists.txt"
    exit 1
fi

EXEC_NAME="$PROJECT_NAME"
if [ -n "$3" ]; then
    EXEC_NAME="$3"
fi

MTP_GAME="${MTP_BASE}/ports/${PROJECT_ID}"
MTP_PORTS="${MTP_BASE}/Roms/PORTS"
BUILD_DIR="build-muos"
STAGE="dist-muos"

"$WEIRD_ENGINE_DIR/scripts/anbernic/build-muos.sh" "$PROJECT_DIR"

echo "== Staging files in $PROJECT_DIR/$STAGE/"
rm -rf "$STAGE"
mkdir -p "$STAGE"

EXEC_PATH=$(find "$BUILD_DIR" -type f -name "$EXEC_NAME" | head -n 1)
if [ -z "$EXEC_PATH" ] || [ ! -x "$EXEC_PATH" ]; then
    echo "Executable $EXEC_NAME not found in $BUILD_DIR!"
    exit 1
fi

cp "$EXEC_PATH" "$STAGE/"
[ -d "$BUILD_DIR/assets" ] && cp -r "$BUILD_DIR/assets" "$STAGE/"
[ -d "$BUILD_DIR/fonts" ] && cp -r "$BUILD_DIR/fonts" "$STAGE/"
[ -d "$BUILD_DIR/shaders" ] && cp -r "$BUILD_DIR/shaders" "$STAGE/"
rm -rf "$STAGE/shaders/.vscode" 2>/dev/null || true

LAUNCHER_SCRIPT="$STAGE/${PROJECT_NAME}.sh"
cat << 'EOF' > "$LAUNCHER_SCRIPT"
#!/bin/bash
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source $controlfolder/control.txt
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls
EOF

echo "GAMEDIR=\"/\$directory/ports/$PROJECT_ID\"" >> "$LAUNCHER_SCRIPT"

cat << 'EOF' >> "$LAUNCHER_SCRIPT"
cd "$GAMEDIR"
export LD_LIBRARY_PATH="$GAMEDIR:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="19004ca6010000000100000000010000,Deeplay-keys,platform:Linux,a:b1,b:b0,x:b2,y:b3,back:b10,start:b11,leftshoulder:b4,rightshoulder:b5,lefttrigger:a2,righttrigger:a3,leftstick:b8,rightstick:b9,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a4,righty:a5,"
export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG}
19004ca6010000000100000000010000,muOS-Keys,platform:Linux,a:b1,b:b0,x:b2,y:b3,back:b10,start:b11,leftshoulder:b4,rightshoulder:b5,lefttrigger:a2,righttrigger:a3,leftstick:b8,rightstick:b9,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a4,righty:a5,"
export SDL_JOYSTICK_LINUX_CLASSIC=0
echo "SDL_GAMECONTROLLERCONFIG: $SDL_GAMECONTROLLERCONFIG" >> "$GAMEDIR/log.txt"
EOF

echo "\$ESUDO chmod +x \"\$GAMEDIR/$EXEC_NAME\"" >> "$LAUNCHER_SCRIPT"

cat << 'EOF' >> "$LAUNCHER_SCRIPT"
$ESUDO chmod 666 /dev/uinput
$GPION
EOF

echo "\$GPTOKEYB \"$EXEC_NAME\" &" >> "$LAUNCHER_SCRIPT"
echo "./$EXEC_NAME 2>&1 | tee -a log.txt" >> "$LAUNCHER_SCRIPT"

cat << 'EOF' >> "$LAUNCHER_SCRIPT"
$ESUDO kill -9 $(pidof gptokeyb) 2>/dev/null
$ESUDO kill -9 $(pidof gptokeyb2) 2>/dev/null
$GPIOFF
pm_finish
sync
EOF

chmod +x "$LAUNCHER_SCRIPT"

echo "== Creating MTP directories if they don't exist..."
kioclient5 mkdir "${MTP_BASE}/ports" 2>/dev/null || true
kioclient5 mkdir "$MTP_GAME" 2>/dev/null || true

echo "== Cleaning stale files on device..."
for item in log.txt assets fonts shaders; do
  kioclient5 rm "$MTP_GAME/$item" 2>/dev/null && echo "  removed $item" || true
done

echo "== Uploading game files to $MTP_GAME ..."
for item in "$EXEC_NAME" assets fonts shaders; do
  if [ -e "$STAGE/$item" ]; then
    echo "  -> $item"
    kioclient5 --noninteractive --overwrite cp "$PWD/$STAGE/$item" "$MTP_GAME/"
  fi
done

echo "== Uploading launcher to $MTP_PORTS ..."
kioclient5 --noninteractive --overwrite cp "$PWD/$LAUNCHER_SCRIPT" "$MTP_PORTS/"

echo "== Deploy complete."
