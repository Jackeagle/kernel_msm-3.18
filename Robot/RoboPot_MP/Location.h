#ifndef Location_H
#define Location_H

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"


#define NORTH 1
#define EAST 2
#define SOUTH 3
#define WEST 4

class Location
{
public:
	Location();
	~Location();
	int Setup();
	char Find_Pot();
	void Find_Path(char Destination, char *Pot, int *Turn_1, int *Turn_2, int *Move_x, int *Move_y);
	int Find_Direction();
	int Find_Proximity();
private:

	
	void Find_coordinates(char ref, int *x, int *y);
};

#endif