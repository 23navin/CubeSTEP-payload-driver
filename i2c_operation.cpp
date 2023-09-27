#include <iostream>
#include <string>
#include <stdio.h>
#include <vector>
#include <wiringPiI2C.h>
using namespace std;

#define DEVICE_ID 0x23

#define START_BYTE 0xAA
#define END_BYTE 0x04
#define VALID 0x88
#define INVALID 0xFF
#define UNKNOWN 0x44


#define STARTUP_TX 0x50
#define COOLDOWN_TX 0x51

uint8_t receive_one_byte(int handle){
	int rx_data = 0;
	
	//wait for start byte
	printf("<sb>waiting");
	while(wiringPiI2CRead(handle) != START_BYTE) {
		printf(".");
	}
	printf("\n");

	//bug fix to ignore erroneous extra start bytes
	do{
		//receive data
		rx_data = wiringPiI2CRead(handle);
	}while(rx_data == START_BYTE);

	return rx_data;
}

uint32_t receive_four_bytes(int handle){
	int rx_data = 0; //holds data received from device

	//wait for start byte
	printf("<sb>waiting");
	while(wiringPiI2CRead(handle) != START_BYTE) {
		printf(".");
	}
	printf("\n");

	//receive data
	int rx_size = 4;
	while(rx_size > 0) {
		int buffer;
		//bug fix to ignore erroneous extra start bytes
		if(rx_size == 4) {
			do{
				//receive data
				buffer = wiringPiI2CRead(handle);
			}while(buffer == START_BYTE);
		}
		else {
			//receive data
			buffer = wiringPiI2CRead(handle);
		}

		rx_size--;
		
		//combine bytes
		rx_data += buffer << (8*rx_size);
	}

	return rx_data;
}

std::string receive_string(int handle){
	string rx_data; //holds data received from device
	uint8_t buffer;

	//wait for start byte
	printf("<sb>waiting");
	while(wiringPiI2CRead(handle) != START_BYTE) {
		printf(".");
	}
	printf("\n");

	//receive data
	while(1) {
		buffer = wiringPiI2CRead(handle);
		if(buffer == 0x04) {
			break;
		}
		
		if(buffer == INVALID) {
			printf("End of File");
			break;
		}
		rx_data.push_back(buffer);
	}

	return rx_data;

}

