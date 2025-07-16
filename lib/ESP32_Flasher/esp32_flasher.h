// esp32_flasher.h
#ifndef ESP32_FLASHER_H
#define ESP32_FLASHER_H

#include <functional>
#include "Arduino.h"

// callback template definition
typedef std::function<void(void)> THandlerFunction;

// Timeouts and retry parameters
#define DEFAULT_TIMEOUT 1000              // Standard timeout for most operations (ms)
#define DEFAULT_FLASH_TIMEOUT 3000        // Extended timeout for flash operations (ms)
#define ERASE_REGION_TIMEOUT_PER_MB 10000 // Timeout per MB for flash erase (ms)
#define PADDING_PATTERN 0xFF              // Pattern used for padding incomplete blocks
#define MAX_TRIALS 5                      // Maximum connection attempts

// SLIP protocol special characters
#define DELIMITER 0xC0                    // Frame delimiter for SLIP protocol
static const uint8_t C0_REPLACEMENT[2] = {0xDB, 0xDC};  // Escape sequence for 0xC0
static const uint8_t DB_REPLACEMENT[2] = {0xDB, 0xDD};  // Escape sequence for 0xDB

// Hardware configuration
#define BOOT_PIN 26                       // ESP32 BOOT pin for entering download mode
#define EN_PIN   25                       // ESP32 EN (Reset) pin
#define SYNC_TIMEOUT 1000                 // Timeout for sync operation (ms)

// Flash parameters
#define ESP_FLASH_MAX_SIZE 0x2470000      // Maximum flash size (~37MB)
#define ESP_FLASH_OFFSET   0x10000        // Default flash offset (64KB) app0
#define ESP_FLASH_SIZE     0x140000       // Default flash size         app0

// Communication direction flags
#define READ_DIRECTION  1
#define WRITE_DIRECTION 0

// Utility macros
#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)

// ESP32 SPI registers (used for flash operations)
#define ESP32_REG_BASE   0x3ff42000
#define ESP32_REG_CMD    ESP32_REG_BASE
#define ESP32_REG_USR    (ESP32_REG_BASE + 0x1c)
#define ESP32_REG_USR1   (ESP32_REG_BASE + 0x20)
#define ESP32_REG_USR2   (ESP32_REG_BASE + 0x24)
#define ESP32_REG_W0     (ESP32_REG_BASE + 0x80)
#define ESP32_REG_MOSI   (ESP32_REG_BASE + 0x28)
#define ESP32_REG_MISO   (ESP32_REG_BASE + 0x2c)

// Status codes
#define SUCCESS            0  // Operation completed successfully
#define ERR_FAIL          1  // General failure
#define ERR_TIMEOUT       2  // Operation timed out
#define ERR_IMG_SIZE      3  // Image too large for flash
#define ERR_INVALID_RESP  4  // Invalid response received

// Command codes
#define FLASH_BEGIN 0x02    // Start flash operation
#define FLASH_DATA  0x03    // Flash data block
#define FLASH_END   0x04    // End flash operation
#define SYNC        0x08    // Synchronize with target
#define WRITE_REG   0x09    // Write register
#define READ_REG    0x0A    // Read register
#define SPI_ATTACH  0x0D    // Attach SPI flash

// Error codes
#define RESPONSE_OK      0x00  // Command executed successfully
#define INVALID_COMMAND  0x05  // Invalid parameters or length
#define COMMAND_FAILED   0x06  // Command execution failed
#define INVALID_CRC      0x07  // CRC check failed
#define FLASH_WRITE_ERR  0x08  // Flash write verification failed
#define FLASH_READ_ERR   0x09  // SPI flash read failed
#define READ_LENGTH_ERR  0x0a  // Read length too large
#define DEFLATE_ERROR    0x0b  // Decompression error

class ESP32Flasher {
  private:
    uint32_t s_flash_write_size = 0;    // Current flash write block size
    uint32_t s_sequence_number = 0;     // Packet sequence counter
    int32_t s_time_end = 0;            // Operation timeout timestamp
    uint32_t _undownloadByte; 	    /* undownload byte of tft file */
    THandlerFunction _updateProgressCallback;

    // Internal command handlers
    int verifyResponse(uint8_t command);
    int flashBeginCmd(uint32_t offset, uint32_t erase_size, uint32_t block_size, uint32_t blocks_to_write);
    int flashDataCmd(const uint8_t *data, uint32_t size);
    int flashEndCmd(bool stay_in_loader);
    int espSyncHandle(void);
    int spiAttachCmd(uint32_t config);

    // Flash operation handlers
    int espFlashStart(uint32_t flash_address, uint32_t image_size, uint32_t block_size);
    int espFlashWrite(void *payload, uint32_t size);
    int epsFlashFinish(bool reboot);
    //int flashBinary(File& file, uint32_t size, uint32_t address);
    int flashBinaryStream(Stream &myFile, uint32_t size, uint32_t address);

  public:
	  void setUpdateProgressCallback(THandlerFunction value);
    void espFlasherInit(void);           // Initialize flasher
    bool espConnect(void);               // Establish connection
    //void espFlashBinFile(const char* bin_file_name);  // Flash binary file
    void espFlashBinStream(Stream &myFile,uint32_t size);  // Flash binary file


};

#endif