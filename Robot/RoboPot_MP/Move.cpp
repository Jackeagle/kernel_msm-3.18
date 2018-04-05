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
	int start_Distance = unit.Find_Proximity();
	while ((unit.Find_Proximity()-start_Distance )< 50) {
		pwmWrite(Left_High_Motor, PWM_Set_Slow);
		pwmWrite(Right_High_Motor, PWM_Set_Slow);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	while ((unit.Find_Proximity() - start_Distance) < 100) {
		pwmWrite(Left_High_Motor, PWM_Set_Medium);
		pwmWrite(Right_High_Motor, PWM_Set_Medium);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	while ((unit.Find_Proximity() - start_Distance) < 400) {
		pwmWrite(Left_High_Motor, PWM_Set_Fast);
		pwmWrite(Right_High_Motor, PWM_Set_Fast);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	while ((unit.Find_Proximity() - start_Distance) < 450) {
		pwmWrite(Left_High_Motor, PWM_Set_Medium);
		pwmWrite(Right_High_Motor, PWM_Set_Medium);
		digitalWrite(Left_Low_Motor, 0);
		digitalWrite(Right_Low_Motor, 0);
	}

	while ((unit.Find_Proximity() - start_Distance) < 500) {
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

void Move::Turn(int Target_Bearing) //Turns the Robot to the Target Bearing
{
	Location Turn;
	int Current_Bearing = Turn.Find_Direction();
	
	char Turn_Direction = Turn.LeftorRight(Target_Bearing, Current_Bearing);
	
	if (Turn_Direction == 'R')
	{
		while (abs(Turn.Find_Direction()-Target_Bearing)>5)
		{
			pwmWrite(Left_High_Motor, PWM_Set_Medium);
			digitalWrite(Left_Low_Motor, 0);
		}
		pwmWrite(Left_High_Motor, PWM_Set_Stop);
		digitalWrite(Left_Low_Motor, 0);
	}else	
	if (Turn_Direction == 'L')
	{
		while (abs(Turn.Find_Direction()-Target_Bearing)>5)
		{
			pwmWrite(Right_High_Motor, PWM_Set_Medium);
			digitalWrite(Right_Low_Motor, 0);
		}
		pwmWrite(Right_High_Motor, PWM_Set_Stop);
		digitalWrite(Right_Low_Motor, 0);
	}
	
}

int Move::Initialise() 		//Initialises wiringPi
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

char Move::Turn_LeftorRight(int Target_Bearing, int Current_Bearing)
{
	
	int a = Target_Bearing-Current_Bearing;
	int b = Target_Bearing-Current_Bearing + 360;
	int c = Target_Bearing-Current_Bearing - 360;
	
	if(abs(a)>abs(b)&& abs(a)>abs(c))
	{
		if(a > 0)
		{
			return 'R'//Turn Right
		}else
		{
			return 'L'//Turn Left
		}
	}else
	if(abs(b)>abs(a)&& abs(b)>abs(c)
	{
		if(b > 0)
		{
			return 'R'//Turn Right
		}else
		{
			return 'L'//Turn Left
		}
	}
	if(abs(c)>abs(a)&& abs(c)>abs(a)
	{
		if(c > 0)
		{
			return 'R'//Turn Right
		}else
		{
			return 'L'//Turn Left
		}
	}
	
}