# RoboPot
We are creating a web-based, plant pot positioning system with integrated light sensing features.  The plant pot will be controlled via a web based GUI - HTML and JavaScript based - with ambient light sensors in order to determine the optimum positioning of the plant pot for maximum illumination. 

### System Description

Robopot allows the keen, technically minded gardener to position their beloved plants wherever they wish within the vicinity of their patio or small garden.

The RoboPot graphical user interface will be accessable via a website which provides data on lighting conditions in the garden and provides plant pot positioning functionality. To move a plant pot to the desired location, simply click on the robopot icon and click on the new robopot location. Now sit back and watch in amazement as the icon on the GUI and RoboPot in the garden moves to the optimum position in the garden as decided by yourself.

Thanks for taking the time to have a look at the Robopot hardware and software designs. We hope this inspires you to take your imaginative ideas and apply them in Robotic projects!


![alt text](https://github.com/FrazLaw/RoboPot/blob/master/RoboPot.png)

# HARDWARE SCHEMATICS

### Ambient Light Sensors
Light sensors are used to measure the ambient light reaching each area of the garden. In our demo, four light sensors are used, with the garden area divided into four quadrants. Each light sensor PCB uses the OPT3001 ambient light sensor module and an I2C range extender IC, the P82B715. The I2C data communication and power supply connections to the central hub are achieved through standard ethernet cables. The schematic of this light sensor PCB is shown below:
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/lightsensor2_sch.png)

### Garden Hub
The garden hub uses a Raspberry Pi Zero to receive the I2C data from all the remote ambient light sensors. The connections to the remote light sensor modules are made with standard ethernet cables. The I2C bus and the ambient light sensors are powered from the Pi's 3.3V output. The program on the Pi Zero collates the data from the sensors and deposits it into a file to be sent to the RoboPot Pi. The data can then be graphically displayed on the GUI webpage to the user. The PCB of the garden hub is as shown as below:
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/zerohub2_sch.png)

### Magnetometer Schematic
So, for us to determine in which direction our mini-movable plant pot is facing, we needed to add a magnetometer. This enables us to then feedback to the GUI and indicate which position we are at. This feedback network, in conjuction with the proximity sensor, is essential ot compile the information into a reliable position. 
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/SchematicPic.PNG)

### Motor Driver
An essential part of the system was to move the plant pot itself. To do this we will demonstrate using a small motor car which will be controlled via this motor driver circuit:
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/SchematicMotor.PNG)




