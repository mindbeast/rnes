//
//  sdl.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include "sdl.h"

void Sdl::parseInput()
{
    SDL_Event event;
    int button;
    /*

    int keys;
    uint8_t *keyState;
    
    SDL_PumpEvents();
    keyState = SDL_GetKeyState(&keys);
    buttonState[BUTTON_UP] = keyState[SDLK_UP];
    buttonState[BUTTON_DOWN] = keyState[SDLK_DOWN];
    buttonState[BUTTON_START] = keyState[SDLK_RETURN];
    */
     
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
