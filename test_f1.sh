#!/bin/bash

# F1 Connection Test Script
# Monitors logs while testing F1 connection from training mode

echo "F1 Connection Test Monitor"
echo "=========================="
echo ""
echo "Instructions:"
echo "1. Start CCCaster normally as HOST"
echo "2. Launch MBAA.exe into training mode"
echo "3. Press F1 to open host browser"
echo "4. Select the host to connect"
echo ""
echo "Monitoring logs..."
echo ""

# Clear old logs
> dll.log
> cccaster.log

# Start monitoring in background
tail -f dll.log &
DLL_PID=$!

tail -f cccaster.log &
LOG_PID=$!

# UDP monitor for debugging
nc -u -l 9999 2>/dev/null &
UDP_PID=$!

echo "PIDs: dll=$DLL_PID log=$LOG_PID udp=$UDP_PID"
echo "Press Ctrl+C to stop monitoring"

# Wait for interrupt
trap "kill $DLL_PID $LOG_PID $UDP_PID 2>/dev/null; exit" INT
wait