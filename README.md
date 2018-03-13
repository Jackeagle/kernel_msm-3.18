# RoboPot
We are creating a web-based, plant pot positioning system with integrated light sensing features.  The plant pot will be controlled via a web based GUI - HTML and JavaScript based - with ambient light sensors in order to determine the optimum positioning of the plant pot for maximum illumination. 

### System Description

Robopot allows the keen, technically minded gardener to position their beloved plants wherever they wish within the vicinity of their patio or small garden.

The RoboPot graphical user interface will be accessable via a website which provides data on lighting conditions in the garden and provides plant pot positioning functionality. To move a plant pot to the desired location, simply click on the robopot icon and click on the new robopot location. Now sit back and watch in amazement as the icon on the GUI and RoboPot in the garden moves to the optimum position in the garden as decided by yourself.

Thanks for taking the time to have a look at the Robopot hardware and software designs. We hope this inspires you to take your imaginative ideas and apply them in Robotic projects!


![alt text](https://github.com/FrazLaw/RoboPot/blob/master/RoboPot.png)

# HARDWARE SCHEMATICS

### Light Sensor PCB

![alt text](https://github.com/FrazLaw/RoboPot/blob/master/schematic-lightsensor.png)

### Magnetometer Schematic
So, for us to determine in which direction our mini-movable plant pot is facing, we needed to add a magnetometer. This enables us to then feedback to the GUI and indicate which position we are at. This feedback network, in conjuction with the proximity sensor, is essential ot compile the information into a reliable position. 
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/SchematicPic.PNG)

### Motor Driver
An essential part of the system was to move the plant pot itself. To do this we will demonstrate using a small motor car which will be controlled via this motor driver circuit:




