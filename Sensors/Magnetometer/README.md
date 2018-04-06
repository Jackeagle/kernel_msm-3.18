### Magnetometer: Overview

This is the magnetometer explanatory file. This will explain the diferent functions and how everything was made and how it works.

#Chip
The chip used was the LSM303AGR. The chip is an LGA package which can be fairly difficult to solder but with a hot air gun it is do-able. The schematic is as it is on the front page:
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/SchematicPic.PNG)
All components chosen were recommmended by the regulator chip chosen. 
Datasheet: 
http://www.st.com/resource/en/datasheet/lsm303agr.pdf

To read the registers from the chip, which stored the earth's magnetic field data in micro-Telsas, I used wiring pi to handle a lot of the complicated reading and writing. Here is a wiring pi download guide:
http://wiringpi.com/download-and-install/

The code can be found in i2c_test.cpp and to compile to program type:
```
$ make
```
Simple!

An issue that was a real pain in the side was the sensitivity of the chip itself. The datasheet didn't specifiy this; however, the sensor worked a lot better when the Z-axis was perpendicular to the ground and was rotating around th Z-axis. In the plot below you can see the comparison between the perpendicular rotation and the parallel rotation.

![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Comparisonplots.PNG)

To compensate, I centred the parallel rotation around the origin by subrating 307uT each read of the registers. You can see the change above. This enabled an accurate reading of the sensor with slightly less sensitivty as opposed to the Z-axis being perpendicular to the ground.
