# CCDCameraCookbook

This project is in the begining of developement. It will allow CCD Camera Cookbook builders to
convert their camera to an USB interface. 

The interface and regulator board is replaced with a Teensy4.0 microcontroller that is 
programmed from the Arduino IDE. Sketches are provided for TC211 and TC245 camera versions.
The preamp board requires replacement of the DS0026 drivers with TC1426 drivers.

A Windows application to communicate with the interface board is under developement and a preliminary
executable projectcb.exe is provided. Source code written using the Lazarus IDE will follow as
the project nears completion.

CNC routing files are provided for generating a single sided PC board. The board is 70mm by 100mm.
Several top side jumper wires are needed. The Cookbook connectors from the preamp board plug into
the new interface card.

Simple-Circuit

![](./screen_shot.png)
