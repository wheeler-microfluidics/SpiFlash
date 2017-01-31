#ifndef ___SPI_FLASH__H___
#define ___SPI_FLASH__H___


#include <stdint.h>
#include <Arduino.h>
//#include <SPI.h>
#include <SoftSpi.h>
#include <CArrayDefs.h>

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

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode = 0>
class SpiFlashBase {
protected:
  uint8_t ERROR_CODE_;
  typedef SoftSPI<MisoPin, MosiPin, SckPin, Mode> SoftSPI_t;
  SoftSPI_t soft_spi_;

  void deselect_chip();
  void select_chip();
  void set_error(uint8_t error_code) { ERROR_CODE_ = error_code; }
public:
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

  void begin();
  void begin(uint8_t cs_pin);

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
};


template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
void SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::deselect_chip() {
  digitalWrite(cs_pin_, HIGH);
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
void SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::select_chip() {
  digitalWrite(cs_pin_, LOW);
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
void SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::begin() {
  pinMode(cs_pin_, OUTPUT);
  soft_spi_.begin();

  select_chip();
  soft_spi_.transfer(INSTR__MANUFACTURER_DEVICE_ID);
  for (int i = 0; i < 3; i++) { soft_spi_.transfer(0); }
  manufacturer_id_ = soft_spi_.transfer(0);
  device_id_ = soft_spi_.transfer(0);
  deselect_chip();
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
void SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::begin(uint8_t cs_pin) {
  cs_pin_ = cs_pin;
  begin();
}

/*
 * # Read status register 1 #
 *
 *  1. Select chip
 *  2. Send `Read Status Register-1`:
 *      * Shift out: `[0x05]`
 *      * Shift out `[0xXX]`, shift in `status`
 *  3. Deselect chip
 */
template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
uint8_t SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::status_register1() {
  select_chip();
  soft_spi_.transfer(INSTR__READ_STATUS_REGISTER_1);
  uint8_t status = soft_spi_.transfer(0);
  deselect_chip();
  return status;
}

/*
 * # Read status register 2 #
 *
 *  1. Select chip
 *  2. Send `Read Status Register-2`:
 *      * Shift out: `[0x35]`
 *      * Shift out `[0xXX]`, shift in `status`
 *  3. Deselect chip
 */
template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
uint8_t SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::status_register2() {
  select_chip();
  soft_spi_.transfer(INSTR__READ_STATUS_REGISTER_2);
  uint8_t status = soft_spi_.transfer(0);
  deselect_chip();
  return status;
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
bool SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::ready() {
  return !(status_register1() & STATUS__BUSY);
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
bool SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>
::ready_wait(uint32_t timeout) {
  uint32_t start = millis();

  while (!ready()) {
    if ((millis() - start) > timeout) {
      set_error(TIMEOUT_ERROR);
      return false;
    }
  }
  return true;
}

/*
 * # Read #
 *
 */
template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
bool SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>
::read(uint32_t address, uint8_t *dst, uint32_t length) {
  //  1. Check that device is ready (see "Wait for ready")
  if (!ready_wait()) { return false; }

  //  2. Select chip
  select_chip();
  /*  3. Send `Read Data`
   *      * Shift out: `[0x03]` */
  soft_spi_.transfer(INSTR__READ_DATA);
  //      * Shift out: `[A23-A16][A15-A8][A7-A0]`
  soft_spi_.transfer(address >> (2 * 8));  // A23-A16
  soft_spi_.transfer(address >> (1 * 8));  // A15-A8
  soft_spi_.transfer(address);  // A7-A0
  //  4. Shift out `[0xXX]`, shift in value
  //  5. Repeat 4 to read bytes as needed.
  for (uint32_t i = 0; i < length; i++) {
    dst[i] = soft_spi_.transfer(0);
  }
  //  6. Deselect chip
  deselect_chip();
  clear_error();
  return true;
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
UInt8Array SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>
::read(uint32_t address, UInt8Array dst) {
  if (!read(address, dst.data, dst.length)) {
    dst.data = NULL;
    dst.length = 0;
  }
  return dst;
}

// Read single byte from address.
template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
uint8_t SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::read(uint32_t address) {
  uint8_t value = 0;
  read(address, &value, 1);
  return value;
}

/*
 * # Write enable #
 */
template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
bool SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::enable_write() {
  select_chip();
  soft_spi_.transfer(INSTR__WRITE_ENABLE);
  deselect_chip();
  // Verify expected state of write enable bit in status register.
  return status_register1() & STATUS__WRITE_ENABLE;
}

/*
 * # Write disable #
 */
template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
bool SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::disable_write() {
  select_chip();
  soft_spi_.transfer(INSTR__WRITE_DISABLE);
  deselect_chip();
  // Verify expected state of write enable bit in status register.
  return !(status_register1() & STATUS__WRITE_ENABLE);
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
bool SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>::erase_chip() {
  /* 1. Check that:
   *      - Device is ready (see "Wait for ready")
   *      - Write is enabled (see "Write enable") */
  if (!ready_wait() || !enable_write()) { return false; }

  select_chip();
  //  2. Send `Chip erase`
  soft_spi_.transfer(INSTR__CHIP_ERASE);
  deselect_chip();
  /*  3. Wait for up to 60s for erase to complete.
   *
   * Notes:
   *
   *  - `BUSY` bit in status register remains set until erase is complete.
   *  - Write enable bit in status register is cleared upon erase completion.
   */
  if (!ready_wait(60000L /* 60 seconds */)) {
    disable_write();
    return false;
  }
  return true;
}

/*
 * # Write page (i.e., up to 256 bytes) #
 */
template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
bool SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>
::write_page(uint32_t address, uint8_t *src, uint32_t length) {
  /*  1. Check that:
   *      * Device is ready (see "Wait for ready")
   *      * Write is enabled (see "Write enable") */
  if (!ready_wait() || !enable_write()) { return false; }

  //  2. Select chip
  select_chip();
  /*  3. Send `Page Program`
   *      * Shift out: `[0x02]`
   *      * Shift out: `[A23-A16][A15-A8][A7-A0]`
   */
  soft_spi_.transfer(INSTR__PAGE_PROGRAM);
  soft_spi_.transfer(address >> (2 * 8));  // A23-A16
  soft_spi_.transfer(address >> (1 * 8));  // A15-A8
  soft_spi_.transfer(address);  // A7-A0
  /*  4. Shift out `N` bytes
   *      * **NOTE** bytes will be written to:
   *
   *             [A23-A16][A15-A8][0x00] + ((i + [A7-A0]) % 256)
   *
   *      * In other words, addresses wrap modulo 256.
   *      * To write 256 contiguous bytes starting at specified address,
   *      address must be 256-byte aligned (i.e., `[A7-A0]` must be 0).
   */
  for (uint32_t i = 0; i < length; i++) {
    soft_spi_.transfer(src[i]);
  }
  //  5. Deselect chip
  deselect_chip();
  /*  6. Wait for up to 100ms for write to complete.
   *
   * Notes:
   *
   *  - `BUSY` bit in status register remains set until write is complete.
   *  - Write enable bit in status register is cleared upon write completion.
   */
  if (!ready_wait(100)) {
    disable_write();
    return false;
  }
  return true;
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
bool SpiFlashBase<MisoPin, MosiPin, SckPin, Mode>
::write_page(uint32_t address, UInt8Array src) {
  return write_page(address, src.data, src.length);
}

#endif  // #ifndef ___SPI_FLASH__H___
