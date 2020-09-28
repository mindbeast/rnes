//
//  sdl.h
//  rnes
//

#pragma once

#include <cstdint>

class SDL_Surface;
class SDL_Window;
class SDL_Renderer;
class SDL_Texture;

namespace Rnes {

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
    BUTTON_SAVE,
    BUTTON_RESTORE,
    BUTTON_COUNT,
  };

private:
  SDL_Surface *display = nullptr;
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  uint32_t *image;
  static constexpr int dispMultiple = 4;
  static constexpr int renderWidth = 256;
  static constexpr int renderHeight = 240;
  static constexpr int displayHeight = dispMultiple * renderHeight;
  static constexpr int displayWidth = dispMultiple * renderWidth;
  //static constexpr int bitsPerPixel;
  bool buttonState[BUTTON_COUNT] = {false};
  typedef void(Callback)(void *data, uint8_t *stream, int len);
  Callback *audioCallback;
  void *callbackData;
  uint32_t audioFreq;
  uint32_t audioBufferSize;

public:
  Sdl();
  int initAudio();
  int initDisplay();
  ~Sdl();

  // Video functions
  void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
  void renderSync();
  void parseInput();
  bool getButtonState(int button);

  // Audio functions
  void callbackWrapper(uint8_t *stream, int len);
  void registerAudioCallback(Callback *audioCallback, void *data);
  void unregisterAudioCallback();
  uint32_t getSampleRate();
  uint32_t getChunkSize();
};

}; // namespace Rnes
