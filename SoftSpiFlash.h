#ifndef ___SOFT_SPI_FLASH__H___
#define ___SOFT_SPI_FLASH__H___

#include <SoftSpi.h>
#include "SpiFlashBase.h"


template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode = 0>
class SoftSpiFlash : public SpiFlashBase {
protected:
  typedef SoftSPI<MisoPin, MosiPin, SckPin, Mode> SoftSPI_t;
  SoftSPI_t soft_spi_;

  virtual uint8_t transfer(uint8_t value);
public:
  SoftSpiFlash() : SpiFlashBase() {}
  SoftSpiFlash(uint8_t cs_pin) : SpiFlashBase(cs_pin) {}

  virtual void begin();
  virtual void begin(uint8_t cs_pin);
};


template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
uint8_t SoftSpiFlash<MisoPin, MosiPin, SckPin, Mode>::transfer(uint8_t value) {
  return soft_spi_.transfer(value);
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
void SoftSpiFlash<MisoPin, MosiPin, SckPin, Mode>::begin() {
  soft_spi_.begin();
  SpiFlashBase::begin();
}

template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode>
void SoftSpiFlash<MisoPin, MosiPin, SckPin, Mode>::begin(uint8_t cs_pin) {
  SpiFlashBase::begin(cs_pin);
}


#endif  // #ifndef ___SOFT_SPI_FLASH__H___
