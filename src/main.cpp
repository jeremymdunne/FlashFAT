#include <Arduino.h>
#include <FlashFAT.h> 

void setup() {
  // put your setup code here, to run once:
  delay(2000); 
  Serial.begin(115200); 
  Serial.println("Hello, World!");
  FlashFAT storage; 
  int status = storage.init(PA2); 
  Serial.println("Storage Status: " + String(status));
  // erase all files 
  storage.erase_all_files(); 
  // make a file 
  storage.open_file_write(); 
  byte buffer[512]; 
  for(int i = 0; i < 256; i ++){
    buffer[i] = 5; 
    buffer[i + 256] = (uint8_t) 15; 
  }
  // write it 
  // write 4 kb of data 
  long start = micros(); 
  storage.write(buffer, 512); 
  long end = micros(); 
  Serial.println("Del time: " + String(end - start));
  storage.close();
  Serial.println("Last item: " + String(buffer[511]));
  // attempt to read a file from before 
  byte buffer1[512]; 
  storage.open_file_read(0); 
  uint16_t len = storage.read(buffer1,512); 
  for(int r = 0; r < 32; r ++){
    for(int c = 0; c < 16; c ++){
      Serial.print(buffer1[r*16 + c]);
      Serial.print("\t"); 
    }
    Serial.println();
  }
  storage.close();
  Serial.println("Done");

}

void loop() {
  // put your main code here, to run repeatedly:
}