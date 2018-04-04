#include <iostream>
#include <fstream>

using namespace std;
int main(int argc, char* argv[])
{
	ofstream datatest;
	datatest.open ("datatest.csv");
	datatest << "X,Y,Z\n";
	datatest << "100,200,300";
	datatest.close();
	return 0;
}
