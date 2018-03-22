CC=gcc
CFLAGS=-I$(IDIR)

VL53L0X_API_SRC =\
		vl53l0x_api_calibration.c\
		vl53l0x_api_core.c \
		vl53l0x_api_ranging.c \
		vl53l0x_api_strings.c \
		vl53l0x_api.c \
		vl53l0x_platform.c


proximityTest: 
	$(CC) $(VL53L0X_API_SRC) Proximity_Test.c -o $@

clean:
	rm %.o proximityTest
