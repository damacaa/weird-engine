#!/bin/bash

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt

# [NEW] Source custom CFW mod files and fetch device-specific controls
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/weird-samples"
cd "$GAMEDIR"

export LD_LIBRARY_PATH="$GAMEDIR:$LD_LIBRARY_PATH"

# PortMaster's $sdl_controllerconfig uses js*-style button indices that do not
# match SDL3's default event* path on this device. Use the verified SDL3 mapping.
# Backup of this known-good launcher:
#   scripts/anbernic/backup/Weird_Samples_MTP.working-hardcoded-mapping.sh
#   scripts/anbernic/backup/rg35xxh-sdl3-known-good.mapping.txt
export SDL_GAMECONTROLLERCONFIG="19004ca6010000000100000000010000,Deeplay-keys,platform:Linux,a:b1,b:b0,x:b2,y:b3,back:b10,start:b11,leftshoulder:b4,rightshoulder:b5,lefttrigger:a2,righttrigger:a3,leftstick:b8,rightstick:b9,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a4,righty:a5,"
# Also ship under muOS-Keys (runtime name) so name-based rebind always hits.
export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG}
19004ca6010000000100000000010000,muOS-Keys,platform:Linux,a:b1,b:b0,x:b2,y:b3,back:b10,start:b11,leftshoulder:b4,rightshoulder:b5,lefttrigger:a2,righttrigger:a3,leftstick:b8,rightstick:b9,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a4,righty:a5,"
# Force event* path (this mapping was verified without classic js*).
export SDL_JOYSTICK_LINUX_CLASSIC=0
echo "SDL_GAMECONTROLLERCONFIG: $SDL_GAMECONTROLLERCONFIG" >> "$GAMEDIR/log.txt"

$ESUDO chmod +x "$GAMEDIR/WeirdSamples"

# [NEW] Ensure virtual input device permissions are set up
$ESUDO chmod 666 /dev/uinput

$GPION

# [NEW] Launch gptokeyb in the background to handle hotkeys
$GPTOKEYB "WeirdSamples" &

./WeirdSamples 2>&1 | tee -a log.txt

# [NEW] Clean up gptokeyb processes once the executable closes
$ESUDO kill -9 $(pidof gptokeyb) 2>/dev/null
$ESUDO kill -9 $(pidof gptokeyb2) 2>/dev/null

$GPIOFF

# [NEW] Perform final PortMaster wrapper cleanup
pm_finish

sync
