#include "SpiFlash.h"

void SpiFlash::deselect_chip() {
  SPI.endTransaction();
  digitalWrite(cs_pin_, HIGH);
}
void SpiFlash::select_chip() {
  digitalWrite(cs_pin_, LOW);
  SPI.beginTransaction(spi_settings_);
}

void SpiFlash::begin() {
  pinMode(10, OUTPUT);
  pinMode(cs_pin_, OUTPUT);

  select_chip();
  SPI.transfer(INSTR__MANUFACTURER_DEVICE_ID);
  for (int i = 0; i < 3; i++) { SPI.transfer(0); }
  manufacturer_id_ = SPI.transfer(0);
  device_id_ = SPI.transfer(0);
  deselect_chip();
}

void SpiFlash::begin(uint8_t cs_pin) {
  cs_pin_ = cs_pin;
  begin();
}

void SpiFlash::set_spi_settings(SPISettings const &spi_settings) {
  spi_settings_ = spi_settings;
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
uint8_t SpiFlash::status_register1() {
  select_chip();
  SPI.transfer(INSTR__READ_STATUS_REGISTER_1);
  uint8_t status = SPI.transfer(0);
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
uint8_t SpiFlash::status_register2() {
  select_chip();
  SPI.transfer(INSTR__READ_STATUS_REGISTER_2);
  uint8_t status = SPI.transfer(0);
  deselect_chip();
  return status;
}

bool SpiFlash::ready() { return !(status_register1() & STATUS__BUSY); }

bool SpiFlash::ready_wait(uint32_t timeout) {
  uint32_t start = millis();

  while (!ready()) {
    if ((millis() - start) < timeout) {
      set_error(TIMEOUT_ERROR);
      return false;
    }
  }
  return true;
}

/*
 * # Read #
 *
 *  1. Check that device is ready (see "Wait for ready")
 *  2. Select chip
 *  3. Send `Read Data`
 *      * Shift out: `[0x03]`
 *      * Shift out: `[A23-A16][A15-A8][A7-A0]`
 *  4. Shift out `[0xXX]`, shift in value
 *  5. Repeat 4 to read bytes as needed.
 *  6. Deselect chip
 */
bool SpiFlash::read(uint32_t address, uint8_t *dst, uint32_t length) {
  if (!ready_wait()) { return false; }

  select_chip();
  SPI.transfer(INSTR__READ_DATA);
  SPI.transfer(address >> (2 * 8));  // A23-A16
  SPI.transfer(address >> (1 * 8));  // A15-A8
  SPI.transfer(address);  // A7-A0
  for (uint32_t i = 0; i < length; i++) {
    dst[i] = SPI.transfer(0);
  }
  deselect_chip();
  clear_error();
  return true;
}

UInt8Array SpiFlash::read(uint32_t address, UInt8Array dst) {
  if (!read(address, dst.data, dst.length)) {
    dst.data = NULL;
    dst.length = 0;
  }
  return dst;
}

uint8_t SpiFlash::read(uint32_t address) {
  uint8_t value = 0;
  read(address, &value, 1);
  return value;
}

/*
 * # Write enable #
 *
 *  1. Select chip
 *  2. Send `Write Enable`
 *      * Shift out: `[0x06]`
 *  3. Deselect chip
 *  4. Select chip
 *  5. Send `Read Status Register-1`:
 *      * Shift out: `[0x05]`
 *      * Shift out `[0xXX]`, shift in `status`
 *  6. Deselect chip
 *  7. Check `status & STATUS__WRITE_ENABLE`
 */
bool SpiFlash::enable_write() {
  select_chip();
  SPI.transfer(INSTR__WRITE_ENABLE);
  deselect_chip();
  return status_register1() & STATUS__WRITE_ENABLE;
}

/*
 * # Write disable #
 *
 *  1. Select chip
 *  2. Send `Write Disable`
 *      * Shift out: `[0x04]`
 *  3. Deselect chip
 */
bool SpiFlash::disable_write() {
  select_chip();
  SPI.transfer(INSTR__WRITE_DISABLE);
  deselect_chip();
  return !(status_register1() & STATUS__WRITE_ENABLE);
}

/*
 * # Erase chip #
 *
 *  1. Check that:
 *      * Device is ready (see "Wait for ready")
 *      * Write is enabled (see "Write enable")
 *  2. Select chip
 *  3. Send `Chip erase`
 *      * Shift out: `[0x60]`
 *  4. Deselect chip
 *  5. Wait for up to 10s:
 *
 *     > Datasheet says 6s max
 *
 *  6. Disable write (see "Write disable")
 */
bool SpiFlash::erase_chip() {
  if (!ready_wait() || !enable_write()) { return false; }

  select_chip();
  SPI.transfer(INSTR__CHIP_ERASE);
  deselect_chip();
  if (!ready_wait(10000L /* 10 seconds */)) { return false; }
  disable_write();
  return true;
}

/*
 * # Write page (i.e., up to 256 bytes) #
 *
 *  1. Check that:
 *      * Device is ready (see "Wait for ready")
 *      * Write is enabled (see "Write enable")
 *  2. Select chip
 *  3. Send `Page Program`
 *      * Shift out: `[0x02]`
 *      * Shift out: `[A23-A16][A15-A8][A7-A0]`
 *  4. Shift out `N` bytes
 *      * **NOTE** bytes will be written to:
 *
 *             [A23-A16][A15-A8][0x00] + ((i + [A7-A0]) % 256)
 *
 *      * In other words, addresses wrap modulo 256.
 *      * To write 256 contiguous bytes starting at specified address, address must be 256-byte aligned (i.e., `[A7-A0]` must be 0).
 *  5. Deselect chip
 *  6. Wait for 3ms:
 *
 *     > Max page program time according to datasheet
 *
 *  7. Disable write (see "Write disable")
 */
bool SpiFlash::write_page(uint32_t address, uint8_t *src, uint32_t length) {
  if (!ready_wait() || !enable_write()) { return false; }

  select_chip();
  SPI.transfer(INSTR__PAGE_PROGRAM);
  SPI.transfer(address >> (2 * 8));  // A23-A16
  SPI.transfer(address >> (1 * 8));  // A15-A8
  SPI.transfer(address);  // A7-A0
  for (uint32_t i = 0; i < length; i++) {
    SPI.transfer(src[i]);
  }
  deselect_chip();
  delay(3);  // Wait 3ms
  disable_write();
  return true;
}

bool SpiFlash::write_page(uint32_t address, UInt8Array src) {
  return write_page(address, src.data, src.length);
}
