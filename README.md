Very bare NES emulator for linux. Written in c++11 and using sdl for video and sound. Only tested on a few mmc0 games at this point.

![alt text](http://imgur.com/5TjWNVa "Super Mario Brothers")

## Build requirements:
    * SDL 1.2
    * gcc >= 4.6

## To build:
    $ cd rnes
    $ RELEASE=1 make -j8

# To run:
    cd rnes
    ./bin/rnes -r <rom_of_your_choice>

# Controls:
    Start - Enter
    Select - Shift
    A - "n" key
    B - "m" key
    DPAD - arrow keys or FPS standard "wsad" keys

