#include <FlashFAT.h> 


int FlashFAT::open_file_write(){
    // close out anything 
    int status = close(); 
    if(status < 0) return status; 
    // make sure not the last available file 
    if(_master_table.num_files >= 255) return FLASH_FAT_NO_MORE_SPACE; 
    // find the next available address 
    uint32_t last_used_page = _master_table.files[_master_table.num_files-1].end_page; 
    // find the next available address  
    uint32_t next_address = ((last_used_page << 8) / (1<<12) + 1) << 12; 
    // add to the index 
    _master_table.num_files ++; 
    _master_table.files[_master_table.num_files-1].start_page = next_address >> 8; 
    _master_table.files[_master_table.num_files-1].end_page = next_address >> 8;
    _master_table.files[_master_table.num_files-1].end_offset = 0;
    // write the fat table 
    write_fat_table(); 
    _current_address = next_address; 
    _mode = WRITE; 
    #ifdef FLASH_FAT_DEBUG
        print_verbose("Creating new file to write: "); 
        print_fat_table(); 
        print_verbose("Start address for writing: " + String(_current_address));
    #endif  
    // erase the immediate 4kb 
    _flash.eraseSector(_current_address); 
    _max_address = _current_address + 4096; 

    // return success

    return 0; 
}

int FlashFAT::write(byte *buffer, uint16_t length){
    // check the mode 
    if(_mode != WRITE) return FLASH_FAT_MODE_MISMATCH; 
    while(_max_address <= _current_address + length){
        _flash.eraseSector(_max_address);
        _max_address += 4096; 
        #ifdef FLASH_FAT_VERBOSE
            print_verbose("Erasing next 4kb "); 
        #endif 
        wait_until_free();
    }
    // check if the cache can be filled up with the data 
    if(_write_cache_index + length >= FLASH_FAT_WRITE_CACHE_SIZE && _write_cache_index != 0){
        #ifdef FLASH_FAT_VERBOSE
            print_verbose("Write cache would be filled, copying contents");
            print_verbose("Bytes to copy over: " + String(FLASH_FAT_WRITE_CACHE_SIZE - _write_cache_index));
        #endif 
        // copy in the data to the cache 
        memcpy(&_write_cache[_write_cache_index], buffer, FLASH_FAT_WRITE_CACHE_SIZE - _write_cache_index);
        // write the cache 
        // must be written in 256 byte pages 
        for(uint32_t i = 0; i < FLASH_FAT_WRITE_CACHE_SIZE/256; i ++){
            // double check no erasing is required 
            wait_until_free();
            _flash.write(_current_address, &_write_cache[i*256], 256);
            #ifdef FLASH_FAT_VERBOSE
                print_verbose("Writing Cache Buffer: "); 
                print_256_byte_buffer(_write_cache); 
            #endif 
            _current_address += 256; 

        }
        // update pointers 
        buffer = &buffer[FLASH_FAT_WRITE_CACHE_SIZE - _write_cache_index];
        // update the length value 
        length -= (FLASH_FAT_WRITE_CACHE_SIZE - _write_cache_index); 
        _write_cache_index = 0; 
    }
    // otherwise, just fill up the cache and return 
    else if(_write_cache_index != 0){
        memcpy(&_write_cache[_write_cache_index], buffer, length);
        _write_cache_index += length; 
        return 0; 
    }
    // at this point, there is data remaining to be directly copied 
    // copy up until the last non-complete cache size 
    uint32_t copy_size = (uint32_t)(length / FLASH_FAT_WRITE_CACHE_SIZE) * FLASH_FAT_WRITE_CACHE_SIZE; 
    #ifdef FLASH_FAT_VERBOSE
        print_verbose("Copy size: " + String(copy_size)); 
    #endif
    // write the data in 256 byte chunks 
    for(uint32_t i = 0; i < copy_size/256; i ++){
        wait_until_free();
        _flash.write(_current_address, buffer, 256);
         #ifdef FLASH_FAT_DEBUG
            print_verbose("Writing Buffer: "); 
            print_256_byte_buffer(buffer); 
        #endif
        _current_address += 256; 
        buffer = &buffer[256];
        length -= 256; 
        
    }
    // shove the remaining in the buffer 
    if(length > 0){
        #ifdef FLASH_FAT_DEBUG
            print_verbose("Saving remaing bytes: " +String(length)); 
        #endif 
        memcpy(_write_cache, buffer, length);
        _write_cache_index = length;
    }
    return 0; 
}

int FlashFAT::open_file_read(uint8_t fd){
    // first sanity check a file is not already open 
    int status = close(); 
    if(status < 0) return status; 
    // check if the fd is valid 
    if(_master_table.num_files <= fd){
        return FLASH_FAT_FILE_DESCRIPTOR_BAD; 
    }
    // otherwise, set our mode 
    _mode = READ; 
    // set the current address to the initial start page address 
    _current_address = _master_table.files[fd].start_page << 8;
    // set the maximum address we can read to 
    _max_address =  _master_table.files[fd].end_page << 8 | _master_table.files[fd].end_offset;
    // report success 
    #ifdef FLASH_FAT_VERBOSE
        print_verbose("Start Address: " + String(_current_address)); 
    #endif
    return 0; 
}

