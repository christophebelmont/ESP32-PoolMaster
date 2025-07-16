#include "esp32_flasher.h"

/**
   Calculate timeout based on data size
   @param size_bytes: Size of data in bytes
   @param time_per_mb: Time allowance per megabyte
   Returns: Calculated timeout value
*/
static uint32_t timeout_per_mb(uint32_t size_bytes, uint32_t time_per_mb) {
  uint32_t timeout = time_per_mb * (size_bytes / 1e6);
  timeout = MAX(timeout, DEFAULT_FLASH_TIMEOUT);
  return timeout;
}

/**
   Calculate checksum for data verification
   @param data: Pointer to data buffer
   @param size: Size of data
   Returns: Calculated checksum
*/
static uint8_t compute_checksum(const uint8_t *data, uint32_t size) {
  uint8_t checksum = 0xEF;  // Initial value
  while (size--) {
    checksum ^= *data++;
  }
  return checksum;
}

void ESP32Flasher::setUpdateProgressCallback(THandlerFunction value){
	_updateProgressCallback = value;
}

/**
   Verify and process response from ESP32
   @param command: Expected command to verify against
   Returns: SUCCESS or error code
*/
int ESP32Flasher::verifyResponse(uint8_t command) {
  uint8_t ch;
  uint8_t buff[8];  // Response buffer: direction(1) + command(1) + size(2) + value(4)
  // Searching for start delimiter
  do {
    // Step 1: Find start delimiter
    do {
      int32_t remaining = (s_time_end - millis());
      if (remaining <= 0) {
        remaining = 0; // Timeout approaching while waiting for delimiter
      }
      Serial2.setTimeout(remaining);

      int read = Serial2.readBytes(&ch, 1);
      if (read < 0) {
        // Read operation failed
        return ERR_FAIL;
      } else if (read < 1) {
        // Timeout waiting for start delimiter
        return ERR_TIMEOUT;
      }
    } while (ch != DELIMITER);

    // Step 2: Skip any extra delimiters
    do {
      int32_t remaining = (s_time_end - millis());
      if (remaining <= 0) {
        remaining = 0;
      }
      Serial2.setTimeout(remaining);

      int read = Serial2.readBytes(&ch, 1);
      if (read < 0) {
        Serial.println("[ERROR] Failed reading extra delimiters");
        return ERR_FAIL;
      } else if (read < 1) {
        Serial.println("[ERROR] Timeout reading extra delimiters");
        return ERR_TIMEOUT;
      }
    } while (ch == DELIMITER);

    // Store first byte (already read)
    buff[0] = ch;

    // Step 3: Read response packet
    for (uint32_t i = 1; i < 8; i++) {
      int32_t remaining = (s_time_end - millis());
      if (remaining <= 0) {
        remaining = 0;
      }
      Serial2.setTimeout(remaining);

      int read = Serial2.readBytes(&ch, 1);
      if (read < 0) {
        Serial.printf("[ERROR] Failed reading byte %d of response\n", i);
        return ERR_FAIL;
      } else if (read < 1) {
        Serial.printf("[ERROR] Timeout reading byte %d of response\n", i);
        return ERR_TIMEOUT;
      }

      // Handle SLIP encoding
      if (ch == 0xDB) {
        remaining = (s_time_end - millis());
        if (remaining <= 0) {
          remaining = 0;
        }
        Serial2.setTimeout(remaining);

        read = Serial2.readBytes(&ch, 1);
        if (read < 0) {
          Serial.println("[ERROR] Failed reading SLIP escape sequence");
          return ERR_FAIL;
        } else if (read < 1) {
          Serial.println("[ERROR] Timeout reading SLIP escape sequence");
          return ERR_TIMEOUT;
        }

        if (ch == 0xDC) {
          buff[i] = 0xC0;
        } else if (ch == 0xDD) {
          buff[i] = 0xDB;
        } else {
          Serial.println("[ERROR] Invalid SLIP escape sequence");
          return ERR_INVALID_RESP;
        }
      } else if (ch == DELIMITER) {
        break;
      } else {
        buff[i] = ch;
      }
    }

    // Step 4: Read status bytes
    uint8_t status[2];  // [0] = failed flag, [1] = error code
    for (uint32_t i = 0; i < 2; i++) {
      int32_t remaining = (s_time_end - millis());
      if (remaining <= 0) {
        remaining = 0;
      }
      Serial2.setTimeout(remaining);

      int read = Serial2.readBytes(&ch, 1);
      if (read < 0) {
        Serial.printf("[ERROR] Failed reading status byte %d\n", i);
        return ERR_FAIL;
      } else if (read < 1) {
        Serial.printf("[ERROR] Timeout reading status byte %d\n", i);
        return ERR_TIMEOUT;
      }

      // Handle SLIP encoding for status
      if (ch == 0xDB) {
        remaining = (s_time_end - millis());
        if (remaining <= 0) {
          remaining = 0;
        }
        Serial2.setTimeout(remaining);

        read = Serial2.readBytes(&ch, 1);
        if (read < 0) {
          Serial.println("[ERROR] Failed reading status SLIP escape");
          return ERR_FAIL;
        } else if (read < 1) {
          Serial.println("[ERROR] Timeout reading status SLIP escape");
          return ERR_TIMEOUT;
        }

        if (ch == 0xDC) {
          status[i] = 0xC0;
        } else if (ch == 0xDD) {
          status[i] = 0xDB;
        } else {
          Serial.println("[ERROR] Invalid status SLIP escape sequence");
          return ERR_INVALID_RESP;
        }
      } else {
        status[i] = ch;
      }
    }

    // Step 5: Wait for end delimiter
    do {
      int32_t remaining = (s_time_end - millis());
      if (remaining <= 0) {
        remaining = 0;
      }
      Serial2.setTimeout(remaining);

      int read = Serial2.readBytes(&ch, 1);
      if (read < 0) {
        Serial.println("[ERROR] Failed reading end delimiter");
        return ERR_FAIL;
      } else if (read < 1) {
        Serial.println("[ERROR] Timeout waiting for end delimiter");
        return ERR_TIMEOUT;
      }
    } while (ch != DELIMITER);

    // Step 6: Verify response matches expected command
    if (buff[0] == READ_DIRECTION && buff[1] == command) {
      // Check status
      if (status[0]) {  // If failed flag is set
        Serial.println("[ERROR] Command failed with status:");
        switch (status[1]) {  // error code
          case INVALID_CRC:
            Serial.println("  INVALID_CRC: Checksum verification failed");
            break;
          case INVALID_COMMAND:
            Serial.println("  INVALID_COMMAND: Command or parameters invalid");
            break;
          case COMMAND_FAILED:
            Serial.println("  COMMAND_FAILED: Failed to execute command");
            break;
          case FLASH_WRITE_ERR:
            Serial.println("  FLASH_WRITE_ERR: Flash write verification failed");
            break;
          case FLASH_READ_ERR:
            Serial.println("  FLASH_READ_ERR: Flash read operation failed");
            break;
          case READ_LENGTH_ERR:
            Serial.println("  READ_LENGTH_ERR: Read length exceeds limit");
            break;
          case DEFLATE_ERROR:
            Serial.println("  DEFLATE_ERROR: Decompression error");
            break;
          default:
            Serial.printf("  UNKNOWN ERROR: Code 0x%02X\n", status[1]);
            break;
        }
        return ERR_INVALID_RESP;
      }
      // Command response verified successfully
      return SUCCESS;
    }
    Serial.println("[DEBUG] Response didn't match expected command, continuing...");
  } while (1);
}

