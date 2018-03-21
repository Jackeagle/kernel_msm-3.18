# RoboPot
We are creating a web-based, plant pot positioning system with integrated light sensing features.  The plant pot will be controlled via a web based GUI - HTML and JavaScript based - with ambient light sensors in order to determine the optimum positioning of the plant pot for maximum illumination. 

### System Description

Robopot allows the keen, technically minded gardener to position their beloved plants wherever they wish within the vicinity of their patio or small garden.

The RoboPot graphical user interface will be accessable via a website which provides data on lighting conditions in the garden and provides plant pot positioning functionality. To move a plant pot to the desired location, simply click on the robopot icon and click on the new robopot location. Now sit back and watch in amazement as the icon on the GUI and RoboPot in the garden moves to the optimum position in the garden as decided by yourself.

Thanks for taking the time to have a look at the Robopot hardware and software designs. We hope this inspires you to take your imaginative ideas and apply them in Robotic projects!


![alt text](https://github.com/FrazLaw/RoboPot/blob/master/RoboPot.png)

# Software Requirements

### Robot Control Software Requirements

1- Implement I2C communication protocols to retrieve data from magnemometer and proximity sensor.

2- Implement PWM and GPIO signals to interface with motor control circuitry and control robot motor speed.

3- Use data from magnemometer and proximity sensor to calculate robot position in garden.

4- Read text file to retrieve robot repositioning request and desired location.

5- Write to text file to allow GUI update of robot position.

### GUI Software Requirements

1- GUI to be accessable from a web page hosted on Apache server on Raspberry Pi.

2- Use HTML, PHP and Javascript to create webpage.

3- Provide buttons to allow user to set requested position of robot.

4- Write to text file to allow robot to be updated on requested robot position.

5- Read from text file to update GUI on present robot location and ambient light levels.

6- Display the location of the robot in garden.

7- Display ambient light levels in Lux of ambient light sensors.

### Garden Sensor Software Requirements

1- Implement I2C communication protocols to retrieve data from ambient light sensor.

2- Use SSH to access Raspberry Pi CLI.

3- Write to text file to update GUI on ambient light levels.

# Hardware Requirements

### Robot Hardware Requirements

1- Raspberry Pi to provide read sensor, motor control, wifi communication and web server hosting functionality.

2- DC Motors to actuate robot wheels.

3- Use STM VL53L0X chip as proximity sensor.

4- Use STM LSM303DLHC chip as magnemometer.

5- Use STM L298 chip as motor driver

6- Provide power supplies for Raspberry Pi and motor driver circuitry. Other Sensors shall take power from raspberry pi.

7- Use 9V battery as power supply.

### Garden Sensor Hardware Requirements

1- Use Raspberry Pi Zero to provide read sensor and wifi communication functionality.

2- Use a collection of sensor modules to gather data from around the garden area.

3- Use a garden hub PCB to interface sensor modules to Raspberry Pi Zero.

4- Use Texas Instruments OPT3001 as ambient light sensor.

5- Use NXP P82B715 as I2C buffer chips.

6- Use 5V mini USB wall socket power supply to power system.

7- Use ethernet cables to conduct signals between sensor modules and garden hub.

# Robot Chassis

The Robot Chassis and motors were purchased [here!](https://www.amazon.co.uk/dp/B00GLO5SMY/ref=pe_3187911_189395841_TE_3p_dp_1)

![alt text](https://github.com/FrazLaw/RoboPot/blob/master/RobotChassis.png)

# HARDWARE SCHEMATICS

### Ambient Light Sensors
Light sensors are used to measure the ambient light reaching each area of the garden. In our demo, four light sensors are used, with the garden area divided into four quadrants. Each light sensor PCB uses the OPT3001 ambient light sensor module and an I2C range extender IC, the P82B715. The I2C data communication and power supply connections to the central hub are achieved through standard ethernet cables. The schematic of this light sensor PCB is shown below:
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/lightsensor2_sch.png)

### Garden Hub
The garden hub uses a Raspberry Pi Zero to receive the I2C data from all the remote ambient light sensors. The connections to the remote light sensor modules are made with standard ethernet cables. The I2C bus and the ambient light sensors are powered from the Pi's 3.3V output. The program on the Pi Zero collates the data from the sensors and deposits it into a file to be sent to the RoboPot Pi. The data can then be graphically displayed on the GUI webpage to the user. The PCB of the garden hub is as shown as below:
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/zerohub2_sch.png)

### Magnetometer Schematic
So, for us to determine in which direction our mini-movable plant pot is facing, we needed to add a magnetometer. This enables us to then feedback to the GUI and indicate which position we are at. This feedback network, in conjuction with the proximity sensor, is essential ot compile the information into a reliable position. 
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/SchematicPic.PNG)

### Proximity Sensor
The proximity sensor PCB gives the robot the ability to recognise the distance between its front face and a garden wall. The schematic for this sensor is given here:
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/VL53L0XPCB_Rev2_Schematic.png)

### Motor Driver
An essential part of the system was to move the plant pot itself. To do this we will demonstrate using a small motor car which will be controlled via this motor driver circuit:
![alt text](https://github.com/FrazLaw/RoboPot/blob/master/Hardware/SchematicMotor.PNG)



