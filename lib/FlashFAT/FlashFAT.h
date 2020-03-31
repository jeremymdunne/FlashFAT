/**
 * implements a File Allocation Table like system for flash storage 
 * 
 * Main component is a 2 page file allocation table consisting of 
 *  3 identifier bytes (spells out "FAT" in ascii) to signify that the chip is set up
 *  Number of files available (1 byte)
 *  Pairs of Page Start (3 bytes), Page End (3 bytes), and offsets (1 byte) for every 'file'   
 * 
 * currently only implemented on the w25q64fv flash chip
 * 
 * does not support actually erasing data, just erasing file entries 
 * 
 * one quirk of the falsh system is that the storage needs to be erased before it is written to
 *  This requires the FAT table to be separated from the first file 
 *  The minimum erase size is a sector (4kb), so the first file can be stored starting in 0x1000
 */ 


#ifndef _FLASH_FAT_H_
#define _FLASH_FAT_H_
#include <Arduino.h>
#include <W25Q64FV.h> 

// needs to be a multiple of 256 
#define FLASH_FAT_WRITE_CACHE_SIZE                  256 
#define FLASH_FAT_MAX_FILES                         32 
#define FLASH_FAT_FILE_START_ADDRESS                0x1000 

#define FLASH_FAT_STANDARD_TIMEOUT_MILLIS           1000

// error definitions 
#define FLASH_FAT_NO_FAT_TABLE_FOUND                -11
#define FLASH_FAT_FLASH_TIMEOUT_ERROR               -12
#define FLASH_FAT_FILE_DESCRIPTOR_BAD               -13 
#define FLASH_FAT_MODE_MISMATCH                     -14
#define FLASH_FAT_NO_MORE_SPACE                     -15 

//#define FLASH_FAT_DEBUG
//#define FLASH_FAT_VERBOSE
#define FLASH_FAT_ERROR_HEADER                      "FLASH FAT ERROR: "
#define FLASH_FAT_VERBOSE_HEADER                    "FLASH FAT VERBOSE: "


class FlashFAT{
public: 
    // basic file definition 
    struct File{
        uint32_t start_page; // 24 bit address
        uint32_t end_page; // 24 bit page address
        uint8_t end_offset; // 8 bit offset (last written data), 0 = 0, 255 = last byte in page 
    };
    
    // organizational method 
    struct FileAllocationTable{
        FlashFAT::File files[FLASH_FAT_MAX_FILES]; // statically initialized for memory purposes 
        uint8_t num_files; 
    };
    
    // read-write mode definition 
    enum MODE{
        READ, WRITE, NONE
    }; 

    /**
     * initialize the file system
     * check the connection with the storage medium 
     * @param cs_pin cs pin for the storage medium 
     * @param force_init_table forcibly init the file allocation table even if one is 
     *  present 
     * @return error or success code 
     */
    int init(int cs_pin, bool force_init_table = false); 

    /**
     * open a file to read 
     * @param fd file to open
     * @return error or success code 
     */ 
    int open_file_read(uint8_t fd);

    /** 
     * open a file to write 
     * only able to write to the last file 
     * @return error or success code
     */
    int open_file_write(); 

    /**
     * close out a file
     * @return error or success code
     */
    int close(); 

    /** 
     * write to the device
     * actually uses a cache to minimize writing delays 
     * @param buffer buffer to write 
     * @param length length of the buffer 
     * @return error or success code
     */ 
    int write(byte *buffer, uint16_t length); 

    /**
     * read the data using a stream-like approach 
     * @param buffer buffer to store the data in 
     * @param length length of data to read 
     * @return number of bytes written to the buffer 
     */ 
    int read(byte *buffer, uint16_t length); 

    /**
     * get the remaining size of the file 
     * @return number of bytes remaining in the file 
     */ 
    uint32_t peek(); 

    /**
     * get the file allocation table 
     * @param table allocation table to copy 
     * @return error or success code 
     */ 
    int get_file_allocation_table(FileAllocationTable *table); 

    /**
     * erase the last file 
     * @return number of files remaining
     */ 
    int erase_last_file(); 

    /**
     * erase all files 
     * @return error or success code
     */ 
    int erase_all_files(); 


private: 
    W25Q64FV _flash; 
    MODE _mode = NONE; 
    uint32_t _current_address = 0;
    uint32_t _max_address = 0;  
    FileAllocationTable _master_table; 
    byte _write_cache[FLASH_FAT_WRITE_CACHE_SIZE]; 
    uint16_t _write_cache_index = 0; 

    /**
     * create the FAT table using the masterTable contents 
     * @return error or success code
     */ 
    int create_fat_table(); 

    /**
     * attempt to read the FAT table from the device 
     * @return error or success code 
     */ 
    int read_fat_table(); 

    /** 
     * wait until the busy flag has dissapeared on the storage system 
     * needed when long writes or erase operations have been conducted 
     * @param timeout timeout in millis 
     * @return error or success code 
     */ 
    int wait_until_free(unsigned long timeout = FLASH_FAT_STANDARD_TIMEOUT_MILLIS); 

    /**
     * check if the storage is busy 
     * @return true if busy, false otherwise 
     */ 
    bool busy(); 

    /** 
     * write the FAT table 
     * @return error or success code 
     */ 
    int write_fat_table(); 

    #ifdef FLASH_FAT_DEBUG
    
    /**
     * print out an error message with an appropriate header 
     * @param message message to display
     */ 
    void print_error(String message); 

    /**
     * print out a 256 byte buffer 
     * pads data with 255 if 256 bytes are not provided 
     * @param buffer buffer to print 
     * @param length length of valid data 
     */
    void print_256_byte_buffer(byte *buffer, uint8_t length = 255); 

    /** 
     * print out the fat table contents 
     */ 
    void print_fat_table(); 

    #ifdef FLASH_FAT_VERBOSE

    /**
     * print out a verbose message 
     * @param message message to display 
     */ 
    void print_verbose(String message); 

    #endif 
    #endif 

}; 



#endif