/**
   Send flash begin command to ESP32
   @param offset: Flash offset address
   @param erase_size: Size of region to erase
   @param block_size: Size of each block
   @param blocks_to_write: Number of blocks to write
   Returns: SUCCESS or error code
*/
int ESP32Flasher::flashBeginCmd(uint32_t offset,
                                uint32_t erase_size,
                                uint32_t block_size,
                                uint32_t blocks_to_write) {
  Serial.println("[INFO] ========== Sending Flash Begin Command ==========");

  // Command packet structure (24 bytes total)
  uint8_t cmd_data[24] = {0};

  // Fill header (8 bytes)
  cmd_data[0] = WRITE_DIRECTION;    // direction
  cmd_data[1] = FLASH_BEGIN;        // command
  cmd_data[2] = 0x10;               // size (16 bytes payload)
  cmd_data[3] = 0x00;               // size high byte
  cmd_data[4] = 0x00;               // checksum placeholder
  cmd_data[5] = 0x00;               // checksum placeholder
  cmd_data[6] = 0x00;               // checksum placeholder
  cmd_data[7] = 0x00;               // checksum placeholder

  // Fill payload (16 bytes)
  memcpy(&cmd_data[8], &erase_size, 4);       // erase_size
  memcpy(&cmd_data[12], &blocks_to_write, 4); // packet_count
  memcpy(&cmd_data[16], &block_size, 4);      // packet_size
  memcpy(&cmd_data[20], &offset, 4);          // offset

  // Reset sequence number for new flash operation
  s_sequence_number = 0;

  //  Serial.println("[DEBUG] Flash begin parameters:");
  //  Serial.printf("  - Offset: 0x%X\n", offset);
  //  Serial.printf("  - Erase size: %d bytes\n", erase_size);
  //  Serial.printf("  - Block size: %d bytes\n", block_size);
  //  Serial.printf("  - Blocks to write: %d\n", blocks_to_write);

  // Send start delimiter
  Serial2.write(DELIMITER);

  // Send command data with SLIP encoding
  for (uint32_t i = 0; i < 24; i++) {
    if (cmd_data[i] == 0xC0) {
      Serial2.write((const char *)C0_REPLACEMENT, 2);
    } else if (cmd_data[i] == 0xDB) {
      Serial2.write((const char *)DB_REPLACEMENT, 2);
    } else {
      Serial2.write((const char *)&cmd_data[i], 1);
    }
  }

  // Send end delimiter
  Serial2.write(DELIMITER);

  // Verify response
  int response = verifyResponse(FLASH_BEGIN);
  if (response == SUCCESS) {
    Serial.println("[INFO] Flash begin command accepted");
  } else {
    Serial.printf("[ERROR] Flash begin command failed with error: %d\n", response);
  }

  Serial.println("================================================\n");
  return response;
}

