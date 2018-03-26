/*
Author: Fraser Menzies
Date: 22 Feb 2018
Robot Control Code
Classes used:
GUI class
motors class
location class
*/
#include <stdio.h>
#include <string>
//#include <wiringPi.h>	//RPI headers and functions commented out in laptop version

//include Classes
#include "GUI.h"
#include "Location.h"
#include "Move.h"
#include "Project_Config.h"

//Function Declarations


//Calculates coordinates from location reference
//@Param int ref - Location reference
//@Param int *x - x coordinate
//@Param int *y - y coordinate
void Find_Direction(int *Direction);

//Reads proximity sensor
//@Param int *Proximity - Distance of robot to object in front
void Find_Pot(char* Pot, int Direction, int Proximity);

//Turns robot
//@Param char *Direction- required direction to be pointed in
void Turn(char Direction);



int main()
{
	char Pot = 'a';    //robot location
	int Turn_1 = 0;
	int Turn_2 = 0;
	int Move_x = 0;
	int Move_y = 0;
	int i;

	GUI interface;
	Location loc;
	Move motors;

	motors.Setup();
	interface.Setup();
	loc.Setup();

	//Check Drive Function
	char cmd = interface.Read_Cmd(cmdfile);

	while (1) {
		if (interface.Read_Cmd(cmdfile)!= cmd) {
		
			motors.Drive();

			cmd = interface.Read_Cmd(cmdfile);
		}
	}

	/*
	while (1)
	{
		bool Move_Flag = interface.Check_Cmd(cmdfile);

		if ( Move_Flag == true)
		{
			char cmd = interface.Read_Cmd(cmdfile);

			if ( cmd != Pot)
			{
				loc.Find_Pot();
				loc.Find_Path(cmd, &Pot, &Turn_1, &Turn_2, &Move_x, &Move_y);

				if (&Move_x != 0)
				{
					Turn(&Turn_1);
					for (i = 0; i<Move_x; i++)
					{
						Drive();
						Find_Location(&Pot, Direction, Proximity);
						interface.Send(ToGUI, &Pot);
					}
				}

				if (&Move_y != 0)
				{
					Turn(&Turn_2);
					for (i = 0; i<Move_y; i++)
					{
						Drive();
						Find_Location(&Pot, Direction, Proximity);
						interface.Send(ToGUI, &Pot);
					}
				}
				if (cmd == Location)
				{
					Move_Flag = 0;
				}

			}
			else {
				Move_Flag = 0;
			}
		}
	}
	*/

}