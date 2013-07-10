//
//  sdl.h
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __SDL_H__
#define __SDL_H__

#include <SDL/SDL.h>
#include <cassert>

class Sdl {
public:
    enum {
        BUTTON_UP,
        BUTTON_DOWN,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_START,
        BUTTON_SELECT,
        BUTTON_A,
        BUTTON_B,
        BUTTON_COUNT,
    };

private:
    SDL_Surface *display = nullptr;
    static const int dispMultiple = 2;
    static const int renderWidth = 256;
    static const int renderHeight = 240;
    static const int displayHeight = dispMultiple * renderHeight;
    static const int displayWidth = dispMultiple * renderWidth;
    static const int bitsPerPixel;
    bool buttonState[BUTTON_COUNT] = {false};
    
public:
    Sdl() {}
    int init() {
        int ret, flags;
        const SDL_VideoInfo *info;
        
        ret = SDL_Init(SDL_INIT_EVERYTHING);
        if (ret < 0) {
            return -1;
        }
        
        info = SDL_GetVideoInfo();
        if (!info) {
            return -1;
        }
        
        flags = SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_ASYNCBLIT;
        display = SDL_SetVideoMode(displayWidth, displayHeight, 32, flags);
        if (!display) {
            return -1;
        }
        
        SDL_WM_SetCaption("rnes", "rnes");
        
        return 0;
    }
    void preRender() {
        SDL_LockSurface(display);
    }
    void postRender() {
        SDL_UnlockSurface(display);
    }
    void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
    {
        assert(SDL_MUSTLOCK(display));
        assert(display->format->BytesPerPixel == 4);
        uint32_t color = SDL_MapRGB(display->format, r, g, b);
        for (int i = 0; i < dispMultiple; i++) {
            for (int j = 0; j < dispMultiple; j++) {
                uint32_t *ptr;
                ptr = (uint32_t*)display->pixels + x * dispMultiple + j + (y * dispMultiple + i) * (display->pitch / 4);
                *ptr = color;
            }
        }
    }
    void renderSync()
    {
        SDL_Flip(display);
    }
    void parseInput();
    bool getButtonState(int button);
    
    ~Sdl()
    {
        SDL_FreeSurface(display);
    }
};

#endif