/**
   Send flash data command with payload to ESP32
   @param data: Pointer to data buffer
   @param size: Size of data to flash
   Returns: SUCCESS or error code
*/
int ESP32Flasher::flashDataCmd(const uint8_t *data, uint32_t size) {

  // Command header (24 bytes total)
  uint8_t cmd_data[24] = {0};

  // Fill header (4 bytes)
  cmd_data[0] = WRITE_DIRECTION;  // Direction
  cmd_data[1] = FLASH_DATA;       // Command
  cmd_data[2] = 0x10;             // Size low byte
  cmd_data[3] = 0x04;             // Size high byte

  // Calculate and fill checksum (4 bytes)
  uint8_t checksum = compute_checksum(data, size);
  cmd_data[4] = checksum;
  cmd_data[5] = 0x00;
  cmd_data[6] = 0x00;
  cmd_data[7] = 0x00;

  // Fill data size (4 bytes)
  cmd_data[8] = 0x00;
  cmd_data[9] = 0x04;
  cmd_data[10] = 0x00;
  cmd_data[11] = 0x00;

  // Fill sequence number (4 bytes)
  cmd_data[12] = s_sequence_number & 0xFF;
  cmd_data[13] = (s_sequence_number >> 8) & 0xFF;
  cmd_data[14] = 0x00;
  cmd_data[15] = 0x00;

  // Increment sequence number for next packet
  s_sequence_number++;

  // Send start delimiter
  Serial2.write(DELIMITER);

  // Send header with SLIP encoding
  for (uint32_t i = 0; i < 24; i++) {
    if (cmd_data[i] == 0xC0) {
      Serial2.write((const char *)C0_REPLACEMENT, 2);
    } else if (cmd_data[i] == 0xDB) {
      Serial2.write((const char *)DB_REPLACEMENT, 2);
    } else {
      Serial2.write((const char *)&cmd_data[i], 1);
    }
  }

  // Send data with SLIP encoding
  for (uint32_t i = 0; i < size; i++) {
    if (data[i] == 0xC0) {
      Serial2.write((const char *)C0_REPLACEMENT, 2);
    } else if (data[i] == 0xDB) {
      Serial2.write((const char *)DB_REPLACEMENT, 2);
    } else {
      Serial2.write((const char *)&data[i], 1);
    }
  }

  // Send end delimiter
  Serial2.write(DELIMITER);

  // Verify response
  int response = verifyResponse(FLASH_DATA);
  if (response != SUCCESS) {
    Serial.printf("[ERROR] Flash data command failed with error: %d\n", response);
  }
  
  return response;
}

