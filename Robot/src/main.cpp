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
	
	//Declare class objects
	GUI interface;	
	Move motors;
	Location loc;
	
	char Pot_Location = 'a'; //Initialise Pot Location to 'a'
	char Destination; // Declare destination variable
	int X_Bearing;	//// Declare x bearing variable for location class
	int Y_Bearing;	// Declare y bearing variable for location class
	int X_PathLength;  // Declare path length in x direction for location class
	int Y_PathLength;  // Declare path length in y direction for location class

	motors.Initialise(); //Initialise Motors

	while (1)
	{
		if ( interface.Check_Cmd(cmdfile) == true)	//If destination is sent from GUI
		{
			Destination = interface.Read_Cmd(cmdfile);	//Read destination
			Pot_Location = loc.Find_Pot();			//Measure position
			if ( Destination != Pot_Location)	//If Destination is different from present location
			{
				loc.Find_Path(Destination, Pot_Location); //Determine path to destination
				if (X_PathLength != 0) //if required to move in along x path
				{
					motors.Turn(X_Bearing); //Turn to face direction of travel
					for (int i = 0; i<X_PathLength; i++)	//Move for required number of unit lengths in x direction
					{
						motors.Drive();
						Pot_Location = loc.Find_Pot(); 	
						interface.Send(ToGUI, &Pot_Location); //transmit location to GUI during transit
					}
				}
				if (Y_PathLength != 0)  //if required to move in along y path
				{
					motors.Turn(Y_Bearing); //Turn to face direction of travel
					for (int i = 0; i<Y_PathLength; i++) //Move for required number of unit lengths in y direction
					{
						motors.Drive();
						Pot_Location = loc.Find_Pot();
						interface.Send(ToGUI, &Pot_Location); //transmit location to GUI during transit
					}
				}
			}
		}
`	}//Close while loop

} // End main
