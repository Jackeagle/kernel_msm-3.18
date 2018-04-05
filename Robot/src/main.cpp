/*
Author: Fraser Menzies
Date: 22 Mar 2018
Robot Control Code
Classes used:
GUI class
motors class
location class
*/
#include <stdio.h>
#include <string>
#include <wiringPi.h>	//RPI headers and functions commented out in laptop version

//include Classes
#include "GUI.h"
#include "Location.h"
#include "Move.h"
#include "Project_Config.h"

int main()
{
	char Pot_Location = 'a'; //Initialise Pot Location to 'a'
	char Destination;
	int Face_EastOrWest_Flag = 0;
	int Face_NorthOrSouth_Flag = 0;
	int Move_EastOrWest_Flag = 0;
	int Move_NorthOrSouth_Flag = 0;

	int X_Bearing;
	int Y_Bearing;
	int X_PathLength;
	int Y_PathLength;

	//Declare class objects
	GUI interface;	
	Location loc;
	Move motors;

	motors.Initialise(); //Initialise Motors

	while (1)
	{
		bool Move_Flag = interface.Check_Cmd(cmdfile); //Check for destination request from GUI

		if ( Move_Flag == true)	//If destination is sent from GUI
		{
			Destination = interface.Read_Cmd(cmdfile);	//Read destination

			if ( Destination != Pot_Location)	//If Destination is different from initial location
			{
				Pot_Location = loc.Find_Pot();
				loc.Find_Path(Destination, Pot_Location);

				if (&X_PathLength != 0)
				{
					motors.Turn(X_Bearing);
					for (int i = 0; i<X_PathLength; i++)
					{
						motors.Drive();
						Pot_Location = loc.Find_Pot();
						interface.Send(ToGUI, &Pot_Location);
					}
				}

				if (Y_PathLength != 0)
				{
					motors.Turn(Y_Bearing);
					for (int i = 0; i<Y_PathLength; i++)
					{
						motors.Drive();
						Pot_Location = loc.Find_Pot();
						interface.Send(ToGUI, &Pot_Location);
					}
				}
				if (Destination == Pot_Location)
				{
					Move_Flag = false;
				}

			}
			else {
				Move_Flag = false;
			}
		}
	}

}
