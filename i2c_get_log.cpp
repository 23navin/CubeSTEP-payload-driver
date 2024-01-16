#include <iostream>
#include <string>
#include <fstream>
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

#define LOG_FILE "payload_log.csv"

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

bool receive_string(int handle, std::string *rx_buffer){
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
			//end of line
			break;
		}
		
		if(buffer == INVALID) {
			//device did not send a line
			return false;
		}
		rx_data.push_back(buffer);
	}

	*rx_buffer = rx_data;
	return true;

}

int main (int argc, char **argv)
{
    uint32_t response;
    uint8_t data_to_send;
	std::fstream fs;

	//connect i2c
	int fd = wiringPiI2CSetup(DEVICE_ID);
	if(fd == -1) {
		printf("I2C Failed\n");
		return -1;
	}
	printf("I2C Connected.\n");

    bool lines_left = true;
    while(lines_left){
        //retrieve
        data_to_send = 0x11;
        wiringPiI2CWrite(fd, data_to_send);

        //retrieve receipt
        int receipt = wiringPiI2CRead(fd);
        while(receipt != data_to_send) {
            receipt = wiringPiI2CRead(fd);
        }

        string received_data;
		if(receive_string(fd, &received_data)){
			//get rid of any 88s
			while((received_data.compare(0, 1, "8") == 0) || (received_data.compare(0, 1, "�") == 0)){
				received_data.erase(0, 1);
			}

			//write line to log
  			fs.open(LOG_FILE, std::fstream::app);
			fs << received_data;
			fs << "\n";
			fs.close();
		}
		else{
			//device has stopped sending lines
			lines_left = false;
		}
    }

    return 0;
}