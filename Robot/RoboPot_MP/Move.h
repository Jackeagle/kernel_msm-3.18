#ifndef Move_H
#define Move_H

#include <stdio.h>
#include <wiringPi.h>
#include <pthread.h>
#include "Project_Config.h"
#include "Location.h"
#include <cmath>

class Move
{
public:
	Move();
	~Move();
	int Initialise(); //Initialises wiringPi
	void Drive(); ////Move one unit space 2m/4units= 0.5m=500mm
	void Turn(int Target_Bearing); //Turns the Robot to the Target Bearing

private:
	char Turn_LeftorRight(int Target_Bearing, int Current_Bearing);
};

#endif
