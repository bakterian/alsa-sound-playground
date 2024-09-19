#!/bin/bash

# First install the libsound2 development package
# So that the source files and includes are added aspecially the ones in: /usr/include/alsa
# Running the apt installation command in case the necessary include file  is not found

ASOUND_FILE=/usr/include/alsa/asoundlib.h
if [ -f "$ASOUND_FILE" ]; then
    echo "The $ASOUND_FILE exist thus alsa sources are there!"
else
    echo "Installing alsa source as the $ASOUND_FILE was not found!"
    sudo apt install libasound2-dev
fi


# 2. finally compiling the simple program
BUILD_OUPUT_DIR=./buildOutput
if [ -f "$BUILD_OUPUT_DIR" ]; then
    mkdir ./buildOutput
fi
echo "Compiling the most recent example program"
g++ minPcmStereoOpt.c -lasound -lm -o buildOutput/minPcmStereoOpt.out


