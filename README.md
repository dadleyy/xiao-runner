# XIAO Runner

A project incoporating the [xiao esp32c3 microcontroller][xiao] and some neopixel
lights. [Demo on youtube](https://youtu.be/LCHULixg3cw).

| |
| --- |
| <img width="400px" src="https://user-images.githubusercontent.com/1545348/194978649-c66d7afd-d38d-4184-83a0-80cc86937910.jpg" /> |

## Hardware

| Quantity | Name | Price (per each) |
| --- | --- | --- |
| 2 | [xiao esp32c3][xiao] | ~$5.00 |
| 1 | [adafruit rugged metal pushbutton][pushbutton] | ~$5.00 |
| 1 | [adafruit thumbstick][thumbstick] | ~$2.50 |
| 1 | [adafruit thumbstick breakout][breakout] | ~$1.50 |
| 1 | [ws2812b led light strip][led] | ~$15.00 |

## Compiling

This project uses [platformio] for compilation and dependency management. Once installed, the project's two main
applications can be compiled and flashed onto esp32c3 devices using the cli:

```
$ pio run -t upload
```

> Note: there are two separate platformio projects in this repository -
> 1. the controller code @ [`src/xiao-controller`]
> 1. the "sever"/light code @ [`src/xiao-lights`]

## Inspiration

This project was inspired by the [line wobbler](https://wobblylabs.com/projects/wobbler) project; a much more
impressive + featured version that is available at the [wonderville](https://www.wonderville.nyc/) bar in nyc.

Additionally, the code here is meant to explore c++ move semantics, and the use of c++17's [`variant`][variant]
type, which provides a similar design experience to enumerated types in other languages (e.g, rust).

[xiao]: https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html
[platformio]: https://platformio.org/
[pushbutton]: https://www.adafruit.com/product/481
[thumbstick]: https://www.adafruit.com/product/2765
[breakout]: https://www.adafruit.com/product/3246
[led]: https://www.amazon.com/gp/product/B09PBJ92FV
[variant]: https://en.cppreference.com/w/cpp/utility/variant
[`src/xiao-controller`]: ./src/xiao-controller
[`src/xiao-lights`]: ./src/xiao-lights
