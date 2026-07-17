#!/bin/bash

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt

GAMEDIR="/$directory/ports/weird-samples"
cd "$GAMEDIR"

export LD_LIBRARY_PATH="$GAMEDIR:$LD_LIBRARY_PATH"
$ESUDO chmod +x "$GAMEDIR/WeirdSamples"



$GPION
./WeirdSamples 2>&1 | tee log.txt
$GPIOFF
