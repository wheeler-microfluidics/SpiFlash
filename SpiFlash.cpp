#include "SpiFlash.h"


uint8_t SpiFlash::transfer(uint8_t value) {
  return SPI.transfer(value);
}

void SpiFlash::select_chip() {
  SPI.beginTransaction(spi_settings_);
}

void SpiFlash::deselect_chip() {
  SPI.endTransaction();
}

void SpiFlash::set_spi_settings(SPISettings const &spi_settings) {
  spi_settings_ = spi_settings;
}

void SpiFlash::begin() {
  SpiFlashBase::begin();
}

void SpiFlash::begin(uint8_t cs_pin) {
  SpiFlashBase::begin(cs_pin);
}

void SpiFlash::begin(SPISettings &spi_settings) {
  set_spi_settings(spi_settings);
  begin();
}

void SpiFlash::begin(SPISettings &spi_settings, uint8_t cs_pin) {
  set_spi_settings(spi_settings);
  begin(cs_pin);
}
