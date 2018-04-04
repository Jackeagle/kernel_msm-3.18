#ifndef Location_H
#define Location_H

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <cmath>
#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"
#include "Project_Config.h"
#include "Move.h"


class Location
{
public:
	Location();
	~Location();
	int Setup(); //Sets bearings and initial location of pot
	char Find_Pot();	//returns character representing location of Pot
	void Find_Path(char Destination, char Pot_Start_Position, int *X_Bearing, int *Y_BearingY, int *X_PathLength, int *Y_PathLength);
	int Find_Direction(); //Returns present bearing 0-360degrees
	int Find_Proximity(); // Returns distance to front wall
private:
	void Find_coordinates(char ref, int *x, int *y); //converts abcd etc into x and y coordinates
	
	const char Location_ref[4][4] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l','m', 'n', 'o', 'p' };
};

#endif