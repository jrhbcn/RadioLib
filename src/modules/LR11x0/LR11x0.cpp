#include "LR11x0.h"

#include "../../utils/CRC.h"
#include "../../utils/Cryptography.h"

#include <string.h>
#include <math.h>

#if !RADIOLIB_EXCLUDE_LR11X0

LR11x0::LR11x0(Module* mod) : PhysicalLayer(RADIOLIB_LR11X0_FREQUENCY_STEP_SIZE, RADIOLIB_LR11X0_MAX_PACKET_LENGTH) {
  this->mod = mod;
  this->XTAL = false;
}

int16_t LR11x0::begin(float bw, uint8_t sf, uint8_t cr, uint8_t syncWord, uint16_t preambleLength, float tcxoVoltage) {
  // set module properties
  this->mod->init();
  this->mod->hal->pinMode(this->mod->getIrq(), this->mod->hal->GpioModeInput);
  this->mod->hal->pinMode(this->mod->getGpio(), this->mod->hal->GpioModeInput);
  this->mod->hal->pinMode(this->mod->getIrq(), this->mod->hal->GpioModeInput);
  this->mod->hal->pinMode(this->mod->getGpio(), this->mod->hal->GpioModeInput);
  this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_ADDR] = Module::BITS_32;
  this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_CMD] = Module::BITS_16;
  this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_STATUS] = Module::BITS_8;
  this->mod->spiConfig.statusPos = 0;
  this->mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_READ] = RADIOLIB_LR11X0_CMD_READ_REG_MEM;
  this->mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_WRITE] = RADIOLIB_LR11X0_CMD_WRITE_REG_MEM;
  this->mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_NOP] = RADIOLIB_LR11X0_CMD_NOP;
  this->mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_STATUS] = RADIOLIB_LR11X0_CMD_GET_STATUS;
  this->mod->spiConfig.stream = true;
  this->mod->spiConfig.parseStatusCb = SPIparseStatus;
  this->mod->spiConfig.checkStatusCb = SPIcheckStatus;
  
  // try to find the LR11x0 chip - this will also reset the module at least once
  if(!LR11x0::findChip(this->chipType)) {
    RADIOLIB_DEBUG_BASIC_PRINTLN("No LR11x0 found!");
    this->mod->term();
    return(RADIOLIB_ERR_CHIP_NOT_FOUND);
  }
  RADIOLIB_DEBUG_BASIC_PRINTLN("M\tLR11x0");

  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // set TCXO control, if requested
  if(!this->XTAL && tcxoVoltage > 0.0) {
    state = setTCXO(tcxoVoltage);
    RADIOLIB_ASSERT(state);
  }

  // configure settings not accessible by API
  state = config(RADIOLIB_LR11X0_PACKET_TYPE_LORA);
  RADIOLIB_ASSERT(state);

  // configure publicly accessible settings
  state = setBandwidth(bw);
  RADIOLIB_ASSERT(state);

  state = setSpreadingFactor(sf);
  RADIOLIB_ASSERT(state);

  state = setCodingRate(cr);
  RADIOLIB_ASSERT(state);

  state = setSyncWord(syncWord);
  RADIOLIB_ASSERT(state);

  state = setPreambleLength(preambleLength);
  RADIOLIB_ASSERT(state);

  // set publicly accessible settings that are not a part of begin method
  // TODO looks like CRC does not work for SX127x
  state = setCRC(0);
  RADIOLIB_ASSERT(state);

  state = invertIQ(false);
  RADIOLIB_ASSERT(state);

  return(RADIOLIB_ERR_NONE);
}

int16_t LR11x0::beginGFSK(float br, float freqDev, float rxBw, uint16_t preambleLength, float tcxoVoltage) {
  // set module properties
  this->mod->init();
  this->mod->hal->pinMode(this->mod->getIrq(), this->mod->hal->GpioModeInput);
  this->mod->hal->pinMode(this->mod->getGpio(), this->mod->hal->GpioModeInput);
  this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_ADDR] = Module::BITS_32;
  this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_CMD] = Module::BITS_16;
  this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_STATUS] = Module::BITS_8;
  this->mod->spiConfig.statusPos = 0;
  this->mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_READ] = RADIOLIB_LR11X0_CMD_READ_REG_MEM;
  this->mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_WRITE] = RADIOLIB_LR11X0_CMD_WRITE_REG_MEM;
  this->mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_NOP] = RADIOLIB_LR11X0_CMD_NOP;
  this->mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_STATUS] = RADIOLIB_LR11X0_CMD_GET_STATUS;
  this->mod->spiConfig.stream = true;
  this->mod->spiConfig.parseStatusCb = SPIparseStatus;
  this->mod->spiConfig.checkStatusCb = SPIcheckStatus;
  
  // try to find the LR11x0 chip - this will also reset the module at least once
  if(!LR11x0::findChip(this->chipType)) {
    RADIOLIB_DEBUG_BASIC_PRINTLN("No LR11x0 found!");
    this->mod->term();
    return(RADIOLIB_ERR_CHIP_NOT_FOUND);
  }
  RADIOLIB_DEBUG_BASIC_PRINTLN("M\tLR11x0");

  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);
  
  // TODO implement GFSK
  (void)br;
  (void)freqDev;
  (void)rxBw;
  (void)preambleLength;
  (void)tcxoVoltage;

  return(RADIOLIB_ERR_UNSUPPORTED);
}

int16_t LR11x0::reset() {
  // run the reset sequence
  this->mod->hal->pinMode(this->mod->getRst(), this->mod->hal->GpioModeOutput);
  this->mod->hal->digitalWrite(this->mod->getRst(), this->mod->hal->GpioLevelLow);
  this->mod->hal->delay(10);
  this->mod->hal->digitalWrite(this->mod->getRst(), this->mod->hal->GpioLevelHigh);

  // the typical transition duration should be 273 ms
  this->mod->hal->delay(300);
  
  // wait for BUSY to go low
  uint32_t start = this->mod->hal->millis();
  while(this->mod->hal->digitalRead(this->mod->getGpio())) {
    this->mod->hal->yield();
    if(this->mod->hal->millis() - start >= 3000) {
      RADIOLIB_DEBUG_BASIC_PRINTLN("BUSY pin timeout after reset!");
      return(RADIOLIB_ERR_SPI_CMD_TIMEOUT);
    }
  }

  return(RADIOLIB_ERR_NONE);
}

int16_t LR11x0::transmit(uint8_t* data, size_t len, uint8_t addr) {
   // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // check packet length
  if(len > RADIOLIB_LR11X0_MAX_PACKET_LENGTH) {
    return(RADIOLIB_ERR_PACKET_TOO_LONG);
  }

  uint32_t timeout = 0;

  // get currently active modem
  uint8_t modem = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  state = getPacketType(&modem);
  if(modem == RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    // calculate timeout (150% of expected time-on-air)
    timeout = (getTimeOnAir(len) * 3) / 2;

  } else if(modem == RADIOLIB_LR11X0_PACKET_TYPE_GFSK) {
    // calculate timeout (500% of expected time-on-air)
    timeout = getTimeOnAir(len) * 5;

  } else {
    return(RADIOLIB_ERR_UNKNOWN);
  }

  RADIOLIB_DEBUG_BASIC_PRINTLN("Timeout in %lu us", timeout);

  // start transmission
  state = startTransmit(data, len, addr);
  RADIOLIB_ASSERT(state);

  // wait for packet transmission or timeout
  uint32_t start = this->mod->hal->micros();
  while(!this->mod->hal->digitalRead(this->mod->getIrq())) {
    this->mod->hal->yield();
    if(this->mod->hal->micros() - start > timeout) {
      finishTransmit();
      return(RADIOLIB_ERR_TX_TIMEOUT);
    }
  }
  uint32_t elapsed = this->mod->hal->micros() - start;

  // update data rate
  this->dataRateMeasured = (len*8.0)/((float)elapsed/1000000.0);

  return(finishTransmit());
}

int16_t LR11x0::receive(uint8_t* data, size_t len) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  uint32_t timeout = 0;

  // get currently active modem
  uint8_t modem = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  state = getPacketType(&modem);
  if(modem == RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    // calculate timeout (100 LoRa symbols, the default for SX127x series)
    float symbolLength = (float)(uint32_t(1) << this->spreadingFactor) / (float)this->bandwidthKhz;
    timeout = (uint32_t)(symbolLength * 100.0);
  
  } else if(modem == RADIOLIB_LR11X0_PACKET_TYPE_GFSK) {
    // calculate timeout (500 % of expected time-one-air)
    size_t maxLen = len;
    if(len == 0) { 
      maxLen = 0xFF;
    }
    float brBps = ((float)(RADIOLIB_LR11X0_CRYSTAL_FREQ) * 1000000.0 * 32.0) / (float)this->bitRate;
    timeout = (uint32_t)(((maxLen * 8.0) / brBps) * 1000.0 * 5.0);

  } else {
    return(RADIOLIB_ERR_UNKNOWN);
  
  }

  RADIOLIB_DEBUG_BASIC_PRINTLN("Timeout in %lu ms", timeout);

  // start reception
  uint32_t timeoutValue = (uint32_t)(((float)timeout * 1000.0) / 30.52);
  state = startReceive(timeoutValue);
  RADIOLIB_ASSERT(state);

  // wait for packet reception or timeout
  bool softTimeout = false;
  uint32_t start = this->mod->hal->millis();
  while(!this->mod->hal->digitalRead(this->mod->getIrq())) {
    this->mod->hal->yield();
    // safety check, the timeout should be done by the radio
    if(this->mod->hal->millis() - start > timeout) {
      softTimeout = true;
      break;
    }
  }

  // if it was a timeout, this will return an error code
  // TODO taken from SX126x, does this really work?
  state = standby();
  if((state != RADIOLIB_ERR_NONE) && (state != RADIOLIB_ERR_SPI_CMD_TIMEOUT)) {
    return(state);
  }

  // check whether this was a timeout or not
  if((getIrqStatus() & RADIOLIB_LR11X0_IRQ_TIMEOUT) || softTimeout) {
    standby();
    clearIrq(RADIOLIB_LR11X0_IRQ_ALL);
    return(RADIOLIB_ERR_RX_TIMEOUT);
  }

  // read the received data
  return(readData(data, len));
}

int16_t LR11x0::standby() {
  return(LR11x0::standby(RADIOLIB_LR11X0_STANDBY_RC));
}

int16_t LR11x0::standby(uint8_t mode, bool wakeup) {
  // set RF switch (if present)
  this->mod->setRfSwitchState(Module::MODE_IDLE);

  // TODO this will block BUSY forever
  (void)wakeup;
  /*if(wakeup) {
    // pull NSS low to wake up
    this->mod->hal->digitalWrite(this->mod->getCs(), this->mod->hal->GpioLevelLow);
  }*/

  uint8_t buff[] = { mode };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_STANDBY, true, buff, 1));
}

int16_t LR11x0::sleep(bool retainConfig, uint32_t sleepTime) {
  // set RF switch (if present)
  this->mod->setRfSwitchState(Module::MODE_IDLE);

  uint8_t buff[] = { 
    (uint8_t)retainConfig,
    (uint8_t)((sleepTime >> 24) & 0xFF), (uint8_t)((sleepTime >> 16) & 0xFF),
    (uint8_t)((sleepTime >> 16) & 0xFF), (uint8_t)(sleepTime & 0xFF),
  };
  if(sleepTime) {
    buff[0] |= RADIOLIB_LR11X0_SLEEP_WAKEUP_ENABLED;
  }

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_SLEEP, true, buff, sizeof(buff));

  // wait for the module to safely enter sleep mode
  this->mod->hal->delay(1);

  return(state);
}

void LR11x0::setDio1Action(void (*func)(void)) {
  this->mod->hal->attachInterrupt(this->mod->hal->pinToInterrupt(this->mod->getIrq()), func, this->mod->hal->GpioInterruptRising);
}