int FlashFAT::read(byte *buffer, uint16_t length){
    // double check valid command 
    if(_mode != READ){
        return FLASH_FAT_MODE_MISMATCH; 
    }
    // check the maximum address we can read 
    if(_current_address + length > _max_address + 1){
        length = _max_address - _current_address; 
    }
    // read the datas 
    wait_until_free();
    int status = _flash.read(_current_address, buffer, length); 
    if(status < 0) return status; 
    // increment the address 
    _current_address += length; 
    return length; 
}

uint32_t FlashFAT::peek(){
    // return the difference in addresses 
    return _max_address - _current_address; 
}

int FlashFAT::close(){
    // change the mode, write the remaining contents to file 
    switch(_mode){
        case READ:
            // change the mode, should be good to go 
            _mode = NONE; 
            break; 
        case WRITE: {
            // write the remaining data
            uint32_t temp = 0; 
            // check if erasing is required 
            #ifdef FLASH_FAT_VERBOSE
                print_verbose("Close, amount left: " + String(_write_cache_index)); 
            #endif 
            if(_write_cache_index != 0){
                wait_until_free(); 
                while(_max_address < _current_address + _write_cache_index){
                    _flash.eraseSector(_max_address);
                    _max_address += 4096; 
                    #ifdef FLASH_FAT_VERBOSE
                        print_verbose("Erasing next 4kb "); 
                    #endif 
                    wait_until_free();
                    
                }
                // write in 256 byte blocks 
                for(uint32_t i = 0; i < _write_cache_index/256; i ++){
                    wait_until_free(); 
                    _flash.write(_current_address, &_write_cache[i*256], 256);
                    _current_address += 256; 
                    _write_cache_index -= 256; 
                    temp += 256; 
                }
                // write the remaining 
                if(_write_cache_index > 0){
                    wait_until_free(); 
                    _flash.write(_current_address, &_write_cache[temp], _write_cache_index - temp);
                    // catch the new address, this is needed to close out the file 
                    _current_address += _write_cache_index - temp; 
                }
            }
            // set the offset and page values 
            _master_table.files[_master_table.num_files-1].end_page = (_current_address-1) >> 8; 
            _master_table.files[_master_table.num_files-1].end_offset = (uint8_t)(_current_address -1); 
            write_fat_table(); 

            #ifdef FLASH_FAT_VERBOSE
                print_fat_table();
            #endif
            }break;
        case NONE:
            break; 
    }
    return 0; 
}

int FlashFAT::erase_all_files(){
    // overwrite the num_files 
    _master_table.num_files = 0; 
    // write it back 
    return write_fat_table(); 
}

int FlashFAT::erase_last_file(){
    // decrement the num_files 
    if(_master_table.num_files > 0)
    _master_table.num_files --; 
    write_fat_table();
    return _master_table.num_files; 
}


int FlashFAT::get_file_allocation_table(FileAllocationTable *table){
    // copy over the master table 
    memcpy(table, &_master_table, sizeof(_master_table)); 
}


int FlashFAT::init(int cs_pin, bool force_init_table){
    // init the flash chip 
    int status = _flash.init(cs_pin, true); 
    
    /*
    // trial runs 
    // create a bogus master file 
    _master_table.num_files = 1; 
    _master_table.files[0].start_page = 4096 >> 8; 
    _master_table.files[0].end_page = (4096 >> 8) + 5; 
    _master_table.files[0].end_offset = 5; 
    write_fat_table();
    // clear the fat table 
    _master_table = FileAllocationTable(); 
    read_fat_table();
    // print out the fat table contents 
    #ifdef FLASH_FAT_DEBUG
        print_fat_table(); 
    #endif 
    byte tempBuffer[256]; 
    _flash.read(0, tempBuffer, 256); 
    #ifdef FLASH_FAT_DEBUG
        print_verbose("FAT table contents: "); 
        print_256_byte_buffer(tempBuffer); 
    #endif 

    return 0; 
    if(status < 0){
        return status; 
    }
    */
    // init the table if forced to do so 
    if(force_init_table){
        write_fat_table(); 
    }
    // read the table in 
    else{
        read_fat_table();
        #ifdef FLASH_FAT_DEBUG
            print_fat_table(); 
        #endif  
    }
    return 0;
}

