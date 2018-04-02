#ifndef LSM303AGR_H
#define LSM303AGR_H

//init device and read values

class LSM303AGR
{
	int msbX;
	int lsbX;
	int msbY;
	int lsbY;
	int msbZ;
	int lsbZ;
	int fd;
	int alastair;
	int16_t xresult;
	int16_t yresult;
	int16_t zresult;
public:
	LSM303AGR();
	int configure();
	int getFileDescriptor();
	int dataAvailable();
	int readCh1();
	int readCh2();
	int readCh3();
	int startSeq();
	int sadW();
	int sak();
	int sub();
	int sadR();
};

#endif