void LR11x0::clearDio1Action() {
  this->mod->hal->detachInterrupt(this->mod->hal->pinToInterrupt(this->mod->getIrq()));
}

void LR11x0::setPacketReceivedAction(void (*func)(void)) {
  this->setDio1Action(func);
}

void LR11x0::clearPacketReceivedAction() {
  this->clearDio1Action();
}

void LR11x0::setPacketSentAction(void (*func)(void)) {
  this->setDio1Action(func);
}

void LR11x0::clearPacketSentAction() {
  this->clearDio1Action();
}

int16_t LR11x0::startTransmit(uint8_t* data, size_t len, uint8_t addr) {
  // suppress unused variable warning
  (void)addr;

  // check packet length
  if(len > RADIOLIB_LR11X0_MAX_PACKET_LENGTH) {
    return(RADIOLIB_ERR_PACKET_TOO_LONG);
  }

  // maximum packet length is decreased by 1 when address filtering is active
  if((this->addrComp != RADIOLIB_LR11X0_GFSK_ADDR_FILTER_DISABLED) && (len > RADIOLIB_LR11X0_MAX_PACKET_LENGTH - 1)) {
    return(RADIOLIB_ERR_PACKET_TOO_LONG);
  }

  // set packet Length
  int16_t state = RADIOLIB_ERR_NONE;
  uint8_t modem = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  state = getPacketType(&modem);
  RADIOLIB_ASSERT(state);
  if(modem == RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    state = setPacketParamsLoRa(this->preambleLengthLoRa, this->headerType, len, this->crcTypeLoRa, this->invertIQEnabled);
  } else if(modem == RADIOLIB_LR11X0_PACKET_TYPE_GFSK) {
    state = setPacketParamsGFSK(this->preambleLengthGFSK, this->preambleDetLength, this->syncWordLength, this->addrComp, this->packetType, len, this->crcTypeGFSK, this->whitening);
  } else {
    return(RADIOLIB_ERR_UNKNOWN);
  }
  RADIOLIB_ASSERT(state);

  // set DIO mapping
  state = setDioIrqParams(RADIOLIB_LR11X0_IRQ_TX_DONE | RADIOLIB_LR11X0_IRQ_TIMEOUT, 0);
  RADIOLIB_ASSERT(state);

  // write packet to buffer
  state = writeBuffer8(data, len);
  RADIOLIB_ASSERT(state);

  // clear interrupt flags
  state = clearIrq(RADIOLIB_LR11X0_IRQ_ALL);
  RADIOLIB_ASSERT(state);

  // set RF switch (if present)
  this->mod->setRfSwitchState(Module::MODE_TX);

  // start transmission
  state = setTx(RADIOLIB_LR11X0_TX_TIMEOUT_NONE);
  RADIOLIB_ASSERT(state);

  // wait for BUSY to go low (= PA ramp up done)
  while(this->mod->hal->digitalRead(this->mod->getGpio())) {
    this->mod->hal->yield();
  }

  return(state);
}

int16_t LR11x0::finishTransmit() {
  // clear interrupt flags
  clearIrq(RADIOLIB_LR11X0_IRQ_ALL);

  // set mode to standby to disable transmitter/RF switch
  return(standby());
}

int16_t LR11x0::startReceive() {
  return(this->startReceive(RADIOLIB_LR11X0_RX_TIMEOUT_INF, RADIOLIB_LR11X0_IRQ_RX_DONE, 0));
}

int16_t LR11x0::startReceive(uint32_t timeout, uint32_t irqFlags, size_t len) {
  (void)len;
  
  // check active modem
  int16_t state = RADIOLIB_ERR_NONE;
  uint8_t modem = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  state = getPacketType(&modem);
  RADIOLIB_ASSERT(state);
  if((modem != RADIOLIB_LR11X0_PACKET_TYPE_LORA) && (modem != RADIOLIB_LR11X0_PACKET_TYPE_GFSK)) {
    return(RADIOLIB_ERR_WRONG_MODEM);
  }

  // set DIO mapping
  uint32_t irq = irqFlags;
  if(timeout != RADIOLIB_LR11X0_RX_TIMEOUT_INF) {
    irq |= RADIOLIB_LR11X0_IRQ_TIMEOUT;
  }

  state = setDioIrqParams(irq, RADIOLIB_LR11X0_IRQ_NONE);
  RADIOLIB_ASSERT(state);

  // clear interrupt flags
  state = clearIrq(RADIOLIB_LR11X0_IRQ_ALL);
  RADIOLIB_ASSERT(state);

  // set implicit mode and expected len if applicable
  if((this->headerType == RADIOLIB_LR11X0_LORA_HEADER_IMPLICIT) && (modem == RADIOLIB_LR11X0_PACKET_TYPE_LORA)) {
    state = setPacketParamsLoRa(this->preambleLengthLoRa, this->headerType, this->implicitLen, this->crcTypeLoRa, this->invertIQEnabled);
    RADIOLIB_ASSERT(state);
  }

  // set RF switch (if present)
  this->mod->setRfSwitchState(Module::MODE_RX);

  // set mode to receive
  state = setRx(timeout);

  return(state);
}

uint32_t LR11x0::getIrqStatus() {
  // there is no dedicated "get IRQ" command, the IRQ bits are sent after the status bytes
  uint8_t buff[6] = { 0 };
  this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_STATUS] = Module::BITS_0;
  mod->SPItransferStream(NULL, 0, false, NULL, buff, sizeof(buff), true, RADIOLIB_MODULE_SPI_TIMEOUT);
  this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_STATUS] = Module::BITS_8;
  uint32_t irq = ((uint32_t)(buff[2]) << 24) | ((uint32_t)(buff[3]) << 16) | ((uint32_t)(buff[4]) << 8) | (uint32_t)buff[5];
  return(irq);
}

int16_t LR11x0::readData(uint8_t* data, size_t len) {
  // check active modem
  int16_t state = RADIOLIB_ERR_NONE;
  uint8_t modem = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  state = getPacketType(&modem);
  RADIOLIB_ASSERT(state);
  if((modem != RADIOLIB_LR11X0_PACKET_TYPE_LORA) && (modem != RADIOLIB_LR11X0_PACKET_TYPE_GFSK)) {
    return(RADIOLIB_ERR_WRONG_MODEM);
  }

  // check integrity CRC
  uint32_t irq = getIrqStatus();
  int16_t crcState = RADIOLIB_ERR_NONE;
  if((irq & RADIOLIB_LR11X0_IRQ_CRC_ERR) || (irq & RADIOLIB_LR11X0_IRQ_HEADER_ERR)) {
    crcState = RADIOLIB_ERR_CRC_MISMATCH;
  }

  // get packet length
  // the offset is needed since LR11x0 seems to move the buffer base by 4 bytes on every packet
  uint8_t offset = 0;
  size_t length = getPacketLength(true, &offset);
  if((len != 0) && (len < length)) {
    // user requested less data than we got, only return what was requested
    length = len;
  }

  // read packet data
  state = readBuffer8(data, length, offset);
  RADIOLIB_ASSERT(state);

  // clear the Rx buffer
  state = clearRxBuffer();
  RADIOLIB_ASSERT(state);

  // clear interrupt flags
  state = clearIrq(RADIOLIB_LR11X0_IRQ_ALL);

  // check if CRC failed - this is done after reading data to give user the option to keep them
  RADIOLIB_ASSERT(crcState);

  return(state);
}

int16_t LR11x0::setBandwidth(float bw) {
  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  int16_t state = getPacketType(&type);
  RADIOLIB_ASSERT(state);
  if(type != RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    return(RADIOLIB_ERR_WRONG_MODEM);
  }

  // ensure byte conversion doesn't overflow
  RADIOLIB_CHECK_RANGE(bw, 0.0, 510.0, RADIOLIB_ERR_INVALID_BANDWIDTH);

  // check allowed bandwidth values
  uint8_t bw_div2 = bw / 2 + 0.01;
  switch (bw_div2)  {
    case 31: // 62.5:
      this->bandwidth = RADIOLIB_LR11X0_LORA_BW_62_5;
      break;
    case 62: // 125.0:
      this->bandwidth = RADIOLIB_LR11X0_LORA_BW_125_0;
      break;
    case 125: // 250.0
      this->bandwidth = RADIOLIB_LR11X0_LORA_BW_250_0;
      break;
    case 250: // 500.0
      this->bandwidth = RADIOLIB_LR11X0_LORA_BW_500_0;
      break;
    default:
      return(RADIOLIB_ERR_INVALID_BANDWIDTH);
  }

  // update modulation parameters
  this->bandwidthKhz = bw;
  return(setModulationParamsLoRa(this->spreadingFactor, this->bandwidth, this->codingRate, this->ldrOptimize));
}

int16_t LR11x0::setSpreadingFactor(uint8_t sf, bool legacy) {
  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  int16_t state = getPacketType(&type);
  RADIOLIB_ASSERT(state);
  if(type != RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    return(RADIOLIB_ERR_WRONG_MODEM);
  }

  RADIOLIB_CHECK_RANGE(sf, 5, 12, RADIOLIB_ERR_INVALID_SPREADING_FACTOR);

  // enable SF6 legacy mode
  if(legacy && (sf == 6)) {
    //this->mod->SPIsetRegValue(RADIOLIB_LR11X0_REG_SF6_SX127X_COMPAT, RADIOLIB_LR11X0_SF6_SX127X, 18, 18);
  }

  // update modulation parameters
  this->spreadingFactor = sf;
  return(setModulationParamsLoRa(this->spreadingFactor, this->bandwidth, this->codingRate, this->ldrOptimize));
}

int16_t LR11x0::setCodingRate(uint8_t cr, bool longInterleave) {
  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  int16_t state = getPacketType(&type);
  RADIOLIB_ASSERT(state);
  if(type != RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    return(RADIOLIB_ERR_WRONG_MODEM);
  }

  RADIOLIB_CHECK_RANGE(cr, 5, 8, RADIOLIB_ERR_INVALID_CODING_RATE);

  if(longInterleave) {
    switch(cr) {
      case 5:
      case 6:
        this->codingRate = cr;
        break;
      case 8: 
        this->codingRate = cr - 1;
        break;
      default:
        return(RADIOLIB_ERR_INVALID_CODING_RATE);
    }
  
  } else {
    this->codingRate = cr - 4;
  
  }

  // update modulation parameters
  return(setModulationParamsLoRa(this->spreadingFactor, this->bandwidth, this->codingRate, this->ldrOptimize));
}

int16_t LR11x0::setSyncWord(uint8_t syncWord) {
  return(setLoRaSyncWord(syncWord));
}

int16_t LR11x0::setPreambleLength(size_t preambleLength) {
  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  int16_t state = getPacketType(&type);
  RADIOLIB_ASSERT(state);
  if(type == RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    this->preambleLengthLoRa = preambleLength;
    return(setPacketParamsLoRa(this->preambleLengthLoRa, this->headerType,  this->implicitLen, this->crcTypeLoRa, (uint8_t)this->invertIQEnabled));
  }

  return(RADIOLIB_ERR_WRONG_MODEM);
}

