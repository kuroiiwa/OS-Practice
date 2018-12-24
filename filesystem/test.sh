#!/bin/bash
xterm -hold -e dmesg -wH &
cd /
sudo mount -t ptreefs zw2497 /ptreefs
