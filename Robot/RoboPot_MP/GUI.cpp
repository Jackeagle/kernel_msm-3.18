#include "GUI.h"

GUI::GUI()
{
}


GUI::~GUI()
{
}

int GUI::Setup()
{
	Send(SetupFile, SetupMessage);
}

bool GUI::Check_Cmd(const std::string& filename)
{
	std::ifstream file(filename.c_str());
	return file.good();
}

char GUI::Read_Cmd(const std::string& filename)
{
	std::ifstream file(filename.c_str());
	file.get(cmd);
	remove(filename.c_str());
	return cmd;
}

void GUI::Send(const std::string& filename, const std::string& message)
{
	std::ifstream file;
	file.open(filename.c_str());
	std::cout << message;
}