int16_t LR11x0::setTCXO(float voltage, uint32_t delay) {
  // check if TCXO is enabled at all
  if(this->XTAL) {
    return(RADIOLIB_ERR_INVALID_TCXO_VOLTAGE);
  }

  // set mode to standby
  standby();

  // check RADIOLIB_LR11X0_ERROR_STAT_HF_XOSC_START_ERR flag and clear it
  uint16_t errors = 0;
  int16_t state = getErrors(&errors);
  RADIOLIB_ASSERT(state);
  if(errors & RADIOLIB_LR11X0_ERROR_STAT_HF_XOSC_START_ERR) {
    clearErrors();
  }

  // check 0 V disable
  if(fabs(voltage - 0.0) <= 0.001) {
    setTcxoMode(0, 0);
    return(reset());
  }

  // check allowed voltage values
  uint8_t tune = 0;
  if(fabs(voltage - 1.6) <= 0.001) {
    tune = RADIOLIB_LR11X0_TCXO_VOLTAGE_1_6;
  } else if(fabs(voltage - 1.7) <= 0.001) {
    tune = RADIOLIB_LR11X0_TCXO_VOLTAGE_1_7;
  } else if(fabs(voltage - 1.8) <= 0.001) {
    tune = RADIOLIB_LR11X0_TCXO_VOLTAGE_1_8;
  } else if(fabs(voltage - 2.2) <= 0.001) {
    tune = RADIOLIB_LR11X0_TCXO_VOLTAGE_2_2;
  } else if(fabs(voltage - 2.4) <= 0.001) {
    tune = RADIOLIB_LR11X0_TCXO_VOLTAGE_2_4;
  } else if(fabs(voltage - 2.7) <= 0.001) {
    tune = RADIOLIB_LR11X0_TCXO_VOLTAGE_2_7;
  } else if(fabs(voltage - 3.0) <= 0.001) {
    tune = RADIOLIB_LR11X0_TCXO_VOLTAGE_3_0;
  } else if(fabs(voltage - 3.3) <= 0.001) {
    tune = RADIOLIB_LR11X0_TCXO_VOLTAGE_3_3;
  } else {
    return(RADIOLIB_ERR_INVALID_TCXO_VOLTAGE);
  }

  // calculate delay value
  uint32_t delayValue = (uint32_t)((float)delay / 30.52f);
  if(delayValue == 0) {
    delayValue = 1;
  }
 
  // enable TCXO control
  return(setTcxoMode(tune, delayValue));
}

int16_t LR11x0::setCRC(uint8_t len, uint16_t initial, uint16_t polynomial, bool inverted) {
  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  int16_t state = getPacketType(&type);
  RADIOLIB_ASSERT(state);
  if(type == RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    // LoRa CRC doesn't allow to set CRC polynomial, initial value, or inversion
    this->crcTypeLoRa = len > 0 ? RADIOLIB_LR11X0_LORA_CRC_ENABLED : RADIOLIB_LR11X0_LORA_CRC_DISABLED;
    return(setPacketParamsLoRa(this->preambleLengthLoRa, this->crcTypeLoRa, this->implicitLen, this->headerType, (uint8_t)this->invertIQEnabled));
  
  } else if(type == RADIOLIB_LR11X0_PACKET_TYPE_GFSK) {
    // TODO add GFSK support
    (void)initial;
    (void)polynomial;
    (void)inverted;
    return(RADIOLIB_ERR_UNSUPPORTED);
  
  }

  return(RADIOLIB_ERR_WRONG_MODEM);
}

int16_t LR11x0::invertIQ(bool enable) {
  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  int16_t state = getPacketType(&type);
  RADIOLIB_ASSERT(state);
  if(type != RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    return(RADIOLIB_ERR_WRONG_MODEM);
  }

  this->invertIQEnabled = enable;
  return(setPacketParamsLoRa(this->preambleLengthLoRa, this->crcTypeLoRa, this->implicitLen, this->headerType, (uint8_t)this->invertIQEnabled));
}

float LR11x0::getRSSI() {
  float val = 0;

  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  getPacketType(&type);
  if(type == RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    getPacketStatusLoRa(&val, NULL, NULL);

  } else if(type == RADIOLIB_LR11X0_PACKET_TYPE_GFSK) {
    getPacketStatusGFSK(NULL, &val, NULL, NULL);
  
  }

  return(val);
}

float LR11x0::getSNR() {
  float val = 0;

  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  getPacketType(&type);
  if(type == RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    getPacketStatusLoRa(NULL, &val, NULL);
  }

  return(val);
}

float LR11x0::getFrequencyError() {
  // TODO implement this
  return(0);
}

size_t LR11x0::getPacketLength(bool update) {
  return(this->getPacketLength(update, NULL));
}

size_t LR11x0::getPacketLength(bool update, uint8_t* offset) {
  (void)update;

  // in implicit mode, return the cached value
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  (void)getPacketType(&type);
  if((type == RADIOLIB_LR11X0_PACKET_TYPE_LORA) && (this->headerType == RADIOLIB_LR11X0_LORA_HEADER_IMPLICIT)) {
    return(this->implicitLen);
  }

  uint8_t len = 0;
  (void)getRxBufferStatus(&len, offset);
  return((size_t)len);
}

uint32_t LR11x0::getTimeOnAir(size_t len) {
  // check active modem
  uint8_t type = RADIOLIB_LR11X0_PACKET_TYPE_NONE;
  getPacketType(&type);
  if(type == RADIOLIB_LR11X0_PACKET_TYPE_LORA) {
    // calculate number of symbols
    float N_symbol = 0;
    if(this->codingRate <= RADIOLIB_LR11X0_LORA_CR_4_8_SHORT) {
      // legacy coding rate - nice and simple

      // get SF coefficients
      float coeff1 = 0;
      int16_t coeff2 = 0;
      int16_t coeff3 = 0;
      if(this->spreadingFactor < 7) {
        // SF5, SF6
        coeff1 = 6.25;
        coeff2 = 4*this->spreadingFactor;
        coeff3 = 4*this->spreadingFactor;
      } else if(this->spreadingFactor < 11) {
        // SF7. SF8, SF9, SF10
        coeff1 = 4.25;
        coeff2 = 4*this->spreadingFactor + 8;
        coeff3 = 4*this->spreadingFactor;
      } else {
        // SF11, SF12
        coeff1 = 4.25;
        coeff2 = 4*this->spreadingFactor + 8;
        coeff3 = 4*(this->spreadingFactor - 2);
      }

      // get CRC length
      int16_t N_bitCRC = 16;
      if(this->crcTypeLoRa == RADIOLIB_LR11X0_LORA_CRC_DISABLED) {
        N_bitCRC = 0;
      }

      // get header length
      int16_t N_symbolHeader = 20;
      if(this->headerType == RADIOLIB_LR11X0_LORA_HEADER_IMPLICIT) {
        N_symbolHeader = 0;
      }

      // calculate number of LoRa preamble symbols
      uint32_t N_symbolPreamble = (this->preambleLengthLoRa & 0x0F) * (uint32_t(1) << ((this->preambleLengthLoRa & 0xF0) >> 4));

      // calculate the number of symbols
      N_symbol = (float)N_symbolPreamble + coeff1 + 8.0 + ceil(RADIOLIB_MAX((int16_t)(8 * len + N_bitCRC - coeff2 + N_symbolHeader), (int16_t)0) / (float)coeff3) * (float)(this->codingRate + 4);

    } else {
      // long interleaving - abandon hope all ye who enter here
      /// \todo implement this mess - SX1280 datasheet v3.0 section 7.4.4.2

    }

    // get time-on-air in us
    return(((uint32_t(1) << this->spreadingFactor) / this->bandwidthKhz) * N_symbol * 1000.0);

  } else if(type == RADIOLIB_LR11X0_PACKET_TYPE_GFSK) {
    return(((uint32_t)len * 8 * 1000) / this->bitRateKbps);
  
  }

  return(0);
}

float LR11x0::getDataRate() const {
  return(this->dataRateMeasured);
}

int16_t LR11x0::SPIparseStatus(uint8_t in) {
  if((in & 0b00001110) == RADIOLIB_LR11X0_STAT_1_CMD_PERR) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  } else if((in & 0b00001110) == RADIOLIB_LR11X0_STAT_1_CMD_FAIL) {
    return(RADIOLIB_ERR_SPI_CMD_FAILED);
  } else if((in == 0x00) || (in == 0xFF)) {
    return(RADIOLIB_ERR_CHIP_NOT_FOUND);
  }
  return(RADIOLIB_ERR_NONE);
}

int16_t LR11x0::SPIcheckStatus(Module* mod) {
  // the status check command doesn't return status in the same place as other read commands,
  // but only as the first byte (as with any other command), hence LR11x0::SPIcommand can't be used
  // it also seems to ignore the actual command, and just sending in bunch of NOPs will work 
  uint8_t buff[6] = { 0 };
  mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_STATUS] = Module::BITS_0;
  int16_t state = mod->SPItransferStream(NULL, 0, false, NULL, buff, sizeof(buff), true, RADIOLIB_MODULE_SPI_TIMEOUT);
  mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_STATUS] = Module::BITS_8;
  RADIOLIB_ASSERT(state);
  return(LR11x0::SPIparseStatus(buff[0]));
}

int16_t LR11x0::SPIcommand(uint16_t cmd, bool write, uint8_t* data, size_t len, uint8_t* out, size_t outLen) {
  int16_t state = RADIOLIB_ERR_UNKNOWN;
  if(!write) {
    // the SPI interface of LR11x0 requires two separate transactions for reading
    // send the 16-bit command
    state = this->mod->SPIwriteStream(cmd, out, outLen, true, false);
    RADIOLIB_ASSERT(state);

    // read the result without command
    this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_CMD] = Module::BITS_0;
    state = this->mod->SPIreadStream(RADIOLIB_LR11X0_CMD_NOP, data, len, true, false);
    this->mod->spiConfig.widths[RADIOLIB_MODULE_SPI_WIDTH_CMD] = Module::BITS_16;

  } else {
    // write is just a single transaction
    state = this->mod->SPIwriteStream(cmd, data, len, true, true);
  
  }
  
  return(state);
}

bool LR11x0::findChip(uint8_t ver) {
  uint8_t i = 0;
  bool flagFound = false;
  while((i < 10) && !flagFound) {
    // reset the module
    reset();

    // read the version
    uint8_t device = 0xFF;
    if((this->getVersion(NULL, &device, NULL, NULL) == RADIOLIB_ERR_NONE) && (device == ver)) {
      RADIOLIB_DEBUG_BASIC_PRINTLN("Found LR11x0: RADIOLIB_LR11X0_CMD_GET_VERSION = 0x%02x", device);
      flagFound = true;
    } else {
      RADIOLIB_DEBUG_BASIC_PRINTLN("LR11x0 not found! (%d of 10 tries) RADIOLIB_LR11X0_CMD_GET_VERSION = 0x%02x", i + 1, device);
      RADIOLIB_DEBUG_BASIC_PRINTLN("Expected: 0x%02x", ver);
      this->mod->hal->delay(10);
      i++;
    }
  }

  return(flagFound);
}

int16_t LR11x0::config(uint8_t modem) {
  int16_t state = RADIOLIB_ERR_UNKNOWN;

  // set Rx/Tx fallback mode to STDBY_RC
  state = this->setRxTxFallbackMode(RADIOLIB_LR11X0_FALLBACK_MODE_STBY_RC);
  RADIOLIB_ASSERT(state);

  // TODO set some CAD parameters - will be overwritten when calling CAD anyway

  // clear IRQ
  state = this->clearIrq(RADIOLIB_LR11X0_IRQ_ALL);
  state |= this->setDioIrqParams(RADIOLIB_LR11X0_IRQ_NONE, RADIOLIB_LR11X0_IRQ_NONE);
  RADIOLIB_ASSERT(state);

  // calibrate all blocks
  state = this->calibrate(RADIOLIB_LR11X0_CALIBRATE_ALL);

  // wait for calibration completion
  this->mod->hal->delay(5);
  while(this->mod->hal->digitalRead(this->mod->getGpio())) {
    this->mod->hal->yield();
  }
  
  // if something failed, show the device errors
  #if RADIOLIB_DEBUG_BASIC
  if(state != RADIOLIB_ERR_NONE) {
    // unless mode is forced to standby, device errors will be 0
    standby();
    uint16_t errors = 0;
    getErrors(&errors);
    RADIOLIB_DEBUG_BASIC_PRINTLN("Calibration failed, device errors: 0x%X", errors);
  }
  #endif

  // set modem
  state = this->setPacketType(modem);
  return(state);
}

Module* LR11x0::getMod() {
  return(this->mod);
}

int16_t LR11x0::writeRegMem32(uint32_t addr, uint32_t* data, size_t len) {
  // check maximum size
  if(len > (RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN/sizeof(uint32_t))) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }
  return(this->writeCommon(RADIOLIB_LR11X0_CMD_WRITE_REG_MEM, addr, data, len));
}

