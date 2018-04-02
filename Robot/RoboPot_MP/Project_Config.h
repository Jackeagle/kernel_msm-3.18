#ifndef Project_Config_H
#define Project_Config_H

#include <string>
#include <wiringPi.h>
#include <pthread.h>

//Project Configuration File

//-------------------------------------------
//-----GUI Text File Configuration-----------
//-------------------------------------------

const std::string SetupMessage = "Robot Initialised";
const std::string SetupFile = "/var/www/html/Setupfile.txt";
const std::string cmdfile = "/var/www/html/commandfile.txt";
const std::string ToGUI = "/var/www/html/GUImessagefile.txt";


//-------------------------------------------
//-------Motor Speed Configuration-----------
//-------------------------------------------

const int PWM_Set_Stop = 0;
const int PWM_Set_Slow = 700;
const int PWM_Set_Medium = 900;
const int PWM_Set_Fast = 1000;

//-------------------------------------------
//-------------Pin Configuration-------------
//-------------------------------------------
//Define Motor Pins

const int Left_High_Motor = 26;    //p32
const int Left_Low_Motor = 23;    //p23
const int Right_High_Motor = 10;    //p24
const int Right_Low_Motor = 11;    //p26

//Define Proximity Sensor Pins
//API Defined

//Define Magnemometer Sensor Pins


#endif // !Project_Config_H

