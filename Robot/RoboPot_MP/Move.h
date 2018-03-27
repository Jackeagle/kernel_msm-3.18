#ifndef Move_H
#define Move_H

#include <stdio.h>
#include <wiringPi.h>
#include <pthread.h>
#include "Project_Config.h"
#include "Location.h"

class Move
{
public:
	Move();
	~Move();
	int Setup();
	void Drive();
	void Turn();

private:

};

#endif