int16_t LR11x0::readRegMem32(uint32_t addr, uint32_t* data, size_t len) {
  // check maximum size
  if(len >= (RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN/sizeof(uint32_t))) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }

  // the request contains the address and length
  uint8_t reqBuff[5] = {
    (uint8_t)((addr >> 24) & 0xFF), (uint8_t)((addr >> 16) & 0xFF),
    (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF),
    (uint8_t)len,
  };

  // build buffers - later we need to ensure endians are correct, 
  // so there is probably no way to do this without copying buffers and iterating
  #if RADIOLIB_STATIC_ONLY
    uint8_t rplBuff[RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* rplBuff = new uint8_t[len*sizeof(uint32_t)];
  #endif

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_READ_REG_MEM, false, rplBuff, len*sizeof(uint32_t), reqBuff, sizeof(reqBuff));

  // convert endians
  if(data && (state == RADIOLIB_ERR_NONE)) {
    for(size_t i = 0; i < len; i++) {
      data[i] = ((uint32_t)rplBuff[2 + i*sizeof(uint32_t)] << 24) | ((uint32_t)rplBuff[3 + i*sizeof(uint32_t)] << 16) | ((uint32_t)rplBuff[4 + i*sizeof(uint32_t)] << 8) | (uint32_t)rplBuff[5 + i*sizeof(uint32_t)];
    }
  }

  #if !RADIOLIB_STATIC_ONLY
    delete[] rplBuff;
  #endif
  
  return(state);
}

int16_t LR11x0::writeBuffer8(uint8_t* data, size_t len) {
  // check maximum size
  if(len > RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WRITE_BUFFER, true, data, len));
}

int16_t LR11x0::readBuffer8(uint8_t* data, size_t len, size_t offset) {
  // check maximum size
  if(len > RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }

  // build buffers
  size_t reqLen = 2*sizeof(uint8_t) + len;
  #if RADIOLIB_STATIC_ONLY
    uint8_t reqBuff[sizeof(uint32_t) + RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* reqBuff = new uint8_t[reqLen];
  #endif

  // set the offset and length
  reqBuff[0] = (uint8_t)offset;
  reqBuff[1] = (uint8_t)len;

  // send the request
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_READ_BUFFER, false, data, len, reqBuff, reqLen);
  #if !RADIOLIB_STATIC_ONLY
    delete[] reqBuff;
  #endif
  return(state);
}

int16_t LR11x0::clearRxBuffer(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CLEAR_RX_BUFFER, true, NULL, 0));
}

int16_t LR11x0::writeRegMemMask32(uint32_t addr, uint32_t mask, uint32_t data) {
  uint8_t buff[12] = {
    (uint8_t)((addr >> 24) & 0xFF), (uint8_t)((addr >> 16) & 0xFF), (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF),
    (uint8_t)((mask >> 24) & 0xFF), (uint8_t)((mask >> 16) & 0xFF), (uint8_t)((mask >> 8) & 0xFF), (uint8_t)(mask & 0xFF),
    (uint8_t)((data >> 24) & 0xFF), (uint8_t)((data >> 16) & 0xFF), (uint8_t)((data >> 8) & 0xFF), (uint8_t)(data & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WRITE_REG_MEM_MASK, true, buff, sizeof(buff)));
}

int16_t LR11x0::getStatus(uint8_t* stat1, uint8_t* stat2, uint32_t* irq) {
  uint8_t buff[6] = { 0 };

  // the status check command doesn't return status in the same place as other read commands
  // but only as the first byte (as with any other command), hence LR11x0::SPIcommand can't be used
  // it also seems to ignore the actual command, and just sending in bunch of NOPs will work 
  int16_t state = this->mod->SPItransferStream(NULL, 0, false, NULL, buff, sizeof(buff), true, RADIOLIB_MODULE_SPI_TIMEOUT);

  // pass the replies
  if(stat1) { *stat1 = buff[0]; }
  if(stat2) { *stat2 = buff[1]; }
  if(irq)   { *irq = ((uint32_t)(buff[2]) << 24) | ((uint32_t)(buff[3]) << 16) | ((uint32_t)(buff[4]) << 8) | (uint32_t)buff[5]; }

  return(state);
}

int16_t LR11x0::getVersion(uint8_t* hw, uint8_t* device, uint8_t* major, uint8_t* minor) {
  uint8_t buff[4] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_VERSION, false, buff, sizeof(buff));

  // pass the replies
  if(hw)      { *hw = buff[0]; }
  if(device)  { *device = buff[1]; }
  if(major)   { *major = buff[2]; }
  if(minor)   { *minor = buff[3]; }

  return(state);
}

int16_t LR11x0::getErrors(uint16_t* err) {
  uint8_t buff[2] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_ERRORS, false, buff, sizeof(buff));

  // pass the replies
  if(err) { *err = ((uint16_t)(buff[0]) << 8) | (uint16_t)buff[1];  }

  return(state);
}

int16_t LR11x0::clearErrors(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CLEAR_ERRORS, true, NULL, 0));
}

int16_t LR11x0::calibrate(uint8_t params) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CALIBRATE, true, &params, 1));
}

int16_t LR11x0::setRegMode(uint8_t mode) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_REG_MODE, true, &mode, 1));
}

int16_t LR11x0::calibImage(float freq1, float freq2) {
  uint8_t buff[2] = {
    (uint8_t)floor((freq1 - 1.0f) / 4.0f),
    (uint8_t)ceil((freq2 + 1.0f) / 4.0f)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CALIB_IMAGE, true, buff, sizeof(buff)));
}

int16_t LR11x0::setDioAsRfSwitch(uint8_t en, uint8_t stbyCfg, uint8_t rxCfg, uint8_t txCfg, uint8_t txHpCfg, uint8_t gnssCfg, uint8_t wifiCfg) {
  uint8_t buff[7] = { en, stbyCfg, rxCfg, txCfg, txHpCfg, gnssCfg, wifiCfg };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_DIO_AS_RF_SWITCH, true, buff, sizeof(buff)));
}

int16_t LR11x0::setDioIrqParams(uint32_t irq1, uint32_t irq2) {
  uint8_t buff[8] = {
    (uint8_t)((irq1 >> 24) & 0xFF), (uint8_t)((irq1 >> 16) & 0xFF), (uint8_t)((irq1 >> 8) & 0xFF), (uint8_t)(irq1 & 0xFF),
    (uint8_t)((irq2 >> 24) & 0xFF), (uint8_t)((irq2 >> 16) & 0xFF), (uint8_t)((irq2 >> 8) & 0xFF), (uint8_t)(irq2 & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_DIO_IRQ_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::clearIrq(uint32_t irq) {
  uint8_t buff[4] = {
    (uint8_t)((irq >> 24) & 0xFF), (uint8_t)((irq >> 16) & 0xFF), (uint8_t)((irq >> 8) & 0xFF), (uint8_t)(irq & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CLEAR_IRQ, true, buff, sizeof(buff)));
}

int16_t LR11x0::configLfClock(uint8_t setup) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_REG_MODE, true, &setup, 1));
}

int16_t LR11x0::setTcxoMode(uint8_t tune, uint32_t delay) {
  uint8_t buff[4] = {
    tune, (uint8_t)((delay >> 16) & 0xFF), (uint8_t)((delay >> 8) & 0xFF), (uint8_t)(delay & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_TCXO_MODE, true, buff, sizeof(buff)));
}

int16_t LR11x0::reboot(bool stay) {
  uint8_t buff[1] = { (uint8_t)stay };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_REBOOT, true, buff, sizeof(buff)));
}

int16_t LR11x0::getVbat(float* vbat) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_VBAT, false, buff, sizeof(buff));

  // pass the replies
  if(vbat) { *vbat = (((float)buff[0]/51.0f) - 1.0f)*1.35f; }

  return(state);
}

int16_t LR11x0::getTemp(float* temp) {
  uint8_t buff[2] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_TEMP, false, buff, sizeof(buff));

  // pass the replies
  if(temp) {
    uint16_t raw = ((uint16_t)(buff[0]) << 8) | (uint16_t)buff[1];
    *temp = 25.0f - (1000.0f/1.7f)*(((float)raw/2047.0f)*1350.0f - 0.7295f);
  }

  return(state);
}

int16_t LR11x0::setFs(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_FS, true, NULL, 0));
}

int16_t LR11x0::getRandomNumber(uint32_t* rnd) {
  uint8_t buff[4] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_RANDOM_NUMBER, false, buff, sizeof(buff));

  // pass the replies
  if(rnd) { *rnd = ((uint32_t)(buff[0]) << 24) | ((uint32_t)(buff[1]) << 16) | ((uint32_t)(buff[2]) << 8) | (uint32_t)buff[3];  }

  return(state);
}

int16_t LR11x0::eraseInfoPage(void) {
  // only page 1 can be erased
  uint8_t buff[1] = { RADIOLIB_LR11X0_INFO_PAGE };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_ERASE_INFO_PAGE, true, buff, sizeof(buff)));
}

int16_t LR11x0::writeInfoPage(uint16_t addr, uint32_t* data, size_t len) {
  // check maximum size
  if(len > (RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN/sizeof(uint32_t))) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }

  // build buffers - later we need to ensure endians are correct, 
  // so there is probably no way to do this without copying buffers and iterating
  size_t buffLen = sizeof(uint8_t) + sizeof(uint16_t) + len*sizeof(uint32_t);
  #if RADIOLIB_STATIC_ONLY
    uint8_t dataBuff[sizeof(uint8_t) + sizeof(uint16_t) + RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* dataBuff = new uint8_t[buffLen];
  #endif

  // set the address
  dataBuff[0] = RADIOLIB_LR11X0_INFO_PAGE;
  dataBuff[1] = (uint8_t)((addr >> 8) & 0xFF);
  dataBuff[2] = (uint8_t)(addr & 0xFF);

  // convert endians
  for(size_t i = 0; i < len; i++) {
    dataBuff[3 + i] = (uint8_t)((data[i] >> 24) & 0xFF);
    dataBuff[4 + i] = (uint8_t)((data[i] >> 16) & 0xFF);
    dataBuff[5 + i] = (uint8_t)((data[i] >> 8) & 0xFF);
    dataBuff[6 + i] = (uint8_t)(data[i] & 0xFF);
  }

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_WRITE_INFO_PAGE, true, dataBuff, buffLen);
  #if RADIOLIB_STATIC_ONLY
    delete[] dataBuff;
  #endif
  return(state);
}

int16_t LR11x0::readInfoPage(uint16_t addr, uint32_t* data, size_t len) {
  // check maximum size
  if(len > (RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN/sizeof(uint32_t))) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }

  // the request contains the address and length
  uint8_t reqBuff[4] = {
    RADIOLIB_LR11X0_INFO_PAGE,
    (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF),
    (uint8_t)len,
  };

  // build buffers - later we need to ensure endians are correct, 
  // so there is probably no way to do this without copying buffers and iterating
  #if RADIOLIB_STATIC_ONLY
    uint8_t rplBuff[RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* rplBuff = new uint8_t[len*sizeof(uint32_t)];
  #endif

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_READ_INFO_PAGE, false, rplBuff, len*sizeof(uint32_t), reqBuff, sizeof(reqBuff));

  // convert endians
  if(data && (state == RADIOLIB_ERR_NONE)) {
    for(size_t i = 0; i < len; i++) {
      data[i] = ((uint32_t)rplBuff[2 + i*sizeof(uint32_t)] << 24) | ((uint32_t)rplBuff[3 + i*sizeof(uint32_t)] << 16) | ((uint32_t)rplBuff[4 + i*sizeof(uint32_t)] << 8) | (uint32_t)rplBuff[5 + i*sizeof(uint32_t)];
    }
  }
  
  #if !RADIOLIB_STATIC_ONLY
    delete[] rplBuff;
  #endif
  
  return(state);
}

int16_t LR11x0::getChipEui(uint8_t* eui) {
  if(!eui) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_CHIP_EUI, false, eui, RADIOLIB_LR11X0_EUI_LEN));
}