/**
   Handle ESP32 synchronization
   Returns: SUCCESS or error code
*/
int ESP32Flasher::espSyncHandle(void) {
  Serial.println("\n[INFO] ========== Starting ESP32 Sync ==========");

  // Command structure (40 bytes total: 4 header + 36 sync sequence)
  uint8_t cmd_data[40] = {0};

  // Fill header
  cmd_data[0] = WRITE_DIRECTION;
  cmd_data[1] = SYNC;
  cmd_data[2] = 36;  // Size of sync sequence
  cmd_data[3] = 0;   // Checksum

  // Fill sync sequence
  cmd_data[4] = 0x07;
  cmd_data[5] = 0x07;
  cmd_data[6] = 0x12;
  cmd_data[7] = 0x20;

  // Fill remaining bytes with 0x55 (sync pattern)
  for (int i = 8; i < 40; i++) {
    cmd_data[i] = 0x55;
  }

  Serial.println("[DEBUG] Sending sync sequence...");

  // Send start delimiter
  Serial2.write(DELIMITER);

  // Send sync command with SLIP encoding
  for (uint32_t i = 0; i < 40; i++) {
    if (cmd_data[i] == 0xC0) {
      Serial2.write((const char *)C0_REPLACEMENT, 2);
    } else if (cmd_data[i] == 0xDB) {
      Serial2.write((const char *)DB_REPLACEMENT, 2);
    } else {
      Serial2.write((const char *)&cmd_data[i], 1);
    }
  }

  // Send end delimiter
  Serial2.write(DELIMITER);

  // Verify response
  int response = verifyResponse(SYNC);
  if (response == SUCCESS) {
    Serial.println("[INFO] Sync successful");
  } else {
    Serial.printf("[ERROR] Sync failed with error: %d\n", response);
  }

  Serial.println("================================================\n");
  return response;
}



/**
   Attach SPI flash
   @param config: SPI configuration
   Returns: SUCCESS or error code
*/
int ESP32Flasher::spiAttachCmd(uint32_t config) {
  Serial.println("\n[INFO] ========== Attaching SPI Flash ==========");

  // Command structure (12 bytes total)
  uint8_t cmd_data[12] = {0};

  // Fill command data
  cmd_data[0] = WRITE_DIRECTION;
  cmd_data[1] = SPI_ATTACH;
  cmd_data[2] = 8;  // Size of payload (config + zeros)
  cmd_data[3] = 0;  // Checksum

  // Copy configuration
  memcpy(&cmd_data[4], &config, 4);
  // Last 4 bytes remain zero

  // Send start delimiter
  Serial2.write(DELIMITER);

  // Send command with SLIP encoding
  for (uint32_t i = 0; i < 12; i++) {
    if (cmd_data[i] == 0xC0) {
      Serial2.write((const char *)C0_REPLACEMENT, 2);
    } else if (cmd_data[i] == 0xDB) {
      Serial2.write((const char *)DB_REPLACEMENT, 2);
    } else {
      Serial2.write((const char *)&cmd_data[i], 1);
    }
  }

  // Send end delimiter
  Serial2.write(DELIMITER);

  // Verify response
  int response = verifyResponse(SPI_ATTACH);
  if (response == SUCCESS) {
    Serial.println("[INFO] SPI flash attached successfully");
  } else {
    Serial.printf("[ERROR] SPI flash attachment failed with error: %d\n", response);
  }

  Serial.println("================================================\n");
  return response;
}

