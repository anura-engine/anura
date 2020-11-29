#!/bin/zsh
#yolo set -eu -o pipefail

SAVEFILE=~/.frogatto/save.cfg

mv $SAVEFILE $SAVEFILE.bak >/dev/null 2>&1 || true

echo "level name, pid, loadable, savable, load 1, load 2, save 2, load 3, exits"

no() {
	printf n\\n
	pkill -9 anura 2>&1
}

for level in modules/frogatto/data/level/**/*.cfg; do
	#if [ "$level:t" = "kitty-factory.cfg" ]; then
	if [ true ]; then
		printf "$level:t, "
		
		#Start the process. Wait 10 seconds for the level to load.
		./anura --level=$level:t >/dev/null 2>&1 &
		ANURA_PROCESS_ID=$!
		printf "$ANURA_PROCESS_ID, "
		sleep 10
		
		#If the process is alive, we didn't crash uncaught loading the level.
		if [ ! -e /proc/$ANURA_PROCESS_ID ]; then no; continue; fi
		
		ANURA_WIN_ID=$(xdotool search --pid $ANURA_PROCESS_ID)
		if [ -z $ANURA_WIN_ID ]; then no; continue; fi
		
		#If the current window ID is not Anura, then it's the Anura error box. (This is a very hard window to find.)
		CURRENT_WIN_ID=$(xdotool getwindowfocus)
		if [ $CURRENT_WIN_ID != $ANURA_WIN_ID ]; then no; continue; fi
		
		printf "y, " #the level was loadable
		
		
		#Clear the save.
		rm $SAVEFILE >/dev/null 2>&1 || true
		
		#Send ctrl-s, ctrl-l. Wait. (Save twice because it's been tempermental.)
		xdotool key --window $ANURA_WIN_ID --delay 20 ctrl+semicolon
		sleep 0.5
		
		#Sanity-check a save now exists.
		if [ ! -f $SAVEFILE ]; then #Didn't work first time? Try again.
			sleep 2
			xdotool key --window $ANURA_WIN_ID --delay 20 ctrl+semicolon ctrl+semicolon ctrl+semicolon #FFS save already.
			sleep 1
			if [ ! -f $SAVEFILE ]; then no; continue; fi #nope, really not saving
		fi
		printf "y, " #The level was at saveable. (This should always pass.)
		
		xdotool key --window $ANURA_WIN_ID --delay 20 ctrl+l
		sleep 0.5
		
		#If the process is alive, we didn't hard-crash loading.
		if [ ! -e /proc/$ANURA_PROCESS_ID ]; then no; continue; fi
		
		#If the current window ID is not Anura, then it's the Anura error box.
		CURRENT_WIN_ID=$(xdotool getwindowfocus)
		if [ $CURRENT_WIN_ID != $ANURA_WIN_ID ]; then no; continue; fi
		
		printf "y, " #the level passed load 1
		
		
		#Send ctrl-l. Wait.
		xdotool key --window $ANURA_WIN_ID --delay 20 ctrl+l
		sleep 0.5
		
		#If the process is alive, we didn't hard-crash loading.
		if [ ! -e /proc/$ANURA_PROCESS_ID ]; then no; continue; fi
		
		#If the current window ID is not Anura, then it's the Anura error box.
		CURRENT_WIN_ID=$(xdotool getwindowfocus)
		if [ $CURRENT_WIN_ID != $ANURA_WIN_ID ]; then no; continue; fi
		
		printf "y, " #the level passed load 2
		
		
		#Clear the save.
		rm $SAVEFILE >/dev/null 2>&1 || true
		
		#Send ctrl-s, ctrl-l. Wait. (Save twice because it's been tempermental.)
		xdotool key --window $ANURA_WIN_ID --delay 20 ctrl+semicolon
		sleep 0.5
		
		#Sanity-check a save now exists.
		if [ ! -f $SAVEFILE ]; then #Didn't work first time? Try again.
			sleep 2
			xdotool key --window $ANURA_WIN_ID --delay 20 ctrl+semicolon ctrl+semicolon ctrl+semicolon #FFS save already.
			sleep 1
			if [ ! -f $SAVEFILE ]; then no; continue; fi #nope, really not saving
		fi
		printf "y, " #the level passed save 2
		
		
		#If the process is alive, we didn't hard-crash loading.
		xdotool key --window $ANURA_WIN_ID --delay 20 ctrl+l
		sleep 0.5
		if [ ! -e /proc/$ANURA_PROCESS_ID ]; then no; continue; fi
		
		#If the current window ID is not Anura, then it's the Anura error box.
		CURRENT_WIN_ID=$(xdotool getwindowfocus)
		if [ $CURRENT_WIN_ID != $ANURA_WIN_ID ]; then no; continue; fi
		
		printf "y, " #the level passed load 3
		
		
		#If the process is alive, we didn't hard-crash double-saving. Good!
		pkill anura
		#Wait 1.5s. If the process is still alive, something otherwise has gone wrong.
		sleep 1.5
		if [ -e /proc/$ANURA_PROCESS_ID ]; then no; continue; fi
		printf y\\n
		
		#For safety, if we accidentally left any processes running we don't want to continue.
		wait
	fi
done

mv SAVEFILE.bak SAVEFILE >/dev/null 2>&1 || true

# level name regex: \/[^\/]*$
# xdotool search --pid 717711
# 182452226
# xdotool getwindowfocus