int16_t LR11x0::getSemtechJoinEui(uint8_t* eui) {
  if(!eui) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_SEMTECH_JOIN_EUI, false, eui, RADIOLIB_LR11X0_EUI_LEN));
}

int16_t LR11x0::deriveRootKeysAndGetPin(uint8_t* pin) {
  if(!pin) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_DERIVE_ROOT_KEYS_AND_GET_PIN, false, pin, RADIOLIB_LR11X0_PIN_LEN));
}

int16_t LR11x0::enableSpiCrc(bool en) {
  // TODO implement this
  (void)en;
  // LR11X0 CRC is gen 0xA6 (0x65 but reflected), init 0xFF, input and result reflected
  /*RadioLibCRCInstance.size = 8;
  RadioLibCRCInstance.poly = 0xA6;
  RadioLibCRCInstance.init = 0xFF;
  RadioLibCRCInstance.out = 0x00;
  RadioLibCRCInstance.refIn = true;
  RadioLibCRCInstance.refOut = true;*/
  return(RADIOLIB_ERR_UNSUPPORTED);
}

int16_t LR11x0::driveDiosInSleepMode(bool en) {
  uint8_t buff[1] = { (uint8_t)en };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_DRIVE_DIOS_IN_SLEEP_MODE, true, buff, sizeof(buff)));
}

int16_t LR11x0::resetStats(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_RESET_STATS, true, NULL, 0));
}

int16_t LR11x0::getStats(uint16_t* nbPktReceived, uint16_t* nbPktCrcError, uint16_t* data1, uint16_t* data2) {
  uint8_t buff[8] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_STATS, false, buff, sizeof(buff));

  // pass the replies
  if(nbPktReceived) { *nbPktReceived = ((uint16_t)(buff[0]) << 8) | (uint16_t)buff[1]; }
  if(nbPktCrcError) { *nbPktCrcError = ((uint16_t)(buff[2]) << 8) | (uint16_t)buff[3]; }
  if(data1) { *data1 = ((uint16_t)(buff[4]) << 8) | (uint16_t)buff[5]; }
  if(data2) { *data2 = ((uint16_t)(buff[6]) << 8) | (uint16_t)buff[7]; }

  return(state);
}

int16_t LR11x0::getPacketType(uint8_t* type) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_PACKET_TYPE, false, buff, sizeof(buff));

  // pass the replies
  if(type) { *type = buff[0]; }

  return(state);
}

int16_t LR11x0::getRxBufferStatus(uint8_t* len, uint8_t* startOffset) {
  uint8_t buff[2] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_RX_BUFFER_STATUS, false, buff, sizeof(buff));

  // pass the replies
  if(len) { *len = buff[0]; }
  if(startOffset) { *startOffset = buff[1]; }

  return(state);
}

int16_t LR11x0::getPacketStatusLoRa(float* rssiPkt, float* snrPkt, float* signalRssiPkt) {
  uint8_t buff[3] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_PACKET_STATUS, false, buff, sizeof(buff));

  // pass the replies
  if(rssiPkt) { *rssiPkt = (float)buff[0] / -2.0f; }
  if(snrPkt) { *snrPkt = (float)buff[1] / 4.0f; }
  if(signalRssiPkt) { *signalRssiPkt = buff[2]; }

  return(state);
}

int16_t LR11x0::getPacketStatusGFSK(float* rssiSync, float* rssiAvg, uint8_t* rxLen, uint8_t* stat) {
  uint8_t buff[4] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_PACKET_STATUS, false, buff, sizeof(buff));

  // pass the replies
  // TODO do the value conversion for RSSI (fixed point?)
  if(rssiSync) { *rssiSync = (float)buff[0]; }
  if(rssiAvg) { *rssiAvg = (float)buff[1]; }
  if(rxLen) { *rxLen = buff[2]; }
  if(stat) { *stat = buff[3]; }

  return(state);
}

int16_t LR11x0::getRssiInst(float* rssi) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_RSSI_INST, false, buff, sizeof(buff));

  // pass the replies
  if(rssi) { *rssi = (float)buff[0] / -2.0f; }

  return(state);
}

int16_t LR11x0::setGfskSyncWord(uint8_t* sync) {
  if(!sync) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_GFSK_SYNC_WORD, false, sync, RADIOLIB_LR11X0_GFSK_SYNC_WORD_LEN));
}

int16_t LR11x0::setLoRaPublicNetwork(bool pub) {
  uint8_t buff[1] = { (uint8_t)pub };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_LORA_PUBLIC_NETWORK, true, buff, sizeof(buff)));
}

int16_t LR11x0::setRx(uint32_t timeout) {
  uint8_t buff[3] = {
    (uint8_t)((timeout >> 16) & 0xFF), (uint8_t)((timeout >> 8) & 0xFF), (uint8_t)(timeout & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RX, true, buff, sizeof(buff)));
}

int16_t LR11x0::setTx(uint32_t timeout) {
  uint8_t buff[3] = {
    (uint8_t)((timeout >> 16) & 0xFF), (uint8_t)((timeout >> 8) & 0xFF), (uint8_t)(timeout & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_TX, true, buff, sizeof(buff)));
}

int16_t LR11x0::setRfFrequency(uint32_t rfFreq) {
  uint8_t buff[4] = {
    (uint8_t)((rfFreq >> 24) & 0xFF), (uint8_t)((rfFreq >> 16) & 0xFF),
    (uint8_t)((rfFreq >> 8) & 0xFF), (uint8_t)(rfFreq & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RF_FREQUENCY, true, buff, sizeof(buff)));
}

int16_t LR11x0::autoTxRx(uint32_t delay, uint8_t intMode, uint32_t timeout) {
  uint8_t buff[7] = {
    (uint8_t)((delay >> 16) & 0xFF), (uint8_t)((delay >> 8) & 0xFF), (uint8_t)(delay & 0xFF), intMode,
    (uint8_t)((timeout >> 16) & 0xFF), (uint8_t)((timeout >> 8) & 0xFF), (uint8_t)(timeout & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_AUTO_TX_RX, true, buff, sizeof(buff)));
}

int16_t LR11x0::setCadParams(uint8_t symNum, uint8_t detPeak, uint8_t detMin, uint8_t cadExitMode, uint32_t timeout) {
  uint8_t buff[7] = {
    symNum, detPeak, detMin, cadExitMode,
    (uint8_t)((timeout >> 16) & 0xFF), (uint8_t)((timeout >> 8) & 0xFF), (uint8_t)(timeout & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_CAD_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setPacketType(uint8_t type) {
  uint8_t buff[1] = { type };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_PACKET_TYPE, true, buff, sizeof(buff)));
}

int16_t LR11x0::setModulationParamsLoRa(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t ldro) {
  uint8_t buff[4] = { sf, bw, cr, ldro };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_MODULATION_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setModulationParamsGFSK(uint32_t br, uint8_t sh, uint8_t rxBw, uint32_t freqDev) {
  uint8_t buff[10] = { 
    (uint8_t)((br >> 24) & 0xFF), (uint8_t)((br >> 16) & 0xFF),
    (uint8_t)((br >> 8) & 0xFF), (uint8_t)(br & 0xFF), sh, rxBw,
    (uint8_t)((freqDev >> 24) & 0xFF), (uint8_t)((freqDev >> 16) & 0xFF),
    (uint8_t)((freqDev >> 8) & 0xFF), (uint8_t)(freqDev & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_MODULATION_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setModulationParamsLrFhss(uint32_t br, uint8_t sh) {
  uint8_t buff[5] = { 
    (uint8_t)((br >> 24) & 0xFF), (uint8_t)((br >> 16) & 0xFF),
    (uint8_t)((br >> 8) & 0xFF), (uint8_t)(br & 0xFF), sh
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_MODULATION_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setModulationParamsSigfox(uint32_t br, uint8_t sh) {
  // same as for LR-FHSS
  return(this->setModulationParamsLrFhss(br, sh));
}

int16_t LR11x0::setPacketParamsLoRa(uint16_t preambleLen, uint8_t hdrType, uint8_t payloadLen, uint8_t crcType, uint8_t invertIQ) {
  uint8_t buff[6] = { 
    (uint8_t)((preambleLen >> 8) & 0xFF), (uint8_t)(preambleLen & 0xFF),
    hdrType, payloadLen, crcType, invertIQ
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_PACKET_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setPacketParamsGFSK(uint16_t preambleLen, uint8_t preambleDetectorLen, uint8_t syncWordLen, uint8_t addrCmp, uint8_t packType, uint8_t payloadLen, uint8_t crcType, uint8_t whiten) {
  uint8_t buff[9] = { 
    (uint8_t)((preambleLen >> 8) & 0xFF), (uint8_t)(preambleLen & 0xFF),
    preambleDetectorLen, syncWordLen, addrCmp, packType, payloadLen, crcType, whiten
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_PACKET_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setPacketParamsSigfox(uint8_t payloadLen, uint16_t rampUpDelay, uint16_t rampDownDelay, uint16_t bitNum) {
  uint8_t buff[7] = { 
    payloadLen, (uint8_t)((rampUpDelay >> 8) & 0xFF), (uint8_t)(rampUpDelay & 0xFF),
    (uint8_t)((rampDownDelay >> 8) & 0xFF), (uint8_t)(rampDownDelay & 0xFF),
    (uint8_t)((bitNum >> 8) & 0xFF), (uint8_t)(bitNum & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_PACKET_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setTxParams(int8_t pwr, uint8_t ramp) {
  uint8_t buff[2] = { (uint8_t)pwr, ramp };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_TX_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setPacketAdrs(uint8_t node, uint8_t broadcast) {
  uint8_t buff[2] = { node, broadcast };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_PACKET_ADRS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setRxTxFallbackMode(uint8_t mode) {
  uint8_t buff[1] = { mode };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RX_TX_FALLBACK_MODE, true, buff, sizeof(buff)));
}

int16_t LR11x0::setRxDutyCycle(uint32_t rxPeriod, uint32_t sleepPeriod, uint8_t mode) {
  uint8_t buff[7] = {
    (uint8_t)((rxPeriod >> 16) & 0xFF), (uint8_t)((rxPeriod >> 8) & 0xFF), (uint8_t)(rxPeriod & 0xFF),
    (uint8_t)((sleepPeriod >> 16) & 0xFF), (uint8_t)((sleepPeriod >> 8) & 0xFF), (uint8_t)(sleepPeriod & 0xFF),
    mode
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RX_DUTY_CYCLE, true, buff, sizeof(buff)));
}

int16_t LR11x0::setPaConfig(uint8_t paSel, uint8_t regPaSupply, uint8_t paDutyCycle, uint8_t paHpSel) {
  uint8_t buff[4] = { paSel, regPaSupply, paDutyCycle, paHpSel };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_PA_CONFIG, true, buff, sizeof(buff)));
}

int16_t LR11x0::stopTimeoutOnPreamble(bool stop) {
  uint8_t buff[1] = { (uint8_t)stop };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_STOP_TIMEOUT_ON_PREAMBLE, true, buff, sizeof(buff)));
}

int16_t LR11x0::setCad(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_CAD, true, NULL, 0));
}

int16_t LR11x0::setTxCw(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_TX_CW, true, NULL, 0));
}

int16_t LR11x0::setTxInfinitePreamble(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_TX_INFINITE_PREAMBLE, true, NULL, 0));
}

int16_t LR11x0::setLoRaSynchTimeout(uint8_t symbolNum) {
  uint8_t buff[1] = { symbolNum };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_LORA_SYNCH_TIMEOUT, true, buff, sizeof(buff)));
}

int16_t LR11x0::setRangingAddr(uint32_t addr, uint8_t checkLen) {
  uint8_t buff[5] = {
    (uint8_t)((addr >> 24) & 0xFF), (uint8_t)((addr >> 16) & 0xFF),
    (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF), checkLen
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RANGING_ADDR, true, buff, sizeof(buff)));
}

