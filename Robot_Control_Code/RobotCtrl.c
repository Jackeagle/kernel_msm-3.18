/*
Author: Fraser Menzies
Date: 22 Feb 2018

Robot Hardware requires

-2 motors controlled with PWM, 4GPIO PWM pins + 2GPIO enable pins
-1 I2C comms bus for Magnemometer
-1 I2C comms bus for Proximity Sensor

Robot Operation

-Check command file is readable
-if command file is readable
	-Get destination
	-Clear command file
	-Determine path to reach destination
	Path Type 1
		-Orient robot (slow first 10% ramp up 10% fast 60% ramp down 10% slow 10%)
		-Move 1 square
		-Write to location file
		-Repeat x times
	Path Type 2
		-Orient robot
			-Move 1 square
			-Write to location file
			-Repeat x times
		-Orient robot
			-Move 1 square
			-Write to location file
			-Repeat y times
	Confirm destination is reached
	if destination is not reached then send warning


*/
#include <stdio.h>
#include <wiringPi.h>

//Function Declarations
/*
Checks command file
@Return 1-command file contains a command | 0-command file is empty
*/
int Check_cmd();
/*
Reads command file
@Param char *cmd- required robot location
*/
void Read_cmd(char *cmd);
/*
Determines path to required location
@Param char *cmd - required location
@Param char *Location -Location of robot
@Param int *Turn_1- 1 or 0 on whether a first turn is required
@Param int *Turn_2- 1 or 0 on whether a second turn is required
@Param int *Move_x- number of movements in the x direction required
@Param int *Move_y- number of movements in the y direction required
*/
void Find_Path(char cmd, char Location, int *Turn_1, int *Turn_2, int *Move_x,int *Move_y);
/*
Reads magnemometer
@Param int *Direction - Direction robot is pointing
*/
void Find_Direction(int *Direction);
/*
Reads proximity sensor
@Param int *Proximity - Distance of robot to object in front
*/
void Find_Proximity(int *Proximity);
/*
Finds Location
@Param char *Location -Location of robot
@Param int Direction - Direction robot is pointing
@Param int Proximity - Distance of robot to object in front
*/
void Find_Location(char* Location, int Direction, int Proximity);
/*
Turns robot
@Param char *Direction- required direction to be pointed in
*/
void Turn(char Direction);
/*
Moves robot by a single unit length
*/
void Drive();
/*
Sends location to location file
@Param char *Location
*/
void Transmit_Location(char *Location);

int main()
{
    char cmd;    //location robot is required to be
    int Direction;    //Direction robot is facing
    int Proximity;    //Proximity Sensor Reading
    char Location='a';    //robot location
    int Turn_1=0;
    int Turn_2=0;
    int Move_x=0;
    int Move_y=0;
    int Move_Flag=0;
    int i;

    while(1)
    {
        Find_Direction(&Direction);
        Find_Proximity(&Proximity);
        Find_Location(&Location, &Direction, &Proximity);
        Move_Flag = Check_cmd();

        if (Move_Flag == 1 && Direction != NULL && Proximity != NULL)
	{
            Read_cmd(&cmd);
            if(cmd!=Location)
            {
                Find_Path(cmd, Location, &Turn_1, &Turn_2, &Move_x, &Move_y);
                if(Turn_1 != 0)
                {
                    Turn(&Turn_1);
                }
                if(&Move_x != 0)
                {
                    for(i=0; i<Move_x; i++)
                    {
                        Drive();
                        Find_Location(&Location, Direction, Proximity);
                        Transmit_Location(Location);
                    }
                }
                if(Turn_2 != 0)
                {
                    Turn(&Turn_2);
                }
                if(&Move_y != 0)
                {
                    for(i=0; i<Move_y; i++)
                    {
                        Drive();
                        Find_Location(&Location, Direction, Proximity);
                        Transmit_Location(Location);
                    }
                }
                if(cmd == Location)
                {
                Move_Flag = 0;
                }

            }else{
                Move_Flag=0;
            }
	}
    }

}