/**
   Establish connection with ESP32 and prepare for flashing
   Returns: true if connection successful, false otherwise
*/
bool ESP32Flasher::espConnect(void) {
  Serial.println("\n[INFO] ========== Starting ESP32 Connection ==========");
  int32_t trials = MAX_TRIALS;
  int get_sync_status;

  // Put ESP32 into download mode sequence
  Serial.println("[DEBUG] Initiating download mode sequence...");

  // Step 1: Assert BOOT pin
  digitalWrite(BOOT_PIN, LOW);
  Serial.println("[DEBUG] BOOT pin set LOW - enabling download mode");
  delay(50);

  // Step 2: Reset sequence
  digitalWrite(EN_PIN, LOW);
  Serial.println("[DEBUG] EN pin set LOW - starting reset");
  delay(100);
  digitalWrite(EN_PIN, HIGH);
  Serial.println("[DEBUG] EN pin set HIGH - completing reset");
  delay(50);

  // Step 3: Release BOOT pin
  digitalWrite(BOOT_PIN, HIGH);
  Serial.println("[DEBUG] BOOT pin set HIGH - ready for sync");

  // Attempt synchronization with timeout and retry
  do {
    Serial.printf("[INFO] Sync attempt %d of %d\n", (MAX_TRIALS - trials + 1), MAX_TRIALS);
    s_time_end = millis() + SYNC_TIMEOUT;

    get_sync_status = espSyncHandle();
    if (get_sync_status == ERR_TIMEOUT) {
      Serial.printf("[WARN] Sync timeout on attempt %d\n", (MAX_TRIALS - trials + 1));
      if (--trials == 0) {
        Serial.println("[ERROR] All sync attempts failed - connection failed");
        return ERR_TIMEOUT;
      }
      delay(100);
    } else if (get_sync_status != SUCCESS) {
      Serial.printf("[ERROR] Sync failed with error code: %d\n", get_sync_status);
      return get_sync_status;
    }
  } while (get_sync_status != SUCCESS);

  Serial.println("[INFO] ESP32 sync successful!");

  // Attach SPI flash
  Serial.println("[DEBUG] Attaching SPI flash...");
  s_time_end = millis() + DEFAULT_TIMEOUT;
  get_sync_status = spiAttachCmd(0);

  if (get_sync_status == SUCCESS) {
    Serial.println("[INFO] SPI flash attached successfully");
    Serial.println("============================================\n");
    return SUCCESS;
  } else {
    Serial.printf("[ERROR] SPI flash attachment failed with code: %d\n", get_sync_status);
    Serial.println("============================================\n");
    return get_sync_status;
  }
}

/**
   Initialize flash process
   @param flash_address: Starting address for flash operation
   @param image_size: Size of image to flash
   @param block_size: Size of each block to write
   Returns: SUCCESS or error code
*/
int ESP32Flasher::espFlashStart(uint32_t flash_address, uint32_t image_size, uint32_t block_size) {
  Serial.println("[INFO] ========== Initializing Flash Process ==========");

  // Validate image size
  if (image_size > ESP_FLASH_MAX_SIZE) {
    Serial.printf("[ERROR] Image size %d exceeds maximum flash size %d\n",
                  image_size, ESP_FLASH_MAX_SIZE);
    return ERR_IMG_SIZE;
  }

  // Calculate number of blocks and erase size
  uint32_t blocks_to_write = (image_size + block_size - 1) / block_size;
  uint32_t erase_size = block_size * blocks_to_write;
  s_flash_write_size = block_size;

  // Calculate timeout based on erase size
  uint32_t timeout = timeout_per_mb(erase_size, ERASE_REGION_TIMEOUT_PER_MB);
  s_time_end = millis() + timeout;
  
  Serial.printf("[DEBUG] Flash timeout set to: %d ms\n", timeout);
  
  // Initialize flash operation
  int result = flashBeginCmd(flash_address, erase_size, block_size, blocks_to_write);

  if (result == SUCCESS) {
    Serial.println("[INFO] Flash initialization successful");
  } else {
    Serial.printf("[ERROR] Flash initialization failed with error: %d\n", result);
  }

  Serial.println("================================================\n");
  return result;
}
/**
   Write data to ESP32 flash
   @param payload: Data to write
   @param size: Size of data
   Returns: SUCCESS or error code
*/
int ESP32Flasher::espFlashWrite(void *payload, uint32_t size) {

  // Calculate padding required
  uint32_t padding_bytes = s_flash_write_size - size;
  uint8_t *data = (uint8_t *)payload;
  uint32_t padding_index = size;

  // Add padding if necessary
  if (padding_bytes > 0) {
    while (padding_bytes--) {
      data[padding_index++] = PADDING_PATTERN;
    }
  }

  // Set timeout for operation
  s_time_end = millis() + DEFAULT_TIMEOUT;

  // Write data to flash
  int result = flashDataCmd(data, s_flash_write_size);
  if (result == SUCCESS) {
  } else {
  }
  return result;
}