int16_t LR11x0::setRangingReqAddr(uint32_t addr) {
  uint8_t buff[4] = {
    (uint8_t)((addr >> 24) & 0xFF), (uint8_t)((addr >> 16) & 0xFF),
    (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RANGING_REQ_ADDR, true, buff, sizeof(buff)));
}

int16_t LR11x0::getRangingResult(uint8_t type, float* res) {
  uint8_t reqBuff[1] = { type };
  uint8_t rplBuff[4] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_NOP, false, rplBuff, sizeof(rplBuff), reqBuff, sizeof(reqBuff));
  RADIOLIB_ASSERT(state);

  if(res) { 
    if(type == RADIOLIB_LR11X0_RANGING_RESULT_DISTANCE) {
      uint32_t raw = ((uint32_t)(rplBuff[0]) << 24) | ((uint32_t)(rplBuff[1]) << 16) | ((uint32_t)(rplBuff[2]) << 8) | (uint32_t)rplBuff[3];
      *res = ((float)(raw*3e8))/((float)(4096*this->bandwidthKhz*1000));
    } else {
      *res = (float)rplBuff[3]/2.0f;
    }
  }

  return(state);
}

int16_t LR11x0::setRangingTxRxDelay(uint32_t delay) {
  uint8_t buff[4] = {
    (uint8_t)((delay >> 24) & 0xFF), (uint8_t)((delay >> 16) & 0xFF),
    (uint8_t)((delay >> 8) & 0xFF), (uint8_t)(delay & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RANGING_TX_RX_DELAY, true, buff, sizeof(buff)));
}

int16_t LR11x0::setGfskCrcParams(uint32_t init, uint32_t poly) {
  uint8_t buff[8] = {
    (uint8_t)((init >> 24) & 0xFF), (uint8_t)((init >> 16) & 0xFF),
    (uint8_t)((init >> 8) & 0xFF), (uint8_t)(init & 0xFF),
    (uint8_t)((poly >> 24) & 0xFF), (uint8_t)((poly >> 16) & 0xFF),
    (uint8_t)((poly >> 8) & 0xFF), (uint8_t)(poly & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_GFSK_CRC_PARAMS, true, buff, sizeof(buff)));
  
}

int16_t LR11x0::setGfskWhitParams(uint16_t seed) {
  uint8_t buff[2] = {
    (uint8_t)((seed >> 8) & 0xFF), (uint8_t)(seed & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_GFSK_WHIT_PARAMS, true, buff, sizeof(buff)));
}

int16_t LR11x0::setRxBoosted(bool en) {
  uint8_t buff[1] = { (uint8_t)en };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RX_BOOSTED, true, buff, sizeof(buff)));
}

int16_t LR11x0::setRangingParameter(uint8_t symbolNum) {
  // the first byte is reserved
  uint8_t buff[2] = { 0x00, symbolNum };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_RANGING_PARAMETER, true, buff, sizeof(buff)));
}

int16_t LR11x0::setLoRaSyncWord(uint8_t sync) {
  uint8_t buff[1] = { sync };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_SET_LORA_SYNC_WORD, true, buff, sizeof(buff)));
}

int16_t LR11x0::lrFhssBuildFrame(uint8_t hdrCount, uint8_t cr, uint8_t grid, bool hop, uint8_t bw, uint16_t hopSeq, int8_t devOffset, uint8_t* payload, size_t len) {
  // check maximum size
  const uint8_t maxLen[4][4] = {
    { 189, 178, 167, 155, },
    { 151, 142, 133, 123, },
    { 112, 105,  99,  92, },
    {  74,  69,  65,  60, },
  };
  if((cr > RADIOLIB_LR11X0_LR_FHSS_CR_1_3) || ((hdrCount - 1) > (int)sizeof(maxLen[0])) || (len > maxLen[cr][hdrCount - 1])) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }

  // build buffers
  size_t buffLen = 9 + len;
  #if RADIOLIB_STATIC_ONLY
    uint8_t dataBuff[9 + 190];
  #else
    uint8_t* dataBuff = new uint8_t[buffLen];
  #endif

  // set properties of the packet
  dataBuff[0] = hdrCount;
  dataBuff[1] = cr;
  dataBuff[2] = RADIOLIB_LR11X0_LR_FHSS_MOD_TYPE_GMSK;
  dataBuff[3] = grid;
  dataBuff[4] = (uint8_t)hop;
  dataBuff[5] = bw;
  dataBuff[6] = (uint8_t)((hopSeq >> 8) & 0x01);
  dataBuff[7] = (uint8_t)(hopSeq & 0xFF);
  dataBuff[8] = devOffset;
  memcpy(&dataBuff[9], payload, len);

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_LR_FHSS_BUILD_FRAME, true, dataBuff, buffLen);
  #if RADIOLIB_STATIC_ONLY
    delete[] dataBuff;
  #endif
  return(state);
}

int16_t LR11x0::lrFhssSetSyncWord(uint32_t sync) {
  uint8_t buff[4] = {
    (uint8_t)((sync >> 24) & 0xFF), (uint8_t)((sync >> 16) & 0xFF),
    (uint8_t)((sync >> 8) & 0xFF), (uint8_t)(sync & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_LR_FHSS_SET_SYNC_WORD, true, buff, sizeof(buff)));
}

int16_t LR11x0::configBleBeacon(uint8_t chan, uint8_t* payload, size_t len) {
  return(this->bleBeaconCommon(RADIOLIB_LR11X0_CMD_CONFIG_BLE_BEACON, chan, payload, len));
}

int16_t LR11x0::getLoRaRxHeaderInfos(uint8_t* info) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GET_LORA_RX_HEADER_INFOS, false, buff, sizeof(buff));

  // pass the replies
  if(info) { *info = buff[0]; }

  return(state);
}

int16_t LR11x0::bleBeaconSend(uint8_t chan, uint8_t* payload, size_t len) {
  return(this->bleBeaconCommon(RADIOLIB_LR11X0_CMD_BLE_BEACON_SEND, chan, payload, len));
}

int16_t LR11x0::bleBeaconCommon(uint16_t cmd, uint8_t chan, uint8_t* payload, size_t len) {
  // check maximum size
  // TODO what is the actual maximum?
  if(len > RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }

  // build buffers
  #if RADIOLIB_STATIC_ONLY
    uint8_t dataBuff[sizeof(uint8_t) + RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* dataBuff = new uint8_t[sizeof(uint8_t) + len];
  #endif

  // set the channel
  dataBuff[0] = chan;
  memcpy(&dataBuff[1], payload, len);

  int16_t state = this->SPIcommand(cmd, true, dataBuff, sizeof(uint8_t) + len);
  #if RADIOLIB_STATIC_ONLY
    delete[] dataBuff;
  #endif
  return(state);
}

int16_t LR11x0::wifiScan(uint8_t type, uint16_t mask, uint8_t acqMode, uint8_t nbMaxRes, uint8_t nbScanPerChan, uint16_t timeout, uint8_t abortOnTimeout) {
  uint8_t buff[9] = {
    type, (uint8_t)((mask >> 8) & 0xFF), (uint8_t)(mask & 0xFF),
    acqMode, nbMaxRes, nbScanPerChan,
    (uint8_t)((timeout >> 8) & 0xFF), (uint8_t)(timeout & 0xFF),
    abortOnTimeout
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_SCAN, true, buff, sizeof(buff)));
}

int16_t LR11x0::wifiScanTimeLimit(uint8_t type, uint16_t mask, uint8_t acqMode, uint8_t nbMaxRes, uint16_t timePerChan, uint16_t timeout) {
  uint8_t buff[9] = {
    type, (uint8_t)((mask >> 8) & 0xFF), (uint8_t)(mask & 0xFF),
    acqMode, nbMaxRes,
    (uint8_t)((timePerChan >> 8) & 0xFF), (uint8_t)(timePerChan & 0xFF),
    (uint8_t)((timeout >> 8) & 0xFF), (uint8_t)(timeout & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_SCAN_TIME_LIMIT, true, buff, sizeof(buff)));
}

int16_t LR11x0::wifiCountryCode(uint16_t mask, uint8_t nbMaxRes, uint8_t nbScanPerChan, uint16_t timeout, uint8_t abortOnTimeout) {
  uint8_t buff[7] = {
    (uint8_t)((mask >> 8) & 0xFF), (uint8_t)(mask & 0xFF),
    nbMaxRes, nbScanPerChan,
    (uint8_t)((timeout >> 8) & 0xFF), (uint8_t)(timeout & 0xFF),
    abortOnTimeout
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_COUNTRY_CODE, true, buff, sizeof(buff)));
}

int16_t LR11x0::wifiCountryCodeTimeLimit(uint16_t mask, uint8_t nbMaxRes, uint16_t timePerChan, uint16_t timeout) {
  uint8_t buff[7] = {
    (uint8_t)((mask >> 8) & 0xFF), (uint8_t)(mask & 0xFF),
    nbMaxRes,
    (uint8_t)((timePerChan >> 8) & 0xFF), (uint8_t)(timePerChan & 0xFF),
    (uint8_t)((timeout >> 8) & 0xFF), (uint8_t)(timeout & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_COUNTRY_CODE_TIME_LIMIT, true, buff, sizeof(buff)));
}

int16_t LR11x0::wifiGetNbResults(uint8_t* nbResults) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_GET_NB_RESULTS, false, buff, sizeof(buff));

  // pass the replies
  if(nbResults) { *nbResults = buff[0]; }

  return(state);
}

int16_t LR11x0::wifiReadResults(uint8_t index, uint8_t nbResults, uint8_t format, uint8_t* results) {
  uint8_t reqBuff[3] = { index, nbResults, format };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_READ_RESULTS, false, results, nbResults, reqBuff, sizeof(reqBuff)));
}

int16_t LR11x0::wifiResetCumulTimings(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_RESET_CUMUL_TIMINGS, true, NULL, 0));
}

int16_t LR11x0::wifiReadCumulTimings(uint32_t* detection, uint32_t* capture, uint32_t* demodulation) {
  uint8_t buff[16] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_READ_CUMUL_TIMINGS, false, buff, sizeof(buff));

  // pass the replies
  if(detection) { *detection = ((uint32_t)(buff[4]) << 24) | ((uint32_t)(buff[5]) << 16) | ((uint32_t)(buff[6]) << 8) | (uint32_t)buff[7]; }
  if(capture) { *capture = ((uint32_t)(buff[8]) << 24) | ((uint32_t)(buff[9]) << 16) | ((uint32_t)(buff[10]) << 8) | (uint32_t)buff[11]; }
  if(demodulation) { *demodulation = ((uint32_t)(buff[12]) << 24) | ((uint32_t)(buff[13]) << 16) | ((uint32_t)(buff[14]) << 8) | (uint32_t)buff[15]; }

  return(state);
}

int16_t LR11x0::wifiGetNbCountryCodeResults(uint8_t* nbResults) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_GET_NB_COUNTRY_CODE_RESULTS, false, buff, sizeof(buff));

  // pass the replies
  if(nbResults) { *nbResults = buff[0]; }

  return(state);
}

int16_t LR11x0::wifiReadCountryCodeResults(uint8_t index, uint8_t nbResults, uint8_t* results) {
  uint8_t reqBuff[2] = { index, nbResults };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_READ_COUNTRY_CODE_RESULTS, false, results, nbResults, reqBuff, sizeof(reqBuff)));
}

int16_t LR11x0::wifiCfgTimestampAPphone(uint32_t timestamp) {
  uint8_t buff[4] = {
    (uint8_t)((timestamp >> 24) & 0xFF), (uint8_t)((timestamp >> 16) & 0xFF),
    (uint8_t)((timestamp >> 8) & 0xFF), (uint8_t)(timestamp & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_COUNTRY_CODE_TIME_LIMIT, true, buff, sizeof(buff)));
}

int16_t LR11x0::wifiReadVersion(uint8_t* major, uint8_t* minor) {
  uint8_t buff[2] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_WIFI_READ_VERSION, false, buff, sizeof(buff));

  // pass the replies
  if(major) { *major = buff[0]; }
  if(minor) { *minor = buff[1]; }

  return(state);
}

