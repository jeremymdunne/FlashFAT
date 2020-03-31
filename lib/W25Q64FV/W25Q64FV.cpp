#include <W25Q64FV.h>


int W25Q64FV::eraseBlock64(uint32_t address){
  if(isBusy()) return FLASH_BUSY;
  writeEnable();
  select();
  SPI.transfer(ERASE_BLOCK_64);
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer((address >> 0) & 0xFF);
  release();
  writeStatus = false;
  return 0;
}

int W25Q64FV::eraseBlock32(uint32_t address){
  if(isBusy()) return FLASH_BUSY;
  writeEnable();
  select();
  SPI.transfer(ERASE_BLOCK_32);
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer((address >> 0) & 0xFF);
  release();
  writeStatus = false;
  return 0;
}

int W25Q64FV::eraseSector(uint32_t address){
  if(isBusy()) return FLASH_BUSY;
  writeEnable();
  select();
  SPI.transfer(ERASE_SECTOR);
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer((address >> 0) & 0xFF);
  release();
  writeStatus = false;
  return 0;
}

int W25Q64FV::eraseAll(){
  if(isBusy()) return FLASH_BUSY;
  writeEnable();
  select();
  SPI.transfer(ERASE_ALL);
  release();
  writeStatus = false;
  return 0;
}

int W25Q64FV::read(uint32_t address, byte *buff, int n){
  if(isBusy()) return FLASH_BUSY;
  select();
  SPI.beginTransaction(SPISettings(100000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(READ_DATA);
  //send the 24 bit address. Note, last 8 bits must be 0 as this writes to a 256 byte page
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer((address >> 0) & 0xFF);
  //read the data
  for (int i = 0; i < n; i++) {
    buff[i] = SPI.transfer(0);
  }
  SPI.endTransaction();
  release();
  return 0;
}

int W25Q64FV::read256(uint32_t address, byte *buff){
  if(isBusy()) return FLASH_BUSY;
  select();
  SPI.beginTransaction(SPISettings(100000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(READ_DATA);
  //send the 24 bit address. Note, last 8 bits must be 0 as this writes to a 256 byte page
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer((address >> 0) & 0xFF);
  //read the data
  for (int i = 0; i < 256; i++) {
    buff[i] = SPI.transfer(0);
  }
  SPI.endTransaction();
  release();
  return 0;
}

int W25Q64FV::checkWriteEnable(){
  if(writeStatus == false){
    writeEnable();
    //check for errors
    int busyStatus = waitUntilFree();
    if(busyStatus != 0) return busyStatus;
  }
}

int W25Q64FV::write256(uint32_t address, byte *buff){
  return write(address,buff,256);
}

int W25Q64FV::write(uint32_t address, byte *buff, uint16_t length){
  if(isBusy()) return FLASH_BUSY;
  writeEnable();
  select();
  SPI.beginTransaction(SPISettings(100000000, MSBFIRST, SPI_MODE0));
  //send the 24 bit address. Note, last 8 bits must be 0 as this writes to a 256 byte page
  SPI.transfer(PROGRAM_PAGE);
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer((address >> 0) & 0xFF);
  //send the data
  for (uint16_t i = 0; i < length; i++) {
    SPI.transfer(buff[i]);
  }
  SPI.endTransaction();
  release();
  return 0;
}

int W25Q64FV::writeEnable(){
  select();
  SPI.transfer(WRITE_ENABLE);
  release();
  writeStatus = true;
  return 0;
}

int W25Q64FV::writeDisable(){
  select();
  SPI.transfer(WRITE_DISABLE);
  release();
  writeStatus = false;
  return 0;
}

bool W25Q64FV::isBusy(){
  //read the status bit
  select();
  SPI.transfer(READ_STATUS_REG_1);
  int value = SPI.transfer(0);
  //Serial.println(value);
  //Serial.println((value & 0));
  release();
  if((value & 1)){
    return true;
  }
  return false;
}


int W25Q64FV::waitUntilFree(unsigned long timeoutMicros){
  //grab start time
  select();
  SPI.transfer(READ_STATUS_REG_1);
  long start = micros();
  while(long(micros()) - start < timeoutMicros){
    if((SPI.transfer(0) & 0) == 0) {
        release();
        return 0;
    }
  }
  release();
  return BUSY_TIMEOUT_ERROR;
}

void W25Q64FV::release(){
  digitalWrite(chipSelect,HIGH);
}

void W25Q64FV::select(){
  //pull high then low to fully release and select the line
  release();
  digitalWrite(chipSelect,LOW);
}

int W25Q64FV::init(int ssPin, bool initSPI){
  //assign chip select
  chipSelect=ssPin;
  //initialize and pull high
  pinMode(chipSelect,OUTPUT);
  release();
  //initialize spi if necessary
  if(initSPI){
    SPI.begin();
    SPI.setDataMode(0);
    SPI.setBitOrder(MSBFIRST);
  }
  //check device id to see if connection is successful
  byte id[3];

  readID(&id[0], &id[1], &id[2]);
  //Serial.println(String(id[0]) + " " + String(id[1]) + " " + String(id[2])); 
  if((id[0] != 0) || (id[1] != 0) || (id[2] != 0)){
    //return the wait until relase
    return waitUntilFree();
  }

  return CONNECTION_NOT_SUCCESSFUL;
}

void W25Q64FV::readID(byte *b1, byte *b2, byte *b3) {
  select();
  SPI.transfer(CHIP_COMMAND_ID);
  *b1 = SPI.transfer(0); // manufacturer id
  *b2 = SPI.transfer(0); // memory type
  *b3 = SPI.transfer(0); // capacity
  release();
}
