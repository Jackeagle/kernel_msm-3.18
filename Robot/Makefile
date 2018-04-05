CC = g++ -pthread -lncurses
CC2=gcc

TARGET = Go_Robopot

INC = -lwiringPi -LGUI -LLocation -LMove -LProject_Config

Proj_Dir = /home/pi/Robopot/Robot

INCLUDES = \
	-I$(Proj_Dir) \
	-I$(Proj_Dir)/inc \
	-I$(Proj_Dir)/Proximity_Sensor/platform/inc \
	-I$(Proj_Dir)/Proximity_Sensor/Api/core/inc \

VPATH = \
	$(Proj_Dir) \
	$(Proj_Dir)/src/ \
	$(Proj_Dir)/Proximity_Sensor/platform/src \
	$(Proj_Dir)/Proximity_Sensor/Api/core/src \

SRCcpp =\
	main.cpp \
	GUI.cpp \
	Location.cpp \
	Move.cpp \

SRCc = \
	vl53l0x_api_calibration.c \
	vl53l0x_api_core.c \
	vl53l0x_api_ranging.c \
	vl53l0x_api_strings.c \
	vl53l0x_api.c \
	vl53l0x_platform.c\

OBJ = $(SRCcpp:%.cpp=%.o) $(SRCc:%.c=%.o)


all:$(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(INC) $(INCLUDES) $^ -o $@

main.o: main.cpp
	$(CC) -c $(INC) $(INCLUDES) $^ -o $@	
	
GUI.o: GUI.cpp
	$(CC) -c $(INC) $(INCLUDES) $^ -o $@

Location.o: Location.cpp
	$(CC) -c $(INC) $(INCLUDES) $^ -o $@

Move.o: Move.cpp
	$(CC) -c $(INC) $(INCLUDES) $^ -o $@

vl53l0x_api_calibration.o: vl53l0x_api_calibration.c
	$(CC) -c $(INC) $(INCLUDES) $^ -o $@

vl53l0x_api_core.o: vl53l0x_api_core.c
	$(CC2) -c $(INC) $(INCLUDES) $^ -o $@

vl53l0x_api_ranging.o: vl53l0x_api_ranging.c
	$(CC) -c $(INC) $(INCLUDES) $^ -o $@

vl53l0x_api_strings.o: vl53l0x_api_strings.c
	$(CC) -c $(INC) $(INCLUDES) $^ -o $@

vl53l0x_api.o: vl53l0x_api.c
	$(CC) -c $(INC) $(INCLUDES) $^ -o $@

vl53l0x_platform.o: vl53l0x_platform.c
	$(CC2) -c $(INC) $(INCLUDES) $^ -o $@

clean:
	rm $(TARGET) $(OBJ)