int16_t LR11x0::gnssSetConstellationToUse(uint8_t mask) {
  uint8_t buff[1] = { mask };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_SET_CONSTELLATION_TO_USE, true, buff, sizeof(buff)));
}

int16_t LR11x0::gnssReadConstellationToUse(uint8_t* mask) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_READ_CONSTELLATION_TO_USE, false, buff, sizeof(buff));

  // pass the replies
  if(mask) { *mask = buff[0]; }

  return(state);
}

int16_t LR11x0::gnssSetAlmanacUpdate(uint8_t mask) {
  uint8_t buff[1] = { mask };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_SET_ALMANAC_UPDATE, true, buff, sizeof(buff)));
}

int16_t LR11x0::gnssReadAlmanacUpdate(uint8_t* mask) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_READ_ALMANAC_UPDATE, false, buff, sizeof(buff));

  // pass the replies
  if(mask) { *mask = buff[0]; }

  return(state);
}

int16_t LR11x0::gnssReadVersion(uint8_t* fw, uint8_t* almanac) {
  uint8_t buff[2] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_READ_VERSION, false, buff, sizeof(buff));

  // pass the replies
  if(fw) { *fw = buff[0]; }
  if(almanac) { *almanac = buff[1]; }

  return(state);
}

int16_t LR11x0::gnssReadSupportedConstellations(uint8_t* mask) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_READ_SUPPORTED_CONSTELLATIONS, false, buff, sizeof(buff));

  // pass the replies
  if(mask) { *mask = buff[0]; }

  return(state);
}

int16_t LR11x0::gnssSetMode(uint8_t mode) {
  uint8_t buff[1] = { mode };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_SET_MODE, true, buff, sizeof(buff)));
}

int16_t LR11x0::gnssAutonomous(uint32_t gpsTime, uint8_t resMask, uint8_t nbSvMask) {
  uint8_t buff[7] = {
    (uint8_t)((gpsTime >> 24) & 0xFF), (uint8_t)((gpsTime >> 16) & 0xFF),
    (uint8_t)((gpsTime >> 8) & 0xFF), (uint8_t)(gpsTime & 0xFF),
    RADIOLIB_LR11X0_GNSS_AUTO_EFFORT_MODE, resMask, nbSvMask
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_AUTONOMOUS, true, buff, sizeof(buff)));
}

int16_t LR11x0::gnssAssisted(uint32_t gpsTime, uint8_t effort, uint8_t resMask, uint8_t nbSvMask) {
  uint8_t buff[7] = {
    (uint8_t)((gpsTime >> 24) & 0xFF), (uint8_t)((gpsTime >> 16) & 0xFF),
    (uint8_t)((gpsTime >> 8) & 0xFF), (uint8_t)(gpsTime & 0xFF),
    effort, resMask, nbSvMask
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_ASSISTED, true, buff, sizeof(buff)));
}

int16_t LR11x0::gnssSetAssistancePosition(float lat, float lon) {
  uint16_t latRaw = (lat*2048.0f)/90.0f + 0.5f;
  uint16_t lonRaw = (lon*2048.0f)/180.0f + 0.5f;
  uint8_t buff[4] = {
    (uint8_t)((latRaw >> 8) & 0xFF), (uint8_t)(latRaw & 0xFF),
    (uint8_t)((lonRaw >> 8) & 0xFF), (uint8_t)(lonRaw & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_SET_ASSISTANCE_POSITION, true, buff, sizeof(buff)));
}

int16_t LR11x0::gnssReadAssistancePosition(float* lat, float* lon) {
  uint8_t buff[4] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_READ_ASSISTANCE_POSITION, false, buff, sizeof(buff));

  // pass the replies
  if(lat) {
    uint16_t latRaw = ((uint16_t)(buff[0]) << 8) | (uint16_t)(buff[1]);
    *lat = ((float)latRaw*90.0f)/2048.0f;
  }
  if(lon) {
    uint16_t lonRaw = ((uint16_t)(buff[2]) << 8) | (uint16_t)(buff[3]);
    *lon = ((float)lonRaw*180.0f)/2048.0f;
  }

  return(state);
}

int16_t LR11x0::gnssPushSolverMsg(uint8_t* payload, size_t len) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_PUSH_SOLVER_MSG, true, payload, len));
}

int16_t LR11x0::gnssPushDmMsg(uint8_t* payload, size_t len) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_PUSH_DM_MSG, true, payload, len));
}

int16_t LR11x0::gnssGetContextStatus(uint8_t* fwVersion, uint32_t* almanacCrc, uint8_t* errCode, uint8_t* almUpdMask, uint8_t* freqSpace) {
  // send the command
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_GET_CONTEXT_STATUS, true, NULL, 0);
  RADIOLIB_ASSERT(state);

  // read the result - this requires some magic bytes first, that's why LR11x0::SPIcommand cannot be used
  uint8_t cmd_buff[3] = { 0x00, 0x02, 0x18 };
  uint8_t buff[9] = { 0 };
  state = this->mod->SPItransferStream(cmd_buff, sizeof(cmd_buff), false, NULL, buff, sizeof(buff), true, RADIOLIB_MODULE_SPI_TIMEOUT);

  // pass the replies
  if(fwVersion) { *fwVersion = buff[0]; }
  if(almanacCrc) { *almanacCrc = ((uint32_t)(buff[1]) << 24) | ((uint32_t)(buff[2]) << 16) | ((uint32_t)(buff[3]) << 8) | (uint32_t)buff[4]; }
  if(errCode) { *errCode = (buff[5] & 0xF0) >> 4; }
  if(almUpdMask) { *almUpdMask = (buff[5] & 0x0E) >> 1; }
  if(freqSpace) { *freqSpace = ((buff[5] & 0x01) << 1) | ((buff[6] & 0x80) >> 7); }

  return(state);
}

int16_t LR11x0::gnssGetNbSvDetected(uint8_t* nbSv) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_GET_NB_SV_DETECTED, false, buff, sizeof(buff));

  // pass the replies
  if(nbSv) { *nbSv = buff[0]; }

  return(state);
}

int16_t LR11x0::gnssGetSvDetected(uint8_t* svId, uint8_t* snr, uint16_t* doppler, size_t nbSv) {
  // TODO this is arbitrary - is there an actual maximum?
  if(nbSv > RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN/sizeof(uint32_t)) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }

  // build buffers
  size_t buffLen = nbSv*sizeof(uint32_t);
  #if RADIOLIB_STATIC_ONLY
    uint8_t dataBuff[RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* dataBuff = new uint8_t[buffLen];
  #endif

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_GET_SV_DETECTED, false, dataBuff, buffLen);
  if(state == RADIOLIB_ERR_NONE) {
    for(size_t i = 0; i < nbSv; i++) {
      if(svId) { svId[i] = dataBuff[4*i]; }
      if(snr) { snr[i] = dataBuff[4*i + 1]; }
      if(doppler) { doppler[i] = ((uint16_t)(dataBuff[4*i + 2]) << 8) | (uint16_t)dataBuff[4*i + 3]; }
    }
  }

  #if RADIOLIB_STATIC_ONLY
    delete[] dataBuff;
  #endif
  return(state);
}

int16_t LR11x0::gnssGetConsumption(uint32_t* cpu, uint32_t* radio) {
  uint8_t buff[8] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_GET_CONSUMPTION, false, buff, sizeof(buff));

  // pass the replies
  if(cpu) { *cpu = ((uint32_t)(buff[0]) << 24) | ((uint32_t)(buff[1]) << 16) | ((uint32_t)(buff[2]) << 8) | (uint32_t)buff[3]; }
  if(radio) { *radio = ((uint32_t)(buff[4]) << 24) | ((uint32_t)(buff[5]) << 16) | ((uint32_t)(buff[6]) << 8) | (uint32_t)buff[7]; }

  return(state);
}

int16_t LR11x0::gnssGetResultSize(uint16_t* size) {
  uint8_t buff[2] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_GET_RESULT_SIZE, false, buff, sizeof(buff));

  // pass the replies
  if(size) { *size = ((uint16_t)(buff[0]) << 8) | (uint16_t)buff[1]; }
  
  return(state);
}

int16_t LR11x0::gnssReadResults(uint8_t* result, uint16_t size) {
  if(!result) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_READ_RESULTS, false, result, size));
}

int16_t LR11x0::gnssAlmanacFullUpdateHeader(uint16_t date, uint32_t globalCrc) {
  uint8_t buff[RADIOLIB_LR11X0_GNSS_ALMANAC_BLOCK_SIZE] = {
    RADIOLIB_LR11X0_GNSS_ALMANAC_HEADER_ID,
    (uint8_t)((date >> 8) & 0xFF), (uint8_t)(date & 0xFF),
    (uint8_t)((globalCrc >> 24) & 0xFF), (uint8_t)((globalCrc >> 16) & 0xFF), 
    (uint8_t)((globalCrc >> 8) & 0xFF), (uint8_t)(globalCrc & 0xFF),
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_ALMANAC_FULL_UPDATE, true, buff, sizeof(buff)));
}

int16_t LR11x0::gnssAlmanacFullUpdateSV(uint8_t svn, uint8_t* svnAlmanac) {
  uint8_t buff[RADIOLIB_LR11X0_GNSS_ALMANAC_BLOCK_SIZE] = { svn };
  memcpy(&buff[1], svnAlmanac, RADIOLIB_LR11X0_GNSS_ALMANAC_BLOCK_SIZE - 1);
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_ALMANAC_FULL_UPDATE, true, buff, sizeof(buff)));
}

int16_t LR11x0::gnssGetSvVisible(uint32_t time, float lat, float lon, uint8_t constellation, uint8_t* nbSv) {
  uint16_t latRaw = (lat*2048.0f)/90.0f + 0.5f;
  uint16_t lonRaw = (lon*2048.0f)/180.0f + 0.5f;
  uint8_t reqBuff[9] = { 
    (uint8_t)((time >> 24) & 0xFF), (uint8_t)((time >> 16) & 0xFF),
    (uint8_t)((time >> 8) & 0xFF), (uint8_t)(time & 0xFF),
    (uint8_t)((latRaw >> 8) & 0xFF), (uint8_t)(latRaw & 0xFF),
    (uint8_t)((lonRaw >> 8) & 0xFF), (uint8_t)(lonRaw & 0xFF),
    constellation,
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_GNSS_GET_SV_VISIBLE, false, nbSv, 1, reqBuff, sizeof(reqBuff)));
}

int16_t LR11x0::cryptoSetKey(uint8_t keyId, uint8_t* key) {
  if(!key) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  uint8_t buff[1 + RADIOLIB_AES128_KEY_SIZE] = { 0 };
  buff[0] = keyId;
  memcpy(&buff[1], key, RADIOLIB_AES128_KEY_SIZE);
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_SET_KEY, false, buff, sizeof(buff)));
}

int16_t LR11x0::cryptoDeriveKey(uint8_t srcKeyId, uint8_t dstKeyId, uint8_t* key) {
  if(!key) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  uint8_t buff[2 + RADIOLIB_AES128_KEY_SIZE] = { 0 };
  buff[0] = srcKeyId;
  buff[1] = dstKeyId;
  memcpy(&buff[2], key, RADIOLIB_AES128_KEY_SIZE);
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_DERIVE_KEY, false, buff, sizeof(buff)));
}