int main (int argc, char **argv)
{
	//connect i2c
	int fd = wiringPiI2CSetup(DEVICE_ID);
	if(fd == -1) {
		printf("I2C Failed\n");
		return -1;
	}
	printf("I2C Connected.\n");

	// int szg = wiringPiI2CRead(fd);
	// printf("first byte: %02X\n", szg);

	//process operation request
	int opcode;
	if(argc > 1) {
		//set opcode from command line argument
		opcode = stoi(argv[1], nullptr, 16);

		//The device takes some time to wake up so\
		0x03 (Wake Device) must wait until confirmation\
		before it can be sent and processed.
		if(opcode == 0x03) {
			printf("<0x03>waiting");
			while(wiringPiI2CRead(fd) != DEVICE_ID) {
				printf(".");
			}
			printf("\n");
			printf("Device Awakened\n");
		}

		//send operation request
		uint8_t data_to_send = opcode;
		wiringPiI2CWrite(fd, data_to_send);
		
		//receive request receipt
		int received_data = wiringPiI2CRead(fd);
		while(received_data != data_to_send) {
				received_data = wiringPiI2CRead(fd);
		}

		printf("Confirmation receipt received\n");
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

		//determine how many bytes need to be sent
		size_of_parameter = opcode >> 5;

		//send parameter byte(s)
		for(int i = size_of_parameter; i > 0; i--){
			wiringPiI2CWrite(fd, pbyte[i-1]);
			printf("Parameter %02X sent\n", pbyte[i-1]);
		}
	}

	//process data response if applicable

	//00 - unused
	
	//1 - Restart Device
	if(opcode == 0x21){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Restart Successful\n");
		if(received_data == UNKNOWN) printf("<!> Parameter Undefined\n");
	}

	//02 - Sleep Device
	if(opcode == 0x02){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Device Slept\n");
	}

	//03 - Wake Device
		/* This operation is processed earlier because it
		has a unique handshake sequence*/

	//04 - Run System Check
		/* OpCode 0x08
		Deprecated */
	
	//05 - Get System Check Result
		/* OpCode 0x29
		Deprecated */

	//06 - Get Experiment Status
	if(opcode == 0x06){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Experiment Active\n");
		if(received_data == INVALID) printf("Experiment Inactive\n");
	}
	
	//07 - Get Time Left in Experiment
		/* OpCode 0x07
		Not Implemented */

	//08 - Start Experiment
	if(opcode == 0x08){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Experiment Started\n");
		if(received_data == INVALID) printf("<!> Experiment is already active\n");
	}

	//09 - Stop Experiment
	if(opcode == 0x29){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Experiment signaled to exit\n");
		if(received_data == INVALID) printf("<!> Experiment is already inactive\n");
		if(received_data == UNKNOWN) printf("<!> Parameter undefined\n");
	}

	//0A - Set Number of Stages
	if(opcode == 0x2A){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
	}

	//0B - Set Selected Stage
		/* OpCode 0x4B
		Deprecated */

	//0C - Set PWM for Stage
		/* OpCode 0x4C
		Deprecated */

	//0D - Set Stage Length
	if(opcode == 0x8D){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
	}

	//0E - Set Temperature Threshold
	if(opcode == 0x8E){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
	}

	//0F - Prepare Experiment Log
	if(opcode == 0x0F){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Log is ready to be read\n");
	}

	//10 - unused

	//11 - Get Experiment Log
	if(opcode == 0x11) {
		string received_data = receive_string(fd);
		//INVALID
		printf("\nData Response: %s\n", received_data.c_str());
	}

	//12 - Get Time
	if(opcode == 0x32){
		uint32_t received_data = receive_four_bytes(fd);

		if(received_data == UNKNOWN) printf("<!> Parameter undefined>\n");
		else printf("Time: %X\n", received_data);
	}

	//13 - Set Time
	if(opcode == 0x93){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
	}

	//14 - Get Temperature
	if(opcode == 0x34) {
		uint32_t received_data = receive_four_bytes(fd);

		if((received_data >> 24) == UNKNOWN) {
			printf("<!> Parameter is undefined\n");
		}
		else {
			union {
				float float_data;
				uint32_t uint_data;
			} uint_to_float = { .uint_data = received_data };

			printf("Temperature: %f\n",uint_to_float.float_data);
		}
	}

	//15 - Get PWM Duty
	if(opcode == 0x15){
		uint32_t received_data = receive_one_byte(fd);

		printf("PWM Duty: %i%\n", received_data);
	}

	//16 - Get Current Stage
	if(opcode == 0x16){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == INVALID) printf("<!> Experiment is inactive\n");
		else if(received_data == STARTUP_TX) printf("Experiment is in Startup Phase\n");
		else if(received_data == COOLDOWN_TX) printf("Experiment is in Cooldown Phase\n");
		else printf("Experiment is in Stage #%i\n", received_data);
	}

	//17 - Set Sampling Interval
	if(opcode == 0x97){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
	}

	//18 - Set Startup Length
	if(opcode == 0x98){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
	}

	//19 - Set Cooldown Length
	if(opcode == 0x99){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
	}

	//1A - Set PWM Array
	if(opcode == 0x5A){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
		if(received_data == 0xFD) printf("<!> Stage provided is invalid>\n");
		if(received_data == 0xFE) printf("<!> PWM provided is invalid>\n");
	}

	//1B - Set PWM Period
	if(opcode == 0x9B){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
		if(received_data == 0xFD) printf("<!> Period provided is invalid>\n");
	}

	//1C - Reset Log
	if(opcode == 0x1C){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Log reset\n");
		if(received_data == INVALID) printf("<!> Error when resetting log\n");
	}

	//1D
	if(opcode == 0x9D){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Experiment is active\n");
		if(received_data == 0xFD) printf("<!> Stage provided is invalid>\n");
	}
	//1E

	//1F

	return 0;
}