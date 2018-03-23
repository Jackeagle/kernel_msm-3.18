#include <iostream>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <cmath>
#include <fstream>
#include <string.h>
#include <unistd.h>

int main (){

	int wiringPiSetup (void);
	int fd = wiringPiI2CSetup (0x45);
	int i=1;

	uint16_t manuID = wiringPiI2CReadReg16 (fd,0x7E);
	uint16_t deviceID = wiringPiI2CReadReg16 (fd,0x7F);
	uint16_t result = 0;
	uint16_t byte1 = 0;
	uint16_t byte2 = 0;
	float lsbsize = 0;
	float lux = 0;
	uint16_t exponent = 0;
	uint16_t fractional = 0;
	char command[100];

	std::fstream myfile;

	std::cout << manuID << std::endl;
	std::cout << deviceID << std::endl;

	wiringPiI2CWriteReg16 (fd,0x01,0x00CC);
	delay (2000);

	uint16_t setconfig = 0x0000;
	setconfig = wiringPiI2CReadReg16 (fd,0x01);
	byte1 = (setconfig&0xFF00);
        byte2 = (setconfig&0x00FF);
        byte1 >>= 8;
        byte2 <<= 8;
	setconfig = (byte1|byte2);
	std::cout << "Set Configuration = " << setconfig << std::endl;

	strcpy( command, "scp /home/pi/Code/opt3001/lightdata.txt teamrobopot@192.168.43.112:/home/teamrobopot/Documents" );

	while(1){

	sleep(5);
	result = wiringPiI2CReadReg16 (fd,0x00);;
	byte1 = (result&0xFF00);
	byte2 = (result&0x00FF);
	byte1 >>= 8;
	byte2 <<= 8;
	result = (byte1|byte2);

	exponent = (result&0xF000);
	fractional = (result&0x0FFF);
	exponent >>= 12;
	lsbsize = (0.01*(pow(2,exponent)));
	lux = (lsbsize*fractional);

	std::cout << "Lux Value = " << lux << std::endl;

	myfile.open ("/home/pi/Code/opt3001/lightdata.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);
	myfile << "Reading " << i << " = " << lux;
	myfile.close();

	system(command);

	i++;
	}

	return 0;
}


