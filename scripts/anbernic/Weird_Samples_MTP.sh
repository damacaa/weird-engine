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
$ESUDO chmod +x "$GAMEDIR/WeirdSamples"

# [NEW] Ensure virtual input device permissions are set up
$ESUDO chmod 666 /dev/uinput

$GPION

# [NEW] Launch gptokeyb in the background to handle hotkeys
$GPTOKEYB "WeirdSamples" &

./WeirdSamples 2>&1 | tee log.txt

# [NEW] Clean up gptokeyb processes once the executable closes
$ESUDO kill -9 $(pidof gptokeyb) 2>/dev/null
$ESUDO kill -9 $(pidof gptokeyb2) 2>/dev/null

$GPIOFF

# [NEW] Perform final PortMaster wrapper cleanup
pm_finish

sync
