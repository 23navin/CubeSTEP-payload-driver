#include <iostream>
#include <string>
#include <stdio.h>
#include <wiringPiI2C.h>
using namespace std;

#define DEVICE_ID 0x23

#define operation 0x32

int main (int argc, char **argv)
{
	//connect i2c
	int fd = wiringPiI2CSetup(DEVICE_ID);
	if(fd == -1) {
		printf("I2C Failed\n");
		return -1;
	}
	printf("I2C Connected.\n");

	//process operation request
	int opcode;
	if(argc > 1) {
		//set opcode from command line argument
		opcode = stoi(argv[1], nullptr, 16);

		//send operation request
		uint8_t data_to_send = opcode;
		wiringPiI2CWrite(fd, data_to_send);
		
		//receive request receipt
		int received_data = wiringPiI2CRead(fd);

		//check request with receipt
		if(received_data == data_to_send) {
			printf("Request Received\n");
		}
		else {
			printf("Request Receipt Mismatch [%02X -> %02X]\n", data_to_send, received_data);
		}
	}
	
	//process parameters if present
	uint32_t param;
	if(argc > 2){
		param = stoi(argv[2], nullptr, 16); //parameter from command line argument
		uint8_t pbyte[4]; //bytes to be sent
		int size_of_parameter = 0;
		int param_size_max = 4;

		//split parameter into bytes
		pbyte[3] = (param & 0xFF000000) >> 24;
		pbyte[2] = (param & 0x00FF0000) >> 16;
		pbyte[1] = (param & 0x0000FF00) >> 8;
		pbyte[0] = param & 0x000000FF;

		printf("%X -> %02X %02X %02X %02X\n", param, pbyte[3], pbyte[2], pbyte[1], pbyte[0]);

		//determine how many bytes need to be sent
		while(size_of_parameter == 0) {
			if(pbyte[param_size_max - 1] != 0x00) {
				size_of_parameter = param_size_max;
			}
			else {
				param_size_max--;
			}
		}

		//send parameter byte(s)
		for(int i = size_of_parameter; i > 0; i--){
			wiringPiI2CWrite(fd, pbyte[i-1]);
			printf("Parameter %02X sent\n", pbyte[i-1]);
		}
	}

	//process data response if applicable
	int received_data = 0;
	if(opcode == 0x32){
		if(param == 0xEE) {
			//wait for data tx start byte
			while(wiringPiI2CRead(fd) != 0xFF) {
				printf("waiting...\n");
			}

			//receive data
			int rx_size = 4;
			while(rx_size > 0) {
				int buffer = wiringPiI2CRead(fd);
				rx_size--;
				
				//combine bytes
				received_data += buffer << (8*rx_size);
			}
		}
	}
	printf("Data Response: %X\n", received_data);

	return 0;
}