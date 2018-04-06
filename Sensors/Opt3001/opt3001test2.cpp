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
		std::fstream file1;
		file1.open ("/home/pi/Code/opt3001/lightdata/lightdata1.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);
		file1 <<lux[0];
		file1.close();
                std::fstream file2;
                file2.open ("/home/pi/Code/opt3001/lightdata/lightdata2.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);
                file2 <<lux[1];
                file2.close();
                std::fstream file3;
                file3.open ("/home/pi/Code/opt3001/lightdata/lightdata3.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);
                file3 <<lux[2];
                file3.close();
                std::fstream file4;
                file4.open ("/home/pi/Code/opt3001/lightdata/lightdata4.txt", std::fstream::in | std::fstream::out | std::fstream::trunc);
                file4 <<lux[3];
                file4.close();

		/*std::cout<<"\n"<<"Sensor 1 = "<<lux[0]<<" lx"<<std::endl;
		std::cout<<"Sensor 2 = "<<lux[1]<<" lx"<<std::endl;
                std::cout<<"Sensor 3 = "<<lux[2]<<" lx"<<std::endl;
                std::cout<<"Sensor 4 = "<<lux[3]<<" lx"<<std::endl;*/
	}
};


int main (){

	char command[100];
	uint16_t configuration = 0;
	//float luxsensor[4];
	int i=0;

	opt3001 opt3001a;

	//transfer command, set IP addresses and directories
	strcpy( command, "scp -r /home/pi/Code/opt3001/lightdata teamrobopot@192.168.43.112:/home/teamrobopot/Documents" );

	//setup wiringPiI2C with all sensors
	opt3001a.setup();

	//set configuration
	configuration = 0x00CC;

	for(i=0;i<4;i++){
	opt3001a.setconfig(i,configuration);
	}

	//get lux readings
	while(1){
		sleep(1);
		for(i=0;i<4;i++){
			opt3001a.getlux(i);
		}
		//save to lightdata.txt
		opt3001a.savedata();
		//send lightdata.txt to the other Pi using scp
		system(command);
	}

	return 0;
}


