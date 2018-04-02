#include "Location.h"



Location::Location()
{
}


Location::~Location()
{
}

int Location::Setup()
{

}

char Location::Find_Pot()
{

}

int Location::Find_Direction()
{

}

int Location::Find_Proximity()
{
	int i;
	int divisor=5;
	int proximity_measurements=0;
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	VL53L0X_Dev_t MyDevice;
	VL53L0X_Dev_t *pMyDevice = &MyDevice;
	VL53L0X_RangingMeasurementData_t RangingMeasurementData;

	printf("VL53L0X API Simple Ranging example FMenzies mod\n\n");

	pMyDevice->I2cDevAddr = 0x29;

	pMyDevice->fd = VL53L0X_i2c_init("/dev/i2c-1", pMyDevice->I2cDevAddr);//choose between i2c-0 and i2c-1; On the raspberry pi zero, i2c-1 are pins 2 and 3


	if (Status == VL53L0X_ERROR_NONE)
	{
		for (i = 0;i<divisor;i++)
		{
			printf("Call of VL53L0X_PerformSingleRangingMeasurement\n");
			Status = VL53L0X_PerformSingleRangingMeasurement(pMyDevice,&RangingMeasurementData);
			//printf("Measured distance: %i\n\n", RangingMeasurementData.RangeMilliMeter);
			proximity_measurements += RangingMeasurementData.RangeMilliMeter;
		}
		return proximity_measurements /divisor;
	}
}

void Location::Find_Path(char Destination, char *Pot, int *Turn_1, int *Turn_2, int *Move_x, int *Move_y)
{
	int Dest_x = 0, Dest_y = 0; //destination coordinates
	int Loc_x = 0, Loc_y = 0;
	Find_coordinates(Destination, &Dest_x, &Dest_y);
	Find_coordinates(*Pot, &Loc_x, &Loc_y);

	*Move_x = Dest_x - Loc_x;
	*Move_y = Dest_y - Loc_y;

	if (*Move_x > 0)
	{
		*Turn_1 = EAST;
	}
	else if (*Move_x < 0) 
	{
		*Turn_1 = WEST;
	}
	else
	{
		*Turn_1 = NULL;
	}

	if (*Move_y > 0)
	{
		*Turn_2 = SOUTH;
	}
	else if (*Move_y < 0)
	{
		*Turn_2 = NORTH;
	}
	else
	{
		*Turn_2 = NULL;
	}
		
}

void Location::Find_coordinates(char ref, int *x, int *y)
{
	int i, j;
	const char Location_ref[4][4] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l','m', 'n', 'o', 'p' };

	for (i = 0;i<4;i++)
	{
		for (j = 0;j<4;j++)
		{
			if (ref == Location_ref[i][j])
			{
				*x = i;
				*y = j;
			}
		}
	}
}