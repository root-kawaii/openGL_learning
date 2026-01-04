#!/bin/bash

# Tracy Profiler Launch Script
# This connects to your running application and profiles it in real-time

echo "========================================"
echo "Tracy Profiler for OpenGL Application"
echo "========================================"
echo ""
echo "Instructions:"
echo "1. This script will launch the Tracy profiler GUI"
echo "2. Run your OpenGL application in another terminal"
echo "3. Tracy will automatically connect and start profiling"
echo "4. Press 'Capture' in Tracy to save the trace"
echo ""
echo "Starting Tracy profiler..."
echo ""

# Launch Tracy profiler from the build directory
cd /home/monolith/Desktop/openGL_learning/tracy/profiler/build
./tracy-profiler

echo ""
echo "Tracy profiler closed."
