#include <iostream>
#include <errno.h>
#include <wiringPiI2C.h>
#include <linux/i2c.h>
#include "LSM303AGR.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <fstream>

#define PI 3.14159265 //used for calculations
#define ACC_ADDR 0x19 //not used
#define MAG_ADDR 0x1E
//default addresses
#define STATUS_REG_M 0x67
#define OUTX_L_REG_M 0x68 //lsb
#define OUTX_H_REG_M 0x69 //msb to shift << 8
#define OUTY_L_REG_M 0x6A //lsb
#define OUTY_H_REG_M 0x6B //msb to shift
#define OUTZ_L_REG_M 0x6C //lsb
#define OUTZ_H_REG_M 0x6D //msb to shift
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
	alastair = wiringPiI2CWriteReg8(fd, CFG_REG_A_M, 0x00);//configure with default settings
}

int LSM303AGR::readCh1(){
	msbX = wiringPiI2CReadReg8(fd, OUTX_H_REG_M);//68h and 69h for X output registers, 69 is MSB(?)
	lsbX = wiringPiI2CReadReg8(fd, OUTX_L_REG_M);
	xresult = (msbX<<8 | lsbX);//the value is a 16-bit signed integer. Therefore, shift the 8 bits read before (msbX) and input the latter 8 bits on the end.
//	printf("%d\n\n",xresult);
	return xresult;
}

int LSM303AGR::readCh2(){
	msbY = wiringPiI2CReadReg8(fd, OUTY_H_REG_M);//6Ah and 6Bh for y output registers,
	lsbY = wiringPiI2CReadReg8(fd, OUTY_L_REG_M);
	yresult = (msbY << 8) | (lsbY);
//	printf("%d\n\n", yresult);
	return yresult;
}
//not actually required to calculate the compas rotation
int LSM303AGR::readCh3(){
	msbZ = wiringPiI2CReadReg8(fd, OUTZ_H_REG_M);//same again, different regs
	lsbZ = wiringPiI2CReadReg8(fd, OUTZ_L_REG_M);
	zresult = (msbZ << 8) |  (lsbZ);
//	printf("%d\n\n",zresult);
	return zresult;
}


int main(){

LSM303AGR lsm;
ofstream magneto;
/*data logging
ofstream datatest;
datatest.open("datatest.csv");
datatest << "X,Y,Z\n";
datatest.close();
*/
lsm.configure();
	for (int i = 0; i<100000; i++){//number of output checks
// more data logging		datatest.open("datatest.csv", fstream::app);
		int sum = 0;
		float average = 0;
		float compass[10];
		float direction; //compass coordingate

		for (int j = 0; j < 10; j++){//number of averages, 10 seems to be max
			float x = lsm.readCh1()-307;//to centre the plot around 0. It performs as expected with few errors
		//	datatest << x;
			float y = lsm.readCh2();
		//	datatest << "," << y;
			float z = lsm.readCh3();
		//	datatest << "," << z << "\n";
		//	datatest.close();
			float coord = x/y;

			if (y > 0){
				direction = 90 - atan(coord)*(180/PI);
				compass[j] = direction;
			} else if (y < 0){
				direction = 270 - atan(coord)*(180/PI);
				compass[j] = direction;
			} else if (y == 0 && x < 0){
				direction = 180;
				compass[j] = direction;
			} else if (y == 0 && x > 0){
				direction = 0;
				compass[j] = direction;
			} else {
				printf("Direction = error\n");
			}
		sum += compass[j];
		}
	average = sum/10;
	cout << "average direction = " << average << endl;
	magneto.open("magneto.txt");
	magneto << average;
	magneto.close();
//	sleep(1); //1 second(s)
	}

/*calculations for compass headings
Direction (y>0) =  90 - [arctan(x/y)]*180/pi
Direction (y<0) = 270 - [arcTAN(x/y)]*180/pi
Direction (y=0, x<0) = 180
Direction (y=o, x>0) = 0
*/
}