int16_t LR11x0::cryptoProcessJoinAccept(uint8_t decKeyId, uint8_t verKeyId, uint8_t lwVer, uint8_t* header, uint8_t* dataIn, size_t len, uint8_t* dataOut) {
  // calculate buffer sizes
  size_t headerLen = 1;
  if(lwVer) {
    headerLen += 11; // LoRaWAN 1.1 header is 11 bytes longer than 1.0
  }
  size_t reqLen = 3*sizeof(uint8_t) + headerLen + len;
  size_t rplLen = sizeof(uint8_t) + len;

  // build buffers
  #if RADIOLIB_STATIC_ONLY
    uint8_t reqBuff[RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
    uint8_t rplBuff[RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* reqBuff = new uint8_t[reqLen];
    uint8_t* rplBuff = new uint8_t[rplLen];
  #endif
  
  // set the request fields
  reqBuff[0] = decKeyId;
  reqBuff[1] = verKeyId;
  reqBuff[2] = lwVer;
  memcpy(&reqBuff[3], header, headerLen);
  memcpy(&reqBuff[3 + headerLen], dataIn, len);

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_PROCESS_JOIN_ACCEPT, false, rplBuff, rplLen, reqBuff, reqLen);
  #if !RADIOLIB_STATIC_ONLY
    delete[] reqBuff;
  #endif
  if(state != RADIOLIB_ERR_NONE) {
    #if !RADIOLIB_STATIC_ONLY
      delete[] rplBuff;
    #endif
    return(state);
  }

  // check the crypto engine state
  if(rplBuff[0] != RADIOLIB_LR11X0_CRYPTO_STATUS_SUCCESS) {
    RADIOLIB_DEBUG_BASIC_PRINTLN("Crypto Engine error: %02x", rplBuff[0]);
    return(RADIOLIB_ERR_SPI_CMD_FAILED);
  }

  // pass the data
  memcpy(dataOut, &rplBuff[1], len);
  return(state);
}

int16_t LR11x0::cryptoComputeAesCmac(uint8_t keyId, uint8_t* data, size_t len, uint32_t* mic) {
  size_t reqLen = sizeof(uint8_t) + len;
  #if RADIOLIB_STATIC_ONLY
    uint8_t reqBuff[sizeof(uint8_t) + RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* reqBuff = new uint8_t[reqLen];
  #endif
  uint8_t rplBuff[5] = { 0 };
  
  reqBuff[0] = keyId;
  memcpy(&reqBuff[1], data, len);

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_COMPUTE_AES_CMAC, false, rplBuff, sizeof(rplBuff), reqBuff, reqLen);
  #if RADIOLIB_STATIC_ONLY
    delete[] reqBuff;
  #endif

  // check the crypto engine state
  if(rplBuff[0] != RADIOLIB_LR11X0_CRYPTO_STATUS_SUCCESS) {
    RADIOLIB_DEBUG_BASIC_PRINTLN("Crypto Engine error: %02x", rplBuff[0]);
    return(RADIOLIB_ERR_SPI_CMD_FAILED);
  }

  if(mic) { *mic = ((uint32_t)(rplBuff[1]) << 24) |  ((uint32_t)(rplBuff[2]) << 16) | ((uint32_t)(rplBuff[3]) << 8) | (uint32_t)rplBuff[4]; }
  return(state);
}

int16_t LR11x0::cryptoVerifyAesCmac(uint8_t keyId, uint32_t micExp, uint8_t* data, size_t len, bool* result) {
   size_t reqLen = sizeof(uint8_t) + sizeof(uint32_t) + len;
  #if RADIOLIB_STATIC_ONLY
    uint8_t reqBuff[sizeof(uint8_t) + sizeof(uint32_t) + RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* reqBuff = new uint8_t[reqLen];
  #endif
  uint8_t rplBuff[1] = { 0 };
  
  reqBuff[0] = keyId;
  reqBuff[1] = (uint8_t)((micExp >> 24) & 0xFF);
  reqBuff[2] = (uint8_t)((micExp >> 16) & 0xFF);
  reqBuff[3] = (uint8_t)((micExp >> 8) & 0xFF);
  reqBuff[4] = (uint8_t)(micExp & 0xFF);
  memcpy(&reqBuff[5], data, len);

  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_VERIFY_AES_CMAC, false, rplBuff, sizeof(rplBuff), reqBuff, reqLen);
  #if RADIOLIB_STATIC_ONLY
    delete[] reqBuff;
  #endif

  // check the crypto engine state
  if(rplBuff[0] != RADIOLIB_LR11X0_CRYPTO_STATUS_SUCCESS) {
    RADIOLIB_DEBUG_BASIC_PRINTLN("Crypto Engine error: %02x", rplBuff[0]);
    return(RADIOLIB_ERR_SPI_CMD_FAILED);
  }

  if(result) { *result = (rplBuff[0] == RADIOLIB_LR11X0_CRYPTO_STATUS_SUCCESS); }
  return(state);
}

int16_t LR11x0::cryptoAesEncrypt01(uint8_t keyId, uint8_t* dataIn, size_t len, uint8_t* dataOut) {
  return(this->cryptoCommon(RADIOLIB_LR11X0_CMD_CRYPTO_AES_ENCRYPT_01, keyId, dataIn, len, dataOut));
}

int16_t LR11x0::cryptoAesEncrypt(uint8_t keyId, uint8_t* dataIn, size_t len, uint8_t* dataOut) {
  return(this->cryptoCommon(RADIOLIB_LR11X0_CMD_CRYPTO_AES_ENCRYPT, keyId, dataIn, len, dataOut));
}

int16_t LR11x0::cryptoAesDecrypt(uint8_t keyId, uint8_t* dataIn, size_t len, uint8_t* dataOut) {
  return(this->cryptoCommon(RADIOLIB_LR11X0_CMD_CRYPTO_AES_DECRYPT, keyId, dataIn, len, dataOut));
}

int16_t LR11x0::cryptoStoreToFlash(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_STORE_TO_FLASH, true, NULL, 0));
}

int16_t LR11x0::cryptoRestoreFromFlash(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_RESTORE_FROM_FLASH, true, NULL, 0));
}

int16_t LR11x0::cryptoSetParam(uint8_t id, uint32_t value) {
  uint8_t buff[5] = {
    id,
    (uint8_t)((value >> 24) & 0xFF), (uint8_t)((value >> 16) & 0xFF),
    (uint8_t)((value >> 8) & 0xFF), (uint8_t)(value & 0xFF)
  };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_SET_PARAM, true, buff, sizeof(buff)));
}

int16_t LR11x0::cryptoGetParam(uint8_t id, uint32_t* value) {
  uint8_t reqBuff[1] = { id };
  uint8_t rplBuff[4] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_GET_PARAM, false, rplBuff, sizeof(rplBuff), reqBuff, sizeof(reqBuff));
  RADIOLIB_ASSERT(state);
  if(value) { *value = ((uint32_t)(rplBuff[0]) << 24) | ((uint32_t)(rplBuff[1]) << 16) | ((uint32_t)(rplBuff[2]) << 8) | (uint32_t)rplBuff[3]; }
  return(state);
}

int16_t LR11x0::cryptoCheckEncryptedFirmwareImage(uint32_t offset, uint32_t* data, size_t len) {
  // check maximum size
  if(len > (RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN/sizeof(uint32_t))) {
    return(RADIOLIB_ERR_SPI_CMD_INVALID);
  }
  
  return(this->writeCommon(RADIOLIB_LR11X0_CMD_CRYPTO_CHECK_ENCRYPTED_FIRMWARE_IMAGE, offset, data, len));
}

int16_t LR11x0::cryptoCheckEncryptedFirmwareImageResult(bool* result) {
  uint8_t buff[1] = { 0 };
  int16_t state = this->SPIcommand(RADIOLIB_LR11X0_CMD_CRYPTO_CHECK_ENCRYPTED_FIRMWARE_IMAGE_RESULT, false, buff, sizeof(buff));

  // pass the replies
  if(result) { *result = (bool)buff[0]; }
  
  return(state);
}

int16_t LR11x0::bootEraseFlash(void) {
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_BOOT_ERASE_FLASH, true, NULL, 0));
}

int16_t LR11x0::bootWriteFlashEncrypted(uint32_t offset, uint32_t* data, size_t len) {
  RADIOLIB_CHECK_RANGE(len, 1, 32, RADIOLIB_ERR_SPI_CMD_INVALID);
  return(this->writeCommon(RADIOLIB_LR11X0_CMD_BOOT_WRITE_FLASH_ENCRYPTED, offset, data, len));
}

int16_t LR11x0::bootReboot(bool stay) {
  uint8_t buff[1] = { (uint8_t)stay };
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_BOOT_REBOOT, true, buff, sizeof(buff)));
}

int16_t LR11x0::bootGetPin(uint8_t* pin) {
  if(!pin) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_BOOT_GET_PIN, false, pin, RADIOLIB_LR11X0_PIN_LEN));
}

int16_t LR11x0::bootGetChipEui(uint8_t* eui) {
  if(!eui) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_BOOT_GET_CHIP_EUI, false, eui, RADIOLIB_LR11X0_EUI_LEN));
}

int16_t LR11x0::bootGetJoinEui(uint8_t* eui) {
  if(!eui) {
    return(RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED);
  }
  return(this->SPIcommand(RADIOLIB_LR11X0_CMD_BOOT_GET_JOIN_EUI, false, eui, RADIOLIB_LR11X0_EUI_LEN));
}

int16_t LR11x0::writeCommon(uint16_t cmd, uint32_t addrOffset, uint32_t* data, size_t len) {
  // build buffers - later we need to ensure endians are correct, 
  // so there is probably no way to do this without copying buffers and iterating
  size_t buffLen = sizeof(uint32_t) + len*sizeof(uint32_t);
  #if RADIOLIB_STATIC_ONLY
    uint8_t dataBuff[sizeof(uint32_t) + RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* dataBuff = new uint8_t[buffLen];
  #endif

  // set the address or offset
  dataBuff[0] = (uint8_t)((addrOffset >> 24) & 0xFF);
  dataBuff[1] = (uint8_t)((addrOffset >> 16) & 0xFF);
  dataBuff[2] = (uint8_t)((addrOffset >> 8) & 0xFF);
  dataBuff[3] = (uint8_t)(addrOffset & 0xFF);

  // convert endians
  for(size_t i = 0; i < len; i++) {
    dataBuff[4 + i] = (uint8_t)((data[i] >> 24) & 0xFF);
    dataBuff[5 + i] = (uint8_t)((data[i] >> 16) & 0xFF);
    dataBuff[6 + i] = (uint8_t)((data[i] >> 8) & 0xFF);
    dataBuff[7 + i] = (uint8_t)(data[i] & 0xFF);
  }

  int16_t state = this->SPIcommand(cmd, true, dataBuff, buffLen);
  #if RADIOLIB_STATIC_ONLY
    delete[] dataBuff;
  #endif
  return(state);
}

int16_t LR11x0::cryptoCommon(uint16_t cmd, uint8_t keyId, uint8_t* dataIn, size_t len, uint8_t* dataOut) {
  // build buffers
  #if RADIOLIB_STATIC_ONLY
    uint8_t reqBuff[RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
    uint8_t rplBuff[RADIOLIB_LR11X0_SPI_MAX_READ_WRITE_LEN];
  #else
    uint8_t* reqBuff = new uint8_t[sizeof(uint8_t) + len];
    uint8_t* rplBuff = new uint8_t[sizeof(uint8_t) + len];
  #endif
  
  // set the request fields
  reqBuff[0] = keyId;
  memcpy(&reqBuff[1], dataIn, len);

  int16_t state = this->SPIcommand(cmd, false, rplBuff, sizeof(uint8_t) + len, reqBuff, sizeof(uint8_t) + len);
  #if !RADIOLIB_STATIC_ONLY
    delete[] reqBuff;
  #endif
  if(state != RADIOLIB_ERR_NONE) {
    #if !RADIOLIB_STATIC_ONLY
      delete[] rplBuff;
    #endif
    return(state);
  }

  // check the crypto engine state
  if(rplBuff[0] != RADIOLIB_LR11X0_CRYPTO_STATUS_SUCCESS) {
    RADIOLIB_DEBUG_BASIC_PRINTLN("Crypto Engine error: %02x", rplBuff[0]);
    return(RADIOLIB_ERR_SPI_CMD_FAILED);
  }

  // pass the data
  memcpy(dataOut, &rplBuff[1], len);
  return(state);
}

#endif