/**
   Initialize the ESP32 Flasher
   - Sets up serial communication
   - Configures control pins
   - Prepares for flashing operations
*/
void ESP32Flasher::espFlasherInit(void) {
  Serial.println("\n[INFO] ========== ESP32 Flasher Initialization ==========");

  // Initialize serial communication with ESP32
  Serial2.begin(115200);
  Serial.println("[DEBUG] Serial2 communication initialized at 115200 baud");

  // Configure control pins
  pinMode(BOOT_PIN, OUTPUT);  // Boot mode control
  pinMode(EN_PIN, OUTPUT);    // Reset control

  Serial.println("[DEBUG] Control pins configured:");
  Serial.printf("  - BOOT_PIN: %d\n", BOOT_PIN);
  Serial.printf("  - EN_PIN: %d\n", EN_PIN);

  Serial.println("[INFO] ESP32 Flasher initialization complete");
  Serial.println("================================================\n");
}

/**
   Flash a binary stream to the ESP32
   @param myFile: Name of data stream
*/
void ESP32Flasher::espFlashBinStream(Stream &myFile, uint32_t size)  // Flash binary file
{
  Serial.println("\n[INFO] ========== Starting Binary Stream Flash Process ==========");
  Serial.println("[WARN] Do not interrupt the flashing process!");
  Serial.printf("[INFO] Attempting to flash stream");

  flashBinaryStream(myFile, size, ESP_FLASH_OFFSET);

  // Reset ESP32
  Serial.println("[INFO] Resetting ESP32...");
  digitalWrite(EN_PIN, LOW);
  delay(100);
  digitalWrite(EN_PIN, HIGH);

  Serial.println("================================================\n");
}

/**
   Flash a binary file to the ESP32
   @param bin_file_name: Name of binary file in SPIFFS
*/
/*void ESP32Flasher::espFlashBinFile(const char* bin_file_name) {
  Serial.println("\n[INFO] ========== Starting Binary File Flash Process ==========");
  Serial.println("[WARN] Do not interrupt the flashing process!");
  Serial.printf("[INFO] Attempting to flash file: %s\n", bin_file_name);

  if (SPIFFS.exists(bin_file_name)) {
    File file_read = SPIFFS.open(bin_file_name, FILE_READ);
    size_t size = file_read.size();

    Serial.printf("[INFO] File found, size: %d bytes\n", size);

    if (size <= ESP_FLASH_MAX_SIZE) {
      Serial.println("[INFO] File size within valid range");
      flashBinary(file_read, size, ESP_FLASH_OFFSET);
    } else {
      Serial.printf("[ERROR] File size %d exceeds maximum flash size %d\n",
                    size, ESP_FLASH_MAX_SIZE);
    }
    file_read.close();
  } else {
    Serial.printf("[ERROR] File %s not found in SPIFFS\n", bin_file_name);
  }

  // Reset ESP32
  Serial.println("[INFO] Resetting ESP32...");
  digitalWrite(EN_PIN, LOW);
  delay(100);
  digitalWrite(EN_PIN, HIGH);

  Serial.println("================================================\n");
}*/

