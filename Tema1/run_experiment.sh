#!/bin/bash

SPEED=10
DELAY=10
LOSS=5
CORRUPT=5
FILENAME=fileX
TASK_INDEX=3

killall link &>/dev/null
killall recv &>/dev/null
killall send &>/dev/null

./link_emulator/link speed=$SPEED delay=$DELAY loss=$LOSS corrupt=$CORRUPT &> /dev/null &
sleep 1
./recv $TASK_INDEX &
sleep 1

./send $TASK_INDEX $FILENAME $SPEED $DELAY

sleep 5
echo "[SCRIPT] Finished transfer, checking files: $FILENAME recv_$FILENAME"
diff $FILENAME recv_$FILENAME
