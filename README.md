# MKV Stream Extracter

Tool for extracting specific streams (video, audio, subtitles) from mkv (container) file. Works on Linux.

## Usage
`./stream-extract <path to mkv> [flags]`

### Flags:
- `-i` - interactive mode (choose streams to extract)
        
        Interactive mode controls:
            [arrows] to navigate
            [space] to select/deselect
            [enter] to confirm
        
- `-n` - no action mode (don't do anything, just print)
- `-h` - show this help message

## Prerequisites

- **boost/regex.hpp** module for parsing ffprobe output (`apt install -y libboost-all-dev`)
- **ncurses.h** module for interactive mode (`apt install -y libncurses-dev`)
- **ffmpeg** for extracting (`apt install -y ffmpeg`)

## Installing

Install prerequisites, then run `make` in this directory. `stream-extract` executable should appear.

`bin/stream-extract` - precompiled on Ubuntu.