/**
   Flash a binary Stream to ESP32
   @param file: Reference to the binary file to flash
   @param address: Flash address to write to
   Returns: SUCCESS or error code
*/
int ESP32Flasher::flashBinaryStream(Stream &myFile, uint32_t size, uint32_t address)
{
  //uint8_t payload[256];  // Buffer for flash data chunks
  uint8_t payload[1024] = { 0 };  // Buffer for flash data chunks
 // Serial.println("\n[INFO] ========== Starting Binary Flash Process ==========");
 // Serial.printf("[INFO] File size: %d bytes\n", size);
  //Serial.printf("[INFO] Flash address: 0x%X\n", address);

  // Step 1: Initialize flash process
  //Serial.println("[INFO] Erasing flash (this may take a while)...");
  int flash_start_status = espFlashStart(address, size, sizeof(payload));
  if (flash_start_status != SUCCESS) {
     Serial.printf("[ERROR] Flash erase failed with error: %d\n", flash_start_status);
    return flash_start_status;
  }

  // Step 2: Program flash
 // Serial.println("[INFO] Starting programming sequence");
  //size_t binary_size = size;
  //size_t written = 0;
  //int previousProgress = -1;

  uint32_t _undownloadByte = size;

  // Write data in chunks
  while (_undownloadByte > 0) {

    size = myFile.available();
    if(size) {
      // read up to 2048 byte into the buffer
			int c = myFile.readBytes(payload, ((size > sizeof(payload)) ? sizeof(payload) : size));


    // Calculate chunk size
    //   size_t to_read = MIN(size, sizeof(payload));

    // Read chunk from file
    //   myFile.readBytes(payload, to_read);

    // Write chunk to flash
      flash_start_status = espFlashWrite(payload, c);
      if (flash_start_status != SUCCESS) {
       // Serial.printf("[ERROR] Flash write failed at offset 0x%X with error: %d\n",
        //            written, flash_start_status);
        return flash_start_status;
      }
    // Update progress
    //size -= to_read;
    //written += to_read;

    // Display progress
    //int progress = (int)(((float)written / binary_size) * 100);
    //if (previousProgress != progress) {
    // previousProgress = progress;
       //Serial.printf("[INFO] Programming progress: %d%% (%d/%d)\n", progress,written,binary_size);  
      //Callback function called at every new percent done
      if(_updateProgressCallback) _updateProgressCallback();

      _undownloadByte -= c;
    }
    delay(1);
  }

//  Serial.println("[INFO] Programming complete!");
//  Serial.println("================================================\n");
  return SUCCESS;
}

/**
   Flash a binary file to ESP32
   @param file: Reference to the binary file to flash
   @param size: Size of the binary file
   @param address: Flash address to write to
   Returns: SUCCESS or error code
*/
/*
int ESP32Flasher::flashBinary(File& file, uint32_t size, uint32_t address) {
  uint8_t payload[1024];  // Buffer for flash data chunks

  Serial.println("\n[INFO] ========== Starting Binary Flash Process ==========");
  Serial.printf("[INFO] File size: %d bytes\n", size);
  Serial.printf("[INFO] Flash address: 0x%X\n", address);

  // Step 1: Initialize flash process
  Serial.println("[INFO] Erasing flash (this may take a while)...");
  int flash_start_status = espFlashStart(address, size, sizeof(payload));
  if (flash_start_status != SUCCESS) {
    Serial.printf("[ERROR] Flash erase failed with error: %d\n", flash_start_status);
    return flash_start_status;
  }

  // Step 2: Program flash
  Serial.println("[INFO] Starting programming sequence");
  size_t binary_size = size;
  size_t written = 0;
  int previousProgress = -1;

  // Write data in chunks
  while (size > 0) {
    // Calculate chunk size
    size_t to_read = MIN(size, sizeof(payload));

    // Read chunk from file
    file.read(payload, to_read);

    // Write chunk to flash
    flash_start_status = espFlashWrite(payload, to_read);
    if (flash_start_status != SUCCESS) {
      Serial.printf("[ERROR] Flash write failed at offset 0x%X with error: %d\n",
                    written, flash_start_status);
      return flash_start_status;
    }

    // Update progress
    size -= to_read;
    written += to_read;

    // Display progress
    int progress = (int)(((float)written / binary_size) * 100);
    if (previousProgress != progress) {
      previousProgress = progress;
      Serial.printf("[INFO] Programming progress: %d%%\n", progress);
    }
  }

  Serial.println("[INFO] Programming complete!");
  Serial.println("================================================\n");
  return SUCCESS;
}*/

int ESP32Flasher::epsFlashFinish(bool reboot)
{
  s_time_end = millis() + DEFAULT_TIMEOUT;

  return flashEndCmd(!reboot);
}