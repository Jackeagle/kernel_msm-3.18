#include <wiringPi.h>
#include <stdio.h>
#include <pthread.h>

/*
Remember to include pthread library
cc -o myprog myprog.c -lwiringPi -lpthread

https://pinout.xyz/
http://wiringpi.com/reference/core-functions/

*/

int main(void)
{
	printf("Robopot Motors PWM Test\n");
	
	//Declare Pins
	pinMode (26, PWM_OUTPUT); //p32
	pinMode (23, PWM_OUTPUT); //p23
	pinMode (10, OUTPUT); //p24
	pinMode (11, OUTPUT); //p26
	
	
	
	//PWM Settings
	int pin1=26;
	int pin0=23;
	int pin24=10;
	int pin26=11;
	int PWM_set=700; //0-1024
	
	
	
	
	while(1)
	{
		printf("Enter PWM setting for Motors 0-1024");
		scanf("%d", &PWM_set);
		printf("Motors set to %d \n", PWM_set);
		pwmWrite(pin1, PWM_set);
		pwmWrite(pin0, PWM_set);
		digitalWrite(pin26, 0);
		digitalWrite(pin24, 0);	
		digitalWrite(pin1, 0);
		digitalWrite(pin0, 0);
	}
	
}
