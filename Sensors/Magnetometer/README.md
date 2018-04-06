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

To compensate, I centred the parallel rotation around the origin by subrating 307 uT each read of the registers. You can see the change above. This enabled an accurate reading of the sensor with slightly less sensitivty as opposed to the Z-axis being perpendicular to the ground.

To examine your values (if your magnetometer doesn't work first time), I logged my data into a csv which I then transferred to the github, then downloaded into excel and plotted my values. Here is a simple data logging example for creating a simple csv file:
fstream magneto;
```c
ofstream datatest; //datatest is now the output data file
datatest.open("datatest.csv"); //creating the csv file
datatest << "X,Y,Z\n"; //create the columns for each axis read
datatest.close(); //close the file
lsm.configure();

	for (int i = 0; i<100000; i++){//number of output checks
    datatest.open("datatest.csv", fstream::app); //open in the for loop and append the values using "fstream::app"
		float direction; //compass coordingate

		float x = lsm.readCh1()-307;//to centre the plot around 0. It performs as expected with few errors
    datatest << x; //variables from reading the registers
    datatest << y;
    datatest << z;
```
Easy to calibrate! 

Any questions, just pop over to our RoboPot facebook page!
