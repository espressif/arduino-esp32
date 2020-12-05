/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef OLEDDISPLAYUI_h
#define OLEDDISPLAYUI_h

#include <Arduino.h>
#include "OLEDDisplay.h"

//#define DEBUG_OLEDDISPLAYUI(...) Serial.printf( __VA_ARGS__ )

#ifndef DEBUG_OLEDDISPLAYUI
#define DEBUG_OLEDDISPLAYUI(...)
#endif

enum AnimationDirection {
  SLIDE_UP,
  SLIDE_DOWN,
  SLIDE_LEFT,
  SLIDE_RIGHT
};

enum IndicatorPosition {
  TOP,
  RIGHT,
  BOTTOM,
  LEFT
};

enum IndicatorDirection {
  LEFT_RIGHT,
  RIGHT_LEFT
};

enum FrameState {
  IN_TRANSITION,
  FIXED
};


const char ANIMATION_activeSymbol[] PROGMEM = {
  0x00, 0x18, 0x3c, 0x7e, 0x7e, 0x3c, 0x18, 0x00
};

const char ANIMATION_inactiveSymbol[] PROGMEM = {
  0x00, 0x0, 0x0, 0x18, 0x18, 0x0, 0x0, 0x00
};


// Structure of the UiState
struct OLEDDisplayUiState {
  uint64_t     lastUpdate                = 0;
  uint16_t      ticksSinceLastStateSwitch = 0;

  FrameState    frameState                = FIXED;
  uint8_t       currentFrame              = 0;

  bool          isIndicatorDrawen         = true;

  // Normal = 1, Inverse = -1;
  int8_t        frameTransitionDirection  = 1;

  bool          manuelControll            = false;

  // Custom data that can be used by the user
  void*         userData                  = NULL;
};

struct LoadingStage {
  const char* process;
  void (*callback)();
};

typedef void (*FrameCallback)(OLEDDisplay *display,  OLEDDisplayUiState* state, int16_t x, int16_t y);
typedef void (*OverlayCallback)(OLEDDisplay *display,  OLEDDisplayUiState* state);
typedef void (*LoadingDrawFunction)(OLEDDisplay *display, LoadingStage* stage, uint8_t progress);

class OLEDDisplayUi {
  private:
    OLEDDisplay             *display;

    // Symbols for the Indicator
    IndicatorPosition   indicatorPosition         = BOTTOM;
    IndicatorDirection  indicatorDirection        = LEFT_RIGHT;

    const char*         activeSymbol              = ANIMATION_activeSymbol;
    const char*         inactiveSymbol            = ANIMATION_inactiveSymbol;

    bool                shouldDrawIndicators      = true;

    // Values for the Frames
    AnimationDirection  frameAnimationDirection   = SLIDE_RIGHT;

    int8_t              lastTransitionDirection   = 1;

    uint16_t            ticksPerFrame             = 151; // ~ 5000ms at 30 FPS
    uint16_t            ticksPerTransition        = 15;  // ~  500ms at 30 FPS

    bool                autoTransition            = true;

    FrameCallback*      frameFunctions;
    uint8_t             frameCount                = 0;

    // Internally used to transition to a specific frame
    int8_t              nextFrameNumber           = -1;

    // Values for Overlays
    OverlayCallback*    overlayFunctions;
    uint8_t             overlayCount              = 0;

    // Will the Indicator be drawen
    // 3 Not drawn in both frames
    // 2 Drawn this frame but not next
    // 1 Not drown this frame but next
    // 0 Not known yet
    uint8_t                indicatorDrawState        = 1;

    // Loading screen
    LoadingDrawFunction loadingDrawFunction       = [](OLEDDisplay *display, LoadingStage* stage, uint8_t progress) {
      display->setTextAlignment(TEXT_ALIGN_CENTER);
      display->setFont(ArialMT_Plain_10);
      display->drawString(64, 18, stage->process);
      display->drawProgressBar(4, 32, 120, 8, progress);
    };

    // UI State
    OLEDDisplayUiState      state;

    // Bookeeping for update
    uint8_t             updateInterval            = 33;

    uint8_t             getNextFrameNumber();
    void                drawIndicator();
    void                drawFrame();
    void                drawOverlays();
    void                tick();
    void                resetState();

  public:

    OLEDDisplayUi(OLEDDisplay *display);

    /**
     * Initialise the display
     */
    void init();

    /**
     * Configure the internal used target FPS
     */
    void setTargetFPS(uint8_t fps);

    // Automatic Controll
    /**
     * Enable automatic transition to next frame after the some time can be configured with `setTimePerFrame` and `setTimePerTransition`.
     */
    void enableAutoTransition();

    /**
     * Disable automatic transition to next frame.
     */
    void disableAutoTransition();

    /**
     * Set the direction if the automatic transitioning
     */
    void setAutoTransitionForwards();
    void setAutoTransitionBackwards();

    /**
     *  Set the approx. time a frame is displayed
     */
    void setTimePerFrame(uint16_t time);

    /**
     * Set the approx. time a transition will take
     */
    void setTimePerTransition(uint16_t time);

    // Customize indicator position and style

    /**
     * Draw the indicator.
     * This is the defaut state for all frames if
     * the indicator was hidden on the previous frame
     * it will be slided in.
     */
    void enableIndicator();

    /**
     * Don't draw the indicator.
     * This will slide out the indicator
     * when transitioning to the next frame.
     */
    void disableIndicator();

    /**
     * Enable drawing of indicators
     */
    void enableAllIndicators();

    /**
     * Disable draw of indicators.
     */
    void disableAllIndicators();

    /**
     * Set the position of the indicator bar.
     */
    void setIndicatorPosition(IndicatorPosition pos);

    /**
     * Set the direction of the indicator bar. Defining the order of frames ASCENDING / DESCENDING
     */
    void setIndicatorDirection(IndicatorDirection dir);

    /**
     * Set the symbol to indicate an active frame in the indicator bar.
     */
    void setActiveSymbol(const char* symbol);

    /**
     * Set the symbol to indicate an inactive frame in the indicator bar.
     */
    void setInactiveSymbol(const char* symbol);


    // Frame settings

    /**
     * Configure what animation is used to transition from one frame to another
     */
    void setFrameAnimation(AnimationDirection dir);

    /**
     * Add frame drawing functions
     */
    void setFrames(FrameCallback* frameFunctions, uint8_t frameCount);

    // Overlay

    /**
     * Add overlays drawing functions that are draw independent of the Frames
     */
    void setOverlays(OverlayCallback* overlayFunctions, uint8_t overlayCount);


    // Loading animation
    /**
     * Set the function that will draw each step
     * in the loading animation
     */
    void setLoadingDrawFunction(LoadingDrawFunction loadingFunction);


    /**
     * Run the loading process
     */
    void runLoadingProcess(LoadingStage* stages, uint8_t stagesCount);


    // Manual Control
    void nextFrame();
    void previousFrame();

    /**
     * Switch without transition to frame `frame`.
     */
    void switchToFrame(uint8_t frame);

    /**
     * Transition to frame `frame`, when the `frame` number is bigger than the current
     * frame the forward animation will be used, otherwise the backwards animation is used.
     */
    void transitionToFrame(uint8_t frame);

    // State Info
    OLEDDisplayUiState* getUiState();

    int8_t update();
};
#endif
