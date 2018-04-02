#ifndef GUI_H
#define GUI_H

#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include "Project_Config.h"

class GUI
{
public:
	GUI();
	~GUI();
	//Setup and Initialisation of GUI
	int Setup();
	//Confirms if GUI has sent command
	bool Check_Cmd(const std::string& filename);
	//returns input from GUI
	char Read_Cmd(const std::string& filename);
	//Sends data to GUI
	void Send(const std::string& filename, const std::string& message);

private:
	//Member Variables
	char cmd;
	
};

#endif // !1