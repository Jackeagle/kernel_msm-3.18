#include "Move.h"



Move::Move()
{
}


Move::~Move()
{
}

void Move::Drive()
{
	//Move one unit space 2m/4units= 0.5m=500mm
	Location unit;
	int start = unit.Find_Proximity();
	while ((unit.Find_Proximity()-start )< 50) {
		pwmWrite(Left_High_Motor, PWM_Set_Slow);
		pwmWrite(Right_High_Motor, PWM_Set_Slow);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	while ((unit.Find_Proximity() - start) < 100) {
		pwmWrite(Left_High_Motor, PWM_Set_Medium);
		pwmWrite(Right_High_Motor, PWM_Set_Medium);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	while ((unit.Find_Proximity() - start) < 400) {
		pwmWrite(Left_High_Motor, PWM_Set_Fast);
		pwmWrite(Right_High_Motor, PWM_Set_Fast);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	while ((unit.Find_Proximity() - start) < 450) {
		pwmWrite(Left_High_Motor, PWM_Set_Medium);
		pwmWrite(Right_High_Motor, PWM_Set_Medium);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	while ((unit.Find_Proximity() - start) < 500) {
		pwmWrite(Left_High_Motor, PWM_Set_Slow);
		pwmWrite(Right_High_Motor, PWM_Set_Slow);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	pwmWrite(Left_High_Motor, PWM_Set_Stop);
	pwmWrite(Right_High_Motor, PWM_Set_Stop);
	digitalWrite(Left_Low_Motor, 0);
	digitalWrite(Right_Low_Motor, 0);
}

void Move::Turn()
{

}

int Move::Setup()
{
	//Initialise Wiring Pi
	if (wiringPiSetup() == -1)
	{
		printf("Wiring Pi Setup Failed \n");
		return -1;
	}
	//Set Motor Pin Configuration
	pinMode(Left_High_Motor, PWM_OUT);
	pinMode(Left_Low_Motor, OUTPUT);
	pinMode(Right_High_Motor, PWM_OUT);
	pinMode(Right_Low_Motor, OUTPUT);
	
	//Motor Initialisation Values
	pwmWrite(Left_High_Motor, PWM_Set_Stop);
	pwmWrite(Right_High_Motor, PWM_Set_Stop);
	digitalWrite(Left_Low_Motor, 0);
	digitalWrite(Right_Low_Motor, 0);

	return 0;

}