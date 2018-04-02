#include <iostream>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <cmath>
#include <fstream>
#include <string.h>
#include <unistd.h>

class opt3001
{
	int sensor[4];
	float lux[4];
	int y=0;

	public:
	int setup(){
		int wiringPiSetup (void);
		sensor[0] = wiringPiI2CSetup (0x44); //GND
		sensor[1] = wiringPiI2CSetup (0x45); //VDD
		sensor[2] = wiringPiI2CSetup (0x46); //SDA
		sensor[3] = wiringPiI2CSetup (0x47); //SCL
	}

	float setconfig(int i,uint16_t config){
		wiringPiI2CWriteReg16 (sensor[i],0x01,config);
	}

	void getlux(int i){
		uint16_t raw = 0;
		uint16_t byte1 = 0;
		uint16_t byte2 = 0;
		uint16_t exponent = 0;
		uint16_t fractional = 0;
		float lsbsize = 0;
		raw = wiringPiI2CReadReg16 (sensor[i],0x00);
		byte1 = (raw&0xFF00);
        	byte2 = (raw&0x00FF);
        	byte1 >>= 8;
        	byte2 <<= 8;
        	raw = (byte1|byte2);
		exponent = (raw&0xF000);
        	fractional = (raw&0x0FFF);
        	exponent >>= 12;
        	lsbsize = (0.01*(pow(2,exponent)));
        	lux[i] = (lsbsize*fractional);
	}

	void savedata(){
		std::fstream myfile;
		myfile.open ("/home/pi/Code/opt3001/lightdata.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);
		myfile <<lux[0]<<"\n"<<lux[1]<<"\n"<<lux[2]<<"\n"<<lux[3];
		myfile.close();
	}
};


int main (){

	char command[100];
	uint16_t configuration = 0;
	//float luxsensor[4];
	int i=0;

	opt3001 opt3001a;

	//transfer command, set IP addresses and directories
	strcpy( command, "scp /home/pi/Code/opt3001/lightdata.txt teamrobopot@192.168.43.112:/home/teamrobopot/Documents" );

	//setup wiringPiI2C with all sensors
	opt3001a.setup();

	//set configuration
	configuration = 0x00CC;

	for(i=0;i<2;i++){
	opt3001a.setconfig(i,configuration);
	}

	//get lux readings
	while(1){
		sleep(5);
		for(i=0;i<2;i++){
			opt3001a.getlux(i);
		}
		//save to lightdata.txt
		opt3001a.savedata();
		//send lightdata.txt to the other Pi using scp
		system(command);
	}

	return 0;
}


