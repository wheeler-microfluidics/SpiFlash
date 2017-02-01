#ifndef ___SPI_FLASH__H___
#define ___SPI_FLASH__H___


#include <SPI.h>
#include "SpiFlashBase.h"


class SpiFlash : public SpiFlashBase {
protected:
  SPISettings spi_settings_;

  virtual uint8_t transfer(uint8_t value);
  virtual void select_chip();
  virtual void deselect_chip();
public:
  SpiFlash() : SpiFlashBase() {}
  SpiFlash(uint8_t cs_pin) : SpiFlashBase(cs_pin) {}

  void set_spi_settings(SPISettings const &spi_settings);

  virtual void begin();
  virtual void begin(uint8_t cs_pin);
  virtual void begin(SPISettings &spi_settings);
  virtual void begin(SPISettings &spi_settings, uint8_t cs_pin);
};


#endif  // #ifndef ___SPI_FLASH__H___
