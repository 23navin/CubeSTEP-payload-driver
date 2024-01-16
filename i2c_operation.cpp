#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <vector>
using namespace std;

#include <fcntl.h> //open
#include <sys/ioctl.h> //ioctl

#define DEVICE_ID 0x23

#define START_BYTE 0xAA
#define END_BYTE 0x04
#define VALID 0x88
#define INVALID 0xFF
#define UNKNOWN 0x44

#define STARTUP_TX 0x50
#define COOLDOWN_TX 0x51

#define LOG_FILE "payload_log.csv"

union i2c_smbus_data
{
  uint8_t  byte;
  uint16_t word;
};

struct i2c_smbus_ioctl_data
{
  char read_write ;
  uint8_t command ;
  int size ;
  union i2c_smbus_data *data ;
};

int i2c_read(int file){
	i2c_smbus_data data;

	i2c_smbus_ioctl_data args = {
		.read_write = 1, //I2C_SMBUS_READ
		.command = 0,
		.size = 1, //I2C_SMBUS_BYTE
		.data = &data
	};

	if(ioctl (file, 0x0720, &args))
		return -1;

	return data.byte & 0xFF;
}

int i2c_write(int file, uint8_t data){
	i2c_smbus_ioctl_data args = {
		.read_write = 0, //I2C_SMBUS_WRITE
		.command = data,
		.size = 1, //I2C_SMBUS_BYTE
		.data = NULL
	};

	return ioctl (file, 0x0720, &args);
}

int connect(const char *i2c_device){
	int file;
	if((file = open(i2c_device, O_RDWR)) < 0)
		return -1; //Can't open i2c device
	
	if(ioctl(file, 0x0703, DEVICE_ID) < 0)
		return -1; //Can't select i2c device

	return file;
}

int handshake(int file, uint8_t opcode, uint32_t parameter){
	uint8_t pbyte[4]; //bytes to be sent
	int size_of_parameter = 0;
	int param_size_max = 4;

	//The device takes some time to wake up so\
	0x03 (Wake Device) must wait until confirmation\
	before it can be sent and processed.
	if(opcode == 0x03) {
		printf("<0x03>waiting");
		while(i2c_read(file) != DEVICE_ID) {
			printf(".");
		}
		printf("\n");
		printf("Device Awakened\n");
	}

	//send operation request
	uint8_t data_to_send = opcode;
	i2c_write(file, data_to_send);
	
	//receive request receipt
	int received_data = i2c_read(file);
	while(received_data != data_to_send) {
		received_data = i2c_read(file);
	}

	printf("Confirmation receipt received\n");

	//split parameter into bytes
	pbyte[3] = (parameter & 0xFF000000) >> 24;
	pbyte[2] = (parameter & 0x00FF0000) >> 16;
	pbyte[1] = (parameter & 0x0000FF00) >> 8;
	pbyte[0] = parameter & 0x000000FF;

	//determine how many bytes need to be sent
	size_of_parameter = opcode >> 5;

	//send parameter byte(s)
	for(int i = size_of_parameter; i > 0; i--){
		i2c_write(file, pbyte[i-1]);
		printf("Parameter %02X sent\n", pbyte[i-1]);
	}

	return 1;
}

uint8_t receive_one_byte(int handle){
	int rx_data = 0;
	
	//wait for start byte
	printf("<sb>waiting");
	while(i2c_read(handle) != START_BYTE) {
		printf(".");
	}
	printf("\n");

	//bug fix to ignore erroneous extra start bytes
	do{
		//receive data
		rx_data = i2c_read(handle);
	}while(rx_data == START_BYTE);

	return rx_data;
}

uint32_t receive_four_bytes(int handle){
	int rx_data = 0; //holds data received from device

	//wait for start byte
	printf("<sb>waiting");
	while(i2c_read(handle) != START_BYTE) {
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
				buffer = i2c_read(handle);
			}while(buffer == START_BYTE);
		}
		else {
			//receive data
			buffer = i2c_read(handle);
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
	while(i2c_read(handle) != START_BYTE) {
		printf(".");
	}
	printf("\n");

	//receive data
	while(1) {
		buffer = i2c_read(handle);
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
	//connect i2c
	int fd = connect("/dev/i2c-1");
	if(fd == -1) {
		printf("I2C Failed\n");
		return -1;
	}
	printf("I2C Connected.\n");

	//process operation request
	uint8_t opcode;
	if(argc > 1) {
		//set opcode from command line argument
		opcode = stoi(argv[1], nullptr, 16); 
	}
	
	//process parameters if present
	uint32_t parameter = 0;
	if(argc > 2){
		parameter = stoi(argv[2], nullptr, 16); //parameter from command line argument
	}

	handshake(fd, opcode, parameter);

	//for retrieving telemetry log to local csv file
	std::ofstream log;

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

		//create local file
		log.open("payload_log.csv");
	}

	//10 - unused

	//11 - Get Experiment Log
	if(opcode == 0x11) {
		string received_data;
		//INVALID

		const string eof = "End of File";
		if(receive_string(fd, &received_data)){
			printf("\nData Response: %s\n", received_data.c_str());
			
			std::fstream fs;
  			fs.open(LOG_FILE, std::fstream::app);
			fs << received_data;
			fs << "\n";
			fs.close();
		}
		else{
			printf("END\n");
		}
	}

	//12 - Get Time
	if(opcode == 0x32){
		uint32_t received_data = receive_four_bytes(fd);

		if((received_data >> 24) == UNKNOWN) {
			printf("<!> Parameter is undefined\n");
		}
		else printf("Time: %u\n", received_data);
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

		if(received_data == 0) printf("<!> Experiment is inactive\n");
		else if(received_data == 2) printf("Experiment is in Startup Phase\n");
		else if(received_data == 3) printf("Experiment is in Cooldown Phase\n");
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
	if(opcode == 0x9E){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID) printf("Value was set\n");
		if(received_data == INVALID) printf("<!> Task is active\n");
	}

	//1F
	if(opcode == 0x3F){
		uint32_t received_data = receive_one_byte(fd);

		if(received_data == VALID){
			if(parameter == 0x01) printf("Passive Logger Started\n");
			if(parameter == 0x02) printf("Passive Logger Stopped\n");
			if(parameter == 0x03) printf("Passive Logger is Active\n");
		}

		if(received_data == INVALID){
			if(parameter == 0x01) printf("<!> Passive Logger already started\n");
			if(parameter == 0x02) printf("<!> Passive Logger already stopped\n");
			if(parameter == 0x03) printf("Passive Logger is Inactive\n");
		}

		if(received_data == UNKNOWN) printf("<!> Parameter is undefined\n");

	}

	return 0;
}