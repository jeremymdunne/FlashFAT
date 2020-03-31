#ifndef _W25Q64FV_H_
#define _W25Q64FV_H_

#include <SPI.h>
#include <Arduino.h>

#define WRITE_ENABLE       0x06 // Address Write Enable
#define WRITE_DISABLE      0x04 // Address Write Disable
#define ERASE_ALL         0x60
#define ERASE_SECTOR      0x20
#define ERASE_BLOCK_32    0x52
#define ERASE_BLOCK_64    0xD8
#define PROGRAM_PAGE      0x02
#define READ_DATA         0x03
#define READ_STATUS_REG_1 0x05
#define CHIP_COMMAND_ID   0x9f
#define CONNECTION_NOT_SUCCESSFUL -1
#define BUSY_TIMEOUT_ERROR -2
#define FLASH_BUSY -3
#define STANDARD_MICROS_TIMEOUT 10000

/*
Class handles the bare minimum of communicating with the spi flash chip
Other code will be responsible for encoding and decoding data, erasing sectors before writing
handling errors, and other things.
*/

class W25Q64FV{
public:
  int init(int ssPin, bool initSPI=true);
  int write256(uint32_t address, byte *buff);
  int write(uint32_t address, byte *buff, uint16_t length);
  int eraseSector(uint32_t address);
  int eraseBlock32(uint32_t address);
  int eraseBlock64(uint32_t address);
  int eraseAll();
  bool isBusy();
  int read256(uint32_t address, byte *buff);
  int read(uint32_t address, byte *buff, int n);
  int readSecurity(byte *buffer);
  int writeEnable();
  int writeDisable();
  int waitUntilFree(unsigned long timeoutMicros = STANDARD_MICROS_TIMEOUT);

private:
  bool writeStatus = false;
  int chipSelect = -1;
  void readID(byte *b1, byte *b2, byte *b3);
  void readStatusByte(byte *b);
  void release();
  void select();
  int checkWriteEnable();
};


#endif
