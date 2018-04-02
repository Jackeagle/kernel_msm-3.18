#include <iostream>
#include <errno.h>
#include <wiringPiI2C.h>
#include <linux/i2c.h>
#include "LSM303AGR.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>


#define PI 3.14159265
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
	alastair = wiringPiI2CWriteReg8(fd, CFG_REG_A_M, 0x00);//copnfigure with default settings
}

int LSM303AGR::readCh1(){
	msbX = wiringPiI2CReadReg8(fd, OUTX_H_REG_M);//68h and 69h for X output registers, 69 is MSB(?)
	lsbX = wiringPiI2CReadReg8(fd, OUTX_L_REG_M);
	xresult = (msbX<<8 | lsbX);//the value is a 16-bit signed integer. Therefore, shift the 8 bits read before (msbX) and input the latter 8 bits on the end.
//	printf("%d\n\n",xresult);
	return xresult;
//	return(wiringPiI2CReadReg8(fd, OUTX_H_REG_M) << 8) | wiringPiI2CReadReg8(fd, OUTX_L_REG_M);
}

int LSM303AGR::readCh2(){
	msbY = wiringPiI2CReadReg8(fd, OUTY_H_REG_M);//6Ah and 6Bh for y output registers,
	lsbY = wiringPiI2CReadReg8(fd, OUTY_L_REG_M);
	yresult = (msbY << 8) | (lsbY);
//	printf("%d\n\n", yresult);
	return yresult;
}
//not actually required to calculate the compas rotation
//int LSM303AGR::readCh3(){
//	msbZ = wiringPiI2CReadReg8(fd, OUTZ_H_REG_M);//same again, different regs
//	lsbZ = wiringPiI2CReadReg8(fd, OUTZ_L_REG_M);
//	printf("%d", msbZ);
//	printf("\n");
//	printf("%d", lsbZ);
//	printf("\n\n");
//cout << msbZ << "\n" << lsbZ << "\n" << "\r";
//}

int main(){
LSM303AGR lsm;

lsm.configure();
	for (int i = 0; i<1000; i++){

//		lsm.readCh1();
//		int y = lsm.readCh1();
//		lsm.readCh2();
//		int x = lsm.readCh2();
//		lsm.readCh3();
		printf("--------------------------------------------\n");
		int sum = 0;
		float average = 0;
		float compass[10];
		float direction; //compass coordingate

		for (int j = 0; j < 10; j++){
		//	int sum = 0;
		//	int average = 0;
		//	int size = 0;
			int x = lsm.readCh1();
			int y = lsm.readCh2();
			if (y > 0){
				direction = (90 - (atan(x/y)*(180/PI)));
		//	printf("Direction = %d\n", direction);
		//		direction = compass[j];
				compass[j] = direction;
			} else if (y < 0){
				direction = (270 - (atan(x/y)*(180/PI)));
		//	printf("Direction = %d\n", direction);
		//		return direction;
		//		direction = compass[j];
				compass[j] = direction;
			} else if (y == 0 && x < 0){
				direction = 180;
		//	printf("Direction = %d\n", direction);
		//		return direction;
		//		direction = compass[j];
				compass[j] = direction;
			} else if (y == 0 && x > 0){
				direction == 0;
		//	printf("Direction = %d\n", direction);
		//		return direction;
		//		direction = compass[j];
				compass[j] = direction;
			} else {
				printf("Direction = error\n");
		//		return 0;
			}
		sum += compass[j];
//		average = sum/10;
//		return average = sum/size;


//		cout << "Average = " << average << endl;
		}
	average = sum/10;
	cout << "average direction = " << average << endl;
	sleep(1); //1 second(s)
	}
/*calculations for compass headings
Direction (y>0) =  90 - [arctan(x/y)]*180/pi
Direction (y<0) = 270 - [arcTAN(x/y)]*180/pi
Direction (y=0, x<0) = 180
Direction (y=o, x>0) = 0
*/
}