int FlashFAT::write_fat_table(){
    // first start the sector erase 
    if(wait_until_free() < 0) return FLASH_FAT_FLASH_TIMEOUT_ERROR;
    _flash.eraseSector(0); 
    // set the first 3 bytes to the identifier 
    byte temp_buffer[256]; 
    temp_buffer[0] = 'F'; 
    temp_buffer[1] = 'A'; 
    temp_buffer[2] = 'T';
    // write in the contents 
    temp_buffer[3] = _master_table.num_files; 
    
    for(uint8_t i = 0; i < _master_table.num_files; i ++){
        temp_buffer[4 + i*7] = _master_table.files[i].start_page >> 16; 
        temp_buffer[4 + i*7 + 1] = _master_table.files[i].start_page >> 8; 
        temp_buffer[4 + i*7 + 2] = _master_table.files[i].start_page >> 0; 

        temp_buffer[4 + i*7 + 3] = _master_table.files[i].end_page >> 16; 
        temp_buffer[4 + i*7 + 4] = _master_table.files[i].end_page >> 8; 
        temp_buffer[4 + i*7 + 5] = _master_table.files[i].end_page >> 0; 

         temp_buffer[4 + i*7 + 6] = _master_table.files[i].end_offset; 
    }
    #ifdef FLASH_FAT_VERBOSE
        print_verbose("Attempting to write fat table: "); 
        print_256_byte_buffer(temp_buffer); 
    #endif 
 
    // write it 
    //wait until free 
    if(wait_until_free() < 0) return FLASH_FAT_FLASH_TIMEOUT_ERROR;
    return _flash.write(0,temp_buffer, 4 + _master_table.num_files * 7); 
}

int FlashFAT::read_fat_table(){
    // read in the fat table 
    // make sure it actually is a legit table as well 
    byte temp_buffer[256]; 
    //wait until free 
    if(wait_until_free() < 0) return FLASH_FAT_FLASH_TIMEOUT_ERROR;
    int status = _flash.read(0, temp_buffer, 256); 
    
    if(status < 0) return status; 
    // check for the correct identifier 
    if(temp_buffer[0] != (byte)'F') return FLASH_FAT_NO_FAT_TABLE_FOUND;
    if(temp_buffer[1] != (byte)'A') return FLASH_FAT_NO_FAT_TABLE_FOUND;
    if(temp_buffer[2] != (byte)'T') return FLASH_FAT_NO_FAT_TABLE_FOUND;
    // read in the contents 
    _master_table.num_files = temp_buffer[3];
    
    for(uint8_t i = 0; i < _master_table.num_files; i ++){
        _master_table.files[i].start_page = ((uint32_t)temp_buffer[4 + i*7] << 16) | ((uint32_t)temp_buffer[4 + i*7 + 1] << 8) | ((uint32_t)temp_buffer[4 + i*7 + 2]); 
        _master_table.files[i].end_page = (uint32_t)(((uint32_t)temp_buffer[4 + i*7 + 3] << 16) | ((uint32_t)temp_buffer[4 + i*7 + 4] << 8) | ((uint32_t)temp_buffer[4 + i*7 + 5]));
        
        _master_table.files[i].end_offset =  temp_buffer[4 + i*7 + 6]; 
    }
    // return success
    return 0; 
}

bool FlashFAT::busy(){
    return _flash.isBusy(); 
}

int FlashFAT::wait_until_free(unsigned long timeout){
    unsigned long long start = millis(); 
    while(millis() < start + timeout && _flash.isBusy()); 
    if(_flash.isBusy()){
        return -1; 
    }
    return 0; 
}

#ifdef FLASH_FAT_DEBUG 
void FlashFAT::print_error(String message){
    //print the message with a tag 
    Serial.print(FLASH_FAT_ERROR_HEADER); 
    Serial.print(" "); 
    Serial.print(message);
    Serial.println();  
}

void FlashFAT::print_256_byte_buffer(byte *buffer, uint8_t length){
    for(uint8_t row = 0; row < 16; row ++){
        for(uint8_t col = 0; col < 16; col ++){
            if((length < 255) || (row*16 + col > length)){
                Serial.print("255"); 
            }
            else{
                Serial.print(buffer[row*16 + col]); 
            }
            Serial.print("\t"); 
        }
        Serial.println(); 
    }
    Serial.println();
}

void FlashFAT::print_fat_table(){
    // print out header info 
    Serial.println("FAT Table Info:"); 
    Serial.println("Num Files: " + String(_master_table.num_files)); 
    for(uint8_t i = 0; i < _master_table.num_files; i ++){
        Serial.println("\tFile: " + String(i) + ":" + "\tStart Page: " + String(_master_table.files[i].start_page) + "\tEnd Page: " + String(_master_table.files[i].end_page) + "\tPage Offset: " + String(_master_table.files[i].end_offset)); 
    }
    Serial.println(); 
}

#ifdef FLASH_FAT_VERBOSE
void FlashFAT::print_verbose(String message){
    //print the message with a tag 
    Serial.print(FLASH_FAT_VERBOSE_HEADER); 
    Serial.print(" "); 
    Serial.print(message);
    Serial.println();  
}
#endif 
#endif 