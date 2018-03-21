#include <stdlib.h>
#include <stdio.h>
#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"





int main()
{
	int i;
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	VL53L0X_Dev_t MyDevice;
    VL53L0X_Dev_t *pMyDevice = &MyDevice;
	VL53L0X_RangingMeasurementData_t RangingMeasurementData;
	
	printf ("VL53L0X API Simple Ranging example FMenzies mod\n\n");

    pMyDevice->I2cDevAddr      = 0x29;
	
    pMyDevice->fd = VL53L0X_i2c_init("/dev/i2c-1", pMyDevice->I2cDevAddr);//choose between i2c-0 and i2c-1; On the raspberry pi zero, i2c-1 are pins 2 and 3

	
if(Status == VL53L0X_ERROR_NONE)
    {
        for(i=0;i<50;i++)
		{
            printf ("Call of VL53L0X_PerformSingleRangingMeasurement\n");
            Status = VL53L0X_PerformSingleRangingMeasurement(pMyDevice,
            		&RangingMeasurementData);

            //print_pal_error(Status);
            //print_range_status(&RangingMeasurementData);

           
            if (Status != VL53L0X_ERROR_NONE) break;

            printf("Measured distance: %i\n\n", RangingMeasurementData.RangeMilliMeter);


        }
    }
	
}
