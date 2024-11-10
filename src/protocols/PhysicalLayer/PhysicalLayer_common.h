#if !defined(_RADIOLIB_PHYSICAL_LAYER_COMMON_H)
#define _RADIOLIB_PHYSICAL_LAYER_COMMON_H

#include "PhysicalLayer.h"

#include <string.h>


#if defined(RADIOLIB_BUILD_ARDUINO)
int16_t common_readData(String& str, size_t len) {
  int16_t state = RADIOLIB_ERR_NONE;

  // read the number of actually received bytes
  size_t length = getPacketLength();

  if((len < length) && (len != 0)) {
    // user requested less bytes than were received, this is allowed (but frowned upon)
    // requests for more data than were received will only return the number of actually received bytes (unlike PhysicalLayer::receive())
    length = len;
  }

  // build a temporary buffer
  #if RADIOLIB_STATIC_ONLY
    uint8_t data[RADIOLIB_STATIC_ARRAY_SIZE + 1];
  #else
    uint8_t* data = new uint8_t[length + 1];
    RADIOLIB_ASSERT_PTR(data);
  #endif

  // read the received data
  state = readData(data, length);

  // any of the following leads to at least some data being available
  // let's leave the decision of whether to keep it or not up to the user
  if((state == RADIOLIB_ERR_NONE) || (state == RADIOLIB_ERR_CRC_MISMATCH) || (state == RADIOLIB_ERR_LORA_HEADER_DAMAGED)) {
    // add null terminator
    data[length] = 0;

    // initialize Arduino String class
    str = String((char*)data);
  }

  // deallocate temporary buffer
  #if !RADIOLIB_STATIC_ONLY
    delete[] data;
  #endif

  return(state);
}
#endif


#endif
