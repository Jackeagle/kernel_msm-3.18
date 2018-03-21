#include <iostream>
#include <errno.h>
#include <wiringPiI2C.h>
#include <linux/i2c.h>
#include "LSM303AGR.h"
#include <unistd.h>
#include <stdio.h>

#define ACC_ADDR 0x19
#define MAG_ADDR 0x1E
//default addresses
#define STATUS_REG_M 0x67
#define OUTX_L_REG_M 0x68 //lsb
#define OUTX_H_REG_M 0x69
#define OUTY_L_REG_M 0x6A //lsb
#define OUTY_H_REG_M 0x6B
#define OUTZ_L_REG_M 0x6C //lsb
#define OUTZ_H_REG_M 0x6D
#define WHO_AM_I_M  0x4F // device ID
#define CFG_REG_A_M 0x60 //configure address

using namespace std;

LSM303AGR::LSM303AGR()
{
	//initialise i2c
	fd = wiringPiI2CSetup(MAG_ADDR);
	//file driectory = fd
	if(fd<0){

	cout << "initialisation error ya boy. Error number: " << fd << "."<<endl;

	}else{

	cout << "Initial Reslt: " << fd << endl;
	cout << "Device ID: " << wiringPiI2CReadReg8(fd, WHO_AM_I_M) << "\n" << endl;
	}
}

int LSM303AGR::getFileDescriptor()
{
cout << fd << endl;
return fd;

}

int LSM303AGR::dataAvailable(){
int status;
status = wiringPiI2CReadReg8(fd,STATUS_REG_M);//data output X register, not really a status reg
cout << status << endl;
}

int LSM303AGR::configure(){
alastair = wiringPiI2CWriteReg8(fd, CFG_REG_A_M, 0x00);
}

int LSM303AGR::readCh1(){
msbX = wiringPiI2CReadReg8(fd, OUTX_H_REG_M);//68h and 69h for X output registers, 69 is MSB(?)
lsbX = wiringPiI2CReadReg8(fd, OUTX_L_REG_M);
printf("%d", msbX);
printf("\n");
printf("%d", lsbX);
printf("\n\n");
return 0;
//cout <<"\r" <<  msbX << "\n" << lsbX << "\n";
}

int LSM303AGR::readCh2(){
msbY = wiringPiI2CReadReg8(fd, OUTY_H_REG_M);//6Ah and 6Bh for y output registers,
lsbY = wiringPiI2CReadReg8(fd, OUTY_L_REG_M);
printf("%d", msbY);
printf("\n");
printf("%d", lsbY);
printf("\n\n");
return 0;
//cout << msbY << "\n" << lsbY << "\n" << "\r";
}

int LSM303AGR::readCh3(){
msbZ = wiringPiI2CReadReg8(fd, OUTZ_H_REG_M);//same again, different regs
lsbZ = wiringPiI2CReadReg8(fd, OUTZ_L_REG_M);
printf("%d", msbZ);
printf("\n");
printf("%d", lsbZ);
printf("\n\n");
return 0;
//cout << msbZ << "\n" << lsbZ << "\n" << "\r";
}

int main(){
LSM303AGR lsm;

lsm.configure();
	for (int i = 0; i<100; i++){

		lsm.readCh1();
		lsm.readCh2();
		lsm.readCh3();
		printf("--------------------------------------------\n");

		usleep(2000000); //2 seconds
	}

}
