# Upgrade from 2.0 to 3.0

While developing version 3.0 we made some breaking changes to the public
API of this library. This document will help you update your code to work with
version 3.0

## Font Definitions

To get better performance and a smaller font definition format, we change the memory
layout of the font definition format. If you are using custom fonts not included in
this library we updated the font generator [here](http://oleddisplay.squix.ch/#/home).
Please update your fonts to be working with 3.0 by selecting the respective version in the dropdown.


## Architectural Changes

To become a more versatile library for the SSD1306 chipset we abstracted the
hardware connection into subclasses of the base display class now called `OLEDDisplay`.
This library is currently shipping with three implementations:

  * `SSD1306Wire` implementing the I2C protocol using the Wire Library.    
  * `SSD1306Brzo` implementing the I2C protocol using the faster [`brzo_i2c`](https://github.com/pasko-zh/brzo_i2c) library.
  * `SSD1306Spi` implementing the SPI protocol.

To keep backwards compatiblity with the old API `SSD1306` is an alias of `SSD1306Wire`.
If you are not using the UI components you don't have to change anything to keep your code working.

## Name Changes

[Naming things is hard](http://martinfowler.com/bliki/TwoHardThings.html), to better reflect our intention with this library
we changed the name of the base class to `OLEDDisplay` and the UI library accordingly to `OLEDDisplayUi`.
As a consequence the type definitions of all frame and overlay related functions changed.
This means that you have to update all your frame drawing callbacks from:

```c
bool frame1(SSD1306 *display,  SSD1306UiState* state, int x, int y);
```

too

```c
void frame1(OLEDDisplay *display,  OLEDDisplayUiState* state, int16_t x, int16_t y);
```

And your overlay drawing functions from:

```c
bool overlay1(SSD1306 *display,  SSD1306UiState* state);
```

too

```c
void overlay1(OLEDDisplay *display,  OLEDDisplayUiState* state);
```

## New Features

### Loading Animation

While using this library ourself we noticed a pattern emerging. We want to drawing
a loading progress while connecting to WiFi and updating weather data etc.

The simplest thing was to add the function `drawProgressBar(x, y, width,  height, progress)`
,where `progress` is between `0` and `100`, right to the `OLEDDisplay` class.

But we didn't stop there. We added a new feature to the `OLEDDisplayUi` called `LoadingStages`.
You can define your loading process like this:

```c++
LoadingStage loadingStages[] = {
  {
    .process = "Connect to WiFi",
    .callback = []() {
      // Connect to WiFi
    }
  },
  {
    .process = "Get time from NTP",
    .callback = []() {
      // Get current time via NTP
    }
  }
  // more steps
};

int LOADING_STAGES_COUNT = sizeof(loadingStages) / sizeof(LoadingStage);
```

After defining your array of `LoadingStages` you can then run the loading process by using
`ui.runLoadingProcess(loadingStages, LOADING_STAGES_COUNT)`. This will give you a
nice little loading animation you can see in the beginning of [this](https://vimeo.com/168362918)
video.

To further customize this you are free to define your own `LoadingDrawFunction` like this:

```c
void myLoadingDraw(OLEDDisplay *display, LoadingStage* stage, uint8_t progress) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  // stage->process contains the text of the current progress e.q. "Connect to WiFi"
  display->drawString(64, 18, stage->process);
  // you could just print the current process without the progress bar
  display->drawString(64, 28, progress);
}
```

After defining a function like that, you can pass it to the Ui library by use
`ui.setLoadingDrawFunction(myLoadingDraw)`.


### Text Logging

It is always useful to display some text on the display without worrying to much
where it goes and managing it. In 3.0 we made the `OLEDDisplay` class implement
[`Print`](https://github.com/arduino/Arduino/blob/master/hardware/arduino/avr/cores/arduino/Print.h)
so you can use it like you would use `Serial`. We calls this feature `LogBuffer`
and the only thing you have to do is to define how many lines you want to display
and how many characters there are on average on each. This is done by calling
`setLogBuffer(lines, chars);`. If there is not enough memory the function will
return false.

After that you can draw the `LogBuffer` anywhere you want by calling `drawLogBuffer(x, y)`.
(Note: You have to call `display()` to update the screen)
We made a [video](https://www.youtube.com/watch?v=8Fiss77A3TE) showing this feature in action.
