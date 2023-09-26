# CubeSTEP

CubeSat Technology Exploration Project

California State Polytechnic University, Pomona

# Payload Processor Firmware

This code serves as a demo of an I2C driver for the CubeSTEP Payload Processor. While being tested, it allowed a Raspberry Pi to emulate the OBC so that I2C could be correctly implemented into the payload's firmware.

## How to use

Build with g++ with the `-lwiringPi` parameter. For example:

```
g++ -o i2c_operation i2c_operation.cpp -lwiringPi
```

Run `./i2c_operation` your console with the operation you would like performed with any necessary parameters. All arguments must be in hexadecimal. The first argument should be an opcode (1 byte). If the operation requires a parameter, include it as the second argument. The size of the parameter should not exceed the size listed in the documentation. For example, the below command can be used to set the device's time (opcode 0x93) to epoch 1694565311 (which is equal to 0x650103BF).

```
./i2c_operation 93 650103BF
```

After executing, the command will first perform the following actions in the specified order:

1. Parses opcode from command line argument

2. Sends opcode to the device

3. Wait for confirmation from the device

If there is a parameter, the following will happen:

4. Parses pararmeter from command line argument

5. Sends parameter to the device

If the device is expected to return data, the following will happen

6. Waits for device to send data start byte

7. Receives data from device

The commands are documented in OneNote. Contact Benjamin James.