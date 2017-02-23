# Arduino_FreeRTOS_Shell
A simple shell, build on top of FreeRTOS

The development is done on top of Arduino Mega 2560 Rev3 with an HX8357B 3.2" TFT LCD. It uses an SD/MicroSD flash card for file storage. FreeRTOS 9 is currently used as a platform's OS.

This shell is very simple, due to running on top of a 8-bit MCU.
Currently implemented commands are:
```
help
reset
ps (displaying the process's high watermark)
```
Dependencies:
- Arduino FreeRTOS package
- https://github.com/Bodmer/TFT_HX8357, cloned to the libraries directory
