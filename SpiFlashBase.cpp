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
  /*  3. Wait for erase to complete (up to 100 seconds).
   *
   *     According to "7.6 AC Electrical Characteristics" in
   *     [`w25q64v` datasheet][1], this can take up to 100
   *     seconds.
   *
   * Notes:
   *
   *  - `BUSY` bit in status register remains set until erase is complete.
   *  - Write enable bit in status register is cleared upon erase completion.
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  if (!ready_wait(100000L /* 100 seconds */)) {
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
  /*  6. Wait for page program to complete (up to 3 milliseconds).
   *
   *     According to "7.6 AC Electrical Characteristics" in
   *     [`w25q64v` datasheet][1], this can take up to 3
   *     milliseconds.
   *
   * Notes:
   *
   *  - `BUSY` bit in status register remains set until write is complete.
   *  - Write enable bit in status register is cleared upon write completion.
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  if (!ready_wait(3)) {
    disable_write();
    return false;
  }
  return true;
}

bool SpiFlashBase::write_page(uint32_t address, UInt8Array src) {
  return write_page(address, src.data, src.length);
}

uint32_t SpiFlashBase::jedec_id() {
  select_chip();
  transfer(INSTR__JEDEC_ID);
  uint32_t result = 0;

  result |= static_cast<uint32_t>(transfer(SPI__DUMMY)) << 16;  // manufacturer
  result |= static_cast<uint32_t>(transfer(SPI__DUMMY)) << 8;  // memory_type
  result |= transfer(SPI__DUMMY);  // capacity

  deselect_chip();
  return result;
}

uint64_t SpiFlashBase::read_unique_id() {
  select_chip();
  transfer(INSTR__READ_UNIQUE_ID);
  transfer(SPI__DUMMY);
  transfer(SPI__DUMMY);
  transfer(SPI__DUMMY);
  transfer(SPI__DUMMY);

  uint64_t result = 0;

  for (int i = sizeof(uint64_t) - 1; i >= 0; i--) {
    result |= static_cast<uint64_t>(transfer(SPI__DUMMY)) << (8 * i);
  }

  deselect_chip();
  return result;
}

uint8_t SpiFlashBase::read_sfdp_register(uint8_t address) {
  select_chip();
  transfer(INSTR__READ_SFDP_REGISTER);
  transfer(0);
  transfer(0);
  uint8_t result = transfer(SPI__DUMMY);
  deselect_chip();
  return result;
}

bool SpiFlashBase::erase(uint32_t address, uint8_t code,
                         uint32_t settling_time_ms) {
  if (!ready_wait() || !enable_write()) { return false; }

  select_chip();
  transfer(address >> (2 * 8));
  transfer(address >> (1 * 8));
  transfer(address);
  deselect_chip();

  /* Wait for sector erase to complete.
   *
   * Refer to "7.6 AC Electrical Characteristics" in [`w25q64v`
   * datasheet][1] for timings.
   *
   * Notes:
   *
   *  - `BUSY` bit in status register remains set until write is complete.
   *  - Write enable bit in status register is cleared upon write completion.
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  if (!ready_wait(settling_time_ms)) {
    disable_write();
    return false;
  }
  return true;
}

bool SpiFlashBase::erase_sector(uint32_t address) {
  /* **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO**
   *
   * What happens if address does not align with a sector boundary??
   *
   * **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO**
   */
  /* Wait for sector erase to complete (up to [400 milliseconds][1]).
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  return erase(address, INSTR__SECTOR_ERASE_4KB_, 400);
}

bool SpiFlashBase::erase_block_32KB(uint32_t address) {
  /* **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO**
   *
   * What happens if address does not align with a 32KB block boundary??
   *
   * **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO**
   */
  /* Wait for sector erase to complete (up to [1600 milliseconds][1]).
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  return erase(address, INSTR__BLOCK_ERASE_32KB_, 1600L);
}

bool SpiFlashBase::erase_block_64KB(uint32_t address) {
  /* **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO**
   *
   * What happens if address does not align with a 64KB block boundary??
   *
   * **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO** **TODO**
   */
  /* Wait for sector erase to complete (up to [2000 milliseconds][1]).
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  return erase(address, INSTR__BLOCK_ERASE_64KB_, 2000L);
}

void SpiFlashBase::power_down() {
  /*
   * From section 6.2.28 of the [datasheet][1]:
   *
   * > While in the power-down state only the "Release from Power-down
   * > / Device ID" instruction, which restores the device to normal
   * > operation, will be recognized.
   * >
   * > **All other instructions are ignored.** This includes the Read
   * > Status Register instruction, which is always available during
   * > normal operation.
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  select_chip();
  transfer(INSTR__POWER_DOWN);
  deselect_chip();
}

void SpiFlashBase::reset() {
  /* From section 6.2.43 in [`w25q64v` datasheet][1]:
   *
   * > To avoid accidental reset, both instructions must be issued in
   * > sequence. Any other commands other than “Reset (99h)” after the
   * > “Enable Reset (66h)” command will disable the “Reset Enable”
   * > state.
   * >
   * > A new sequence of “Enable Reset (66h)” and “Reset (99h)” is
   * > needed to reset the device. Once the Reset command is accepted
   * > by the device, the device will take approximately ... 30us to
   * > reset. During this period, no command will be accepted.
   */
  // Enable reset (must be done immediately before requesting reset).
  select_chip();
  transfer(INSTR__ENABLE_RESET);
  deselect_chip();

  // Request reset.
  select_chip();
  transfer(INSTR__RESET);
  deselect_chip();
}

void SpiFlashBase::release_powerdown() {
  select_chip();
  transfer(INSTR__RELEASE_POWERDOWN_ID);
  deselect_chip();

  /* Wait for chip to "wake up".
   *
   * According to `tRES2` "7.6 AC Electrical Characteristics" in [`w25q64v`
   * datasheet][1], this can take up to 3 microseconds.
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  delayMicroseconds(3);
}

uint8_t SpiFlashBase::release_powerdown_id() {
  select_chip();
  transfer(INSTR__RELEASE_POWERDOWN_ID);
  transfer(SPI__DUMMY);
  transfer(SPI__DUMMY);
  transfer(SPI__DUMMY);
  uint8_t device_id = transfer(SPI__DUMMY);
  deselect_chip();

  /* Wait for chip to "wake up".
   *
   * According to `tRES2` "7.6 AC Electrical Characteristics" in [`w25q64v`
   * datasheet][1], this can take up to 3 microseconds.
   *
   * [1]: https://cdn.sparkfun.com/datasheets/Dev/Teensy/w25q64fv.pdf
   */
  delayMicroseconds(3);
  return device_id;
}
