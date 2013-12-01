//
//  sdl.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include <SDL2/SDL.h>

#include "sdl.h"

namespace Rnes {

void callback(void *userData, Uint8 *stream, int len)
{
    Sdl *sdl = (Sdl*)userData;
    sdl->callbackWrapper(stream, len);
}

void Sdl::callbackWrapper(uint8_t *stream, int len)
{
    if (audioCallback) {
        audioCallback(callbackData, stream, len);
    }
}

void Sdl::registerAudioCallback(Callback *cb, void *data)
{
    callbackData = data;
    audioCallback = cb; 
    SDL_PauseAudio(0);
}

void Sdl::unregisterAudioCallback()
{
    SDL_PauseAudio(1);
    audioCallback = nullptr; 
    callbackData = nullptr;  
}

uint32_t Sdl::getSampleRate()
{
    return audioFreq;
}

uint32_t Sdl::getChunkSize()
{
    return audioBufferSize;
}

int Sdl::initDisplay()
{
    window = SDL_CreateWindow("rnes",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              displayWidth,
                              displayHeight,
                              0);
    renderer = SDL_CreateRenderer(window,
                                  -1, 
                                  SDL_RENDERER_PRESENTVSYNC | 
                                  SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                renderWidth,
                                renderHeight);
    SDL_RenderSetLogicalSize(renderer, renderWidth, renderHeight);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    image = new uint32_t[renderWidth * renderHeight];

    return 0;
}

int Sdl::initAudio()
{
    SDL_AudioSpec desired, obtained;

    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 2048;
    desired.callback = callback;
    desired.userdata = this;

    if (SDL_OpenAudio(&desired, &obtained) < 0) {
        printf("Fail\n");
        return 1;
    }

    audioFreq = obtained.freq;
    audioBufferSize = obtained.samples;
    
    return 0;
}

Sdl::Sdl()
{
    int ret;
    
    ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (ret < 0) {
        return;
    }

    if (initDisplay() < 0) {
        return;
    }

    if (initAudio() < 0) {
        return;
    }
}

Sdl::~Sdl()
{
    SDL_FreeSurface(display);
    SDL_CloseAudio();
}

void Sdl::setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t pixel =  0;
    pixel |= 255 << 24;
    pixel |= (uint32_t)r << 16;
    pixel |= (uint32_t)g << 8;
    pixel |= (uint32_t)b << 0;
    image[x + y * renderWidth] = pixel;
}

void Sdl::renderSync()
{
    SDL_UpdateTexture(texture, NULL, image, renderWidth * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void Sdl::parseInput()
{
    SDL_Event event;
    int button;
     
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_UP:
                    case SDLK_w:
                        button = BUTTON_UP;
                        break;
                    case SDLK_DOWN:
                    case SDLK_s:
                        button = BUTTON_DOWN;
                        break;
                    case SDLK_LEFT:
                    case SDLK_a:
                        button = BUTTON_LEFT;
                        break;
                    case SDLK_RIGHT:
                    case SDLK_d:
                        button = BUTTON_RIGHT;
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        button = BUTTON_START;
                        break;
                    case SDLK_RSHIFT:
                    case SDLK_LSHIFT:
                        button = BUTTON_SELECT;
                        break;
                    case SDLK_z:
                    case SDLK_n:
                        button = BUTTON_A;
                        break;
                    case SDLK_x:
                    case SDLK_m:
                        button = BUTTON_B;
                        break;
                    default:
                        continue;
                }
                if (event.type == SDL_KEYDOWN) {
                    buttonState[button] = true;
                }
                else {
                    buttonState[button] = false;
                }
                break;
            case SDL_QUIT:
                exit(1);
                break;
            default:
                break;
        }
    }
}

bool Sdl::getButtonState(int button)
{
    return buttonState[button];
}

};
