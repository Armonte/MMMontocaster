#!/bin/bash

# UDP Log Monitor for F1 Connection Testing
# Listens on port 9999 for UDP debug messages

echo "F1 Connection UDP Monitor"
echo "========================="
echo ""
echo "Listening on UDP port 9999..."
echo "Start CCCaster and test F1 connections"
echo ""

nc -u -l 9999