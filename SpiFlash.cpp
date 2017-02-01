#include "SpiFlash.h"


void SpiFlashBase::deselect_chip() {
  digitalWrite(cs_pin_, HIGH);
}

void SpiFlashBase::select_chip() {
  digitalWrite(cs_pin_, LOW);
}

void SpiFlashBase::begin() {
  pinMode(cs_pin_, OUTPUT);

  select_chip();
  transfer(INSTR__MANUFACTURER_DEVICE_ID);
  for (int i = 0; i < 3; i++) { transfer(0); }
  manufacturer_id_ = transfer(0);
  device_id_ = transfer(0);
  deselect_chip();
}

void SpiFlashBase::begin(uint8_t cs_pin) {
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
uint8_t SpiFlashBase::status_register1() {
  select_chip();
  transfer(INSTR__READ_STATUS_REGISTER_1);
  uint8_t status = transfer(0);
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
uint8_t SpiFlashBase::status_register2() {
  select_chip();
  transfer(INSTR__READ_STATUS_REGISTER_2);
  uint8_t status = transfer(0);
  deselect_chip();
  return status;
}

bool SpiFlashBase::ready() {
  return !(status_register1() & STATUS__BUSY);
}

bool SpiFlashBase::ready_wait(uint32_t timeout) {
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
bool SpiFlashBase::read(uint32_t address, uint8_t *dst, uint32_t length) {
  //  1. Check that device is ready (see "Wait for ready")
  if (!ready_wait()) { return false; }

  //  2. Select chip
  select_chip();
  /*  3. Send `Read Data`
   *      * Shift out: `[0x03]` */
  transfer(INSTR__READ_DATA);
  //      * Shift out: `[A23-A16][A15-A8][A7-A0]`
  transfer(address >> (2 * 8));  // A23-A16
  transfer(address >> (1 * 8));  // A15-A8
  transfer(address);  // A7-A0
  //  4. Shift out `[0xXX]`, shift in value
  //  5. Repeat 4 to read bytes as needed.
  for (uint32_t i = 0; i < length; i++) {
    dst[i] = transfer(0);
  }
  //  6. Deselect chip
  deselect_chip();
  clear_error();
  return true;
}

UInt8Array SpiFlashBase::read(uint32_t address, UInt8Array dst) {
  if (!read(address, dst.data, dst.length)) {
    dst.data = NULL;
    dst.length = 0;
  }
  return dst;
}

// Read single byte from address.
uint8_t SpiFlashBase::read(uint32_t address) {
  uint8_t value = 0;
  read(address, &value, 1);
  return value;
}

/*
 * # Write enable #
 */
bool SpiFlashBase::enable_write() {
  select_chip();
  transfer(INSTR__WRITE_ENABLE);
  deselect_chip();
  // Verify expected state of write enable bit in status register.
  return status_register1() & STATUS__WRITE_ENABLE;
}

/*
 * # Write disable #
 */
bool SpiFlashBase::disable_write() {
  select_chip();
  transfer(INSTR__WRITE_DISABLE);
  deselect_chip();
  // Verify expected state of write enable bit in status register.
  return !(status_register1() & STATUS__WRITE_ENABLE);
}

bool SpiFlashBase::erase_chip() {
  /* 1. Check that:
   *      - Device is ready (see "Wait for ready")
   *      - Write is enabled (see "Write enable") */
  if (!ready_wait() || !enable_write()) { return false; }

  select_chip();
  //  2. Send `Chip erase`
  transfer(INSTR__CHIP_ERASE);
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
bool SpiFlashBase::write_page(uint32_t address, uint8_t *src, uint32_t length) {
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
  transfer(INSTR__PAGE_PROGRAM);
  transfer(address >> (2 * 8));  // A23-A16
  transfer(address >> (1 * 8));  // A15-A8
  transfer(address);  // A7-A0
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
    transfer(src[i]);
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

bool SpiFlashBase::write_page(uint32_t address, UInt8Array src) {
  return write_page(address, src.data, src.length);
}
