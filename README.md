PaperFrame, an E-Paper photo frame
==================================

**Photo frame built on an ESP32 with an E-Paper display. The picture shown in the frame can be changed from a smartphone over a WiFi web interface**

Detailed explaination on [my website](https://www.bdeliers.com/e-paper-photo-frame/).

![PaperFrame is nice !](https://www.bdeliers.com/wp-content/uploads/2023/12/IMG_20231231_134519.jpg)

## Bill Of materials

For this project, I used only existing modules from Waveshare and some spare parts from my drawers.

- **7.5inch E-Paper (B) E-Ink from Waveshare**
    - black/white/red colors
    - 480x800px resolution
- **Universal e-Paper Raw Panel Driver Board from Waveshare too**
    - ESP32 MCU from Espressif
    - 4MB ROM & 512kB RAM
    - WiFi and BLE capabilities
    - 3.3v or 5v power
    - E-Paper driving circuit included
- **3x AA battery handler**
    - Provides 4.5v, enough to make it work
- **Any 13x18cm photo frame you like**

## Software

All the MCU software is written in C, based on the excellent ESP-IDF. To avoid all the IDF installation, I used a devcontainer on VSCode, thanks to [this guy](https://gist.github.com/bwrrp/dc2fe8926dfe8860da21cb87ba91aeaa)! On the Web side, I used the standard trio HTML5/CSS3/JavaSript.

### Image processing

The image taken from a camera is very different from the one an E-Paper display needs. My display needs a 480×800 pixels frame, with colors encoded on 2 bits (black/white and red/non-red). As the input image can be very large, it’s difficult to send it entirely to the ESP32 to process it. Then I decided to process the image in the browser, with some Javascript code.

First the image is rotated to landscape format if it’s higher than larger. The script crops it to a 5:3 ratio as on the display then downsizes it to a width of 800 pixels. Now that the image has the right size, the last step is to convert it to our very reduced colorspace. To do so, I used the Floyd-Steinberg dithering algorithm after the quantization process to have a nice result.

**&copy; BDeliers - 2023** \
**Under Apache-2 License**