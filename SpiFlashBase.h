#ifndef ___SPI_FLASH_BASE__H___
#define ___SPI_FLASH_BASE__H___


#include <stdint.h>
#include <Arduino.h>
#include <CArrayDefs.h>
#include <SoftSpi.h>
#include <Spi.h>

/*
 * # Standard SPI Instructions #
 *
 * Adapted from "6.2.2 Instruction Set Table 1" in [`w25q64v` datasheet][1].
 *
 *     |----------------------------|---------|--------------|-------------|-----------|-----------|------------|
 *     | CLOCK NUMBER               | (0 – 7) | (8 – 15)     | (16 – 23)   | (24 – 31) | (32 – 39) | (40 – 47)  |
 *     |----------------------------|---------|--------------|-------------|-----------|-----------|------------|
 *     | INSTRUCTION NAME           | BYTE 1  | BYTE 2       | BYTE 3      | BYTE 4    | BYTE 5    | BYTE 6     |
 *     |----------------------------|---------|--------------|-------------|-----------|-----------|------------|
 *     | Write Enable               | 06h     |              |             |           |           |            |
 *     | Volatile SR Write Enable   | 50h     |              |             |           |           |            |
 *     | Write Disable              | 04h     |              |             |           |           |            |
 *     | Read Status Register-1     | 05h     | (S7-S0)      |             |           |           |            |
 *     | Read Status Register-2     | 35h     | (S15-S8)     |             |           |           |            |
 *     | Write Status Register      | 01h     | (S7-S0)      | (S15-S8)    |           |           |            |
 *     | Page Program               | 02h     | A23-A16      | A15-A8      | A7-A0     | D7-D0     | D7-D0      |
 *     | Sector Erase (4KB)         | 20h     | A23-A16      | A15-A8      | A7-A0     |           |            |
 *     | Block Erase (32KB)         | 52h     | A23-A16      | A15-A8      | A7-A0     |           |            |
 *     | Block Erase (64KB)         | D8h     | A23-A16      | A15-A8      | A7-A0     |           |            |
 *     | Chip Erase                 | C7h/60h |              |             |           |           |            |
 *     | Erase / Program Suspend    | 75h     |              |             |           |           |            |
 *     | Erase / Program Resume     | 7Ah     |              |             |           |           |            |
 *     | Power-down                 | B9h     |              |             |           |           |            |
 *     | Read Data                  | 03h     | A23-A16      | A15-A8      | A7-A0     | D7-D0     |            |
 *     | Fast Read                  | 0Bh     | A23-A16      | A15-A8      | A7-A0     | dummy     | D7-D0      |
 *     | Release Powerdown / ID     | ABh     | dummy        | dummy       | dummy     | ID7-ID0   |            |
 *     | Manufacturer/Device ID     | 90h     | dummy        | dummy       | 00h       | MF7-MF0   | ID7-ID0    |
 *     | JEDEC ID                   | 9Fh     | MF7-MF0      | ID15-ID8    | ID7-ID0   |           |            |
 *     |                            |         | Manufacturer | Memory Type | Capacity  |           |            |
 *     | Read Unique ID             | 4Bh     | dummy        | dummy       | dummy     | dummy     | UID63-UID0 |
 *     | Read SFDP Register         | 5Ah     | 00h          | 00h         | A7–A0     | dummy     | D7-0       |
 *     | Erase Security Registers   | 44h     | A23-A16      | A15-A8      | A7-A0     |           |            |
 *     | Program Security Registers | 42h     | A23-A16      | A15-A8      | A7-A0     | D7-D0     |            |
 *     | Read Security Registers    | 48h     | A23-A16      | A15-A8      | A7-A0     | dummy     | D7-D0      |
 *     | Enable QPI                 | 38h     |              |             |           |           |            |
 *     | Enable Reset               | 66h     |              |             |           |           |            |
 *     | Reset                      | 99h     |              |             |           |           |            |
 *     |----------------------------|---------|--------------|-------------|-----------|-----------|------------|
 *
 * **NOTE** Operations involving multiple reads or writes wrap at addresses
 * modulo 256.
 *
 * From the [datasheet][1]:
 *
 * > If an entire 256 byte page is to be programmed, the last address byte
 * > (the 8 least significant address bits) should be set to 0. If the last
 * > address byte is not zero, and the number of clocks exceed the
 * > remaining page length, the addressing will wrap to the beginning of
 * > the page. In some cases, less than 256 bytes (a partial page) can be
 * > programmed without having any effect on other bytes within the same
 * > page. One condition to perform a partial page program is that the
 * > number of clocks can not exceed the remaining page length. If more
 * > than 256 bytes are sent to the device the addressing will wrap to the
 * > beginning of the page and overwrite previously sent data.
 *
 *
 * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
 */

class SpiFlashBase {
protected:
  uint8_t ERROR_CODE_;

  virtual void deselect_chip();
  virtual void select_chip();
  virtual uint8_t transfer(uint8_t value) = 0;

  void set_error(uint8_t error_code) { ERROR_CODE_ = error_code; }

  bool erase(uint32_t address, uint8_t code, uint32_t settling_time_ms);
public:
  static const uint8_t SPI__DUMMY             = 0x00;

  // **TODO** Add support for the following instructions as needed.
  //static const uint8_t INSTR__ENABLE_QPI = 0x38;
  //static const uint8_t INSTR__ERASE_PROGRAM_RESUME = 0x7A;
  //static const uint8_t INSTR__ERASE_PROGRAM_SUSPEND = 0x75;
  //static const uint8_t INSTR__ERASE_SECURITY_REGISTERS = 0x44;
  //static const uint8_t INSTR__FAST_READ = 0x0B;
  //static const uint8_t INSTR__PROGRAM_SECURITY_REGISTERS = 0x42;
  //static const uint8_t INSTR__READ_SECURITY_REGISTERS = 0x48;
  //static const uint8_t INSTR__VOLATILE_SR_WRITE_ENABLE = 0x50;
  //static const uint8_t INSTR__WRITE_STATUS_REGISTER = 0x01;

  /* See "6.2.2 Instruction Set Table 1" in [datasheet][1] (or in summary
   * above).
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  static const uint8_t INSTR__CHIP_ERASE             = 0x60;
  static const uint8_t INSTR__MANUFACTURER_DEVICE_ID = 0x90;
  static const uint8_t INSTR__PAGE_PROGRAM           = 0x02;
  static const uint8_t INSTR__READ_DATA              = 0x03;
  static const uint8_t INSTR__READ_STATUS_REGISTER_1 = 0x05;
  static const uint8_t INSTR__READ_STATUS_REGISTER_2 = 0x35;
  static const uint8_t INSTR__WRITE_DISABLE          = 0x04;
  static const uint8_t INSTR__WRITE_ENABLE           = 0x06;
  static const uint8_t INSTR__JEDEC_ID               = 0x9F;
  static const uint8_t INSTR__READ_UNIQUE_ID         = 0x4B;
  static const uint8_t INSTR__READ_SFDP_REGISTER     = 0x5A;
  static const uint8_t INSTR__ENABLE_RESET           = 0x66;
  static const uint8_t INSTR__RESET                  = 0x99;
  static const uint8_t INSTR__SECTOR_ERASE_4KB_      = 0x20;
  static const uint8_t INSTR__BLOCK_ERASE_32KB_      = 0x52;
  static const uint8_t INSTR__BLOCK_ERASE_64KB_      = 0xD8;
  static const uint8_t INSTR__POWER_DOWN             = 0xB9;
  static const uint8_t INSTR__RELEASE_POWERDOWN_ID   = 0xAB;

  /* See "Figure 4a. Status Register-1" in [datasheet][1].
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  static const uint8_t STATUS__BUSY         = 0b00000001;
  static const uint8_t STATUS__WRITE_ENABLE = 0b00000010;

  uint8_t cs_pin_;  // Chip select pin should connect to `/CS` pin on chip
  uint8_t device_id_;
  uint8_t manufacturer_id_;

  static const uint8_t TIMEOUT_ERROR = 0x10;

  bool disable_write();
  bool enable_write();

  SpiFlashBase() : ERROR_CODE_(0), cs_pin_(0), device_id_(0),
                   manufacturer_id_(0) {}
  SpiFlashBase(uint8_t cs_pin) : ERROR_CODE_(0), cs_pin_(cs_pin),
                                 device_id_(0), manufacturer_id_(0) {}

  virtual void begin();
  virtual void begin(uint8_t cs_pin);

  uint8_t error_code() const { return ERROR_CODE_; }
  void clear_error() { set_error(0); }

  uint8_t status_register1();
  uint8_t status_register2();

  bool ready();
  bool ready_wait(uint32_t timeout=100L);

  bool read(uint32_t address, uint8_t *dst, uint32_t length);
  UInt8Array read(uint32_t address, UInt8Array dst);
  uint8_t read(uint32_t address);  // Read single byte
  bool erase_chip();
  bool write_page(uint32_t address, uint8_t *src, uint32_t length);
  bool write_page(uint32_t address, UInt8Array src);

  uint32_t jedec_id();
  uint64_t read_unique_id();
  uint8_t read_sfdp_register(uint8_t address);
  bool erase_sector(uint32_t address);
  bool erase_block_32KB(uint32_t address);
  bool erase_block_64KB(uint32_t address);
  void power_down();
  void reset();

  // Return from standby mode (i.e., restore after call to `power_down()`).
  void release_powerdown();

  /* Return from standby mode and read device ID (i.e., restore after call to
   * `power_down()`). */
  uint8_t release_powerdown_id();
};


#endif  // #ifndef ___SPI_FLASH_BASE__H___
