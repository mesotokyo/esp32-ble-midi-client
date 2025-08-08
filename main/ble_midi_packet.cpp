#include "ble_midi_packet.h"

#define TIMESTAMP_LOW_MASK 0b01111111
#define TIMESTAMP_HIGH_MASK 0b00111111

BLEMidiMessage::BLEMidiMessage(const uint8_t* pAttrValue, uint8_t timestampHigh, size_t size) {
  timestamp = 0;
  bodySize = 0;
  pValue = pAttrValue;
  valid = false;

  if (pValue == nullptr || size < 2) {
    return;
  }

  timestamp = (timestampHigh << 7) + (TIMESTAMP_LOW_MASK & pValue[0]);
  bodySize = size - 1;
  valid = true;
}

uint16_t BLEMidiMessage::getTimestamp() {
  return timestamp;
}

const uint8_t* BLEMidiMessage::getMessageBody() {
  return (pValue + 1);
}

size_t BLEMidiMessage::getMessageBodySize() {
  return bodySize;
}

bool BLEMidiMessage::isValid() {
  return valid;
}

BLEMidiPacket::BLEMidiPacket(const uint8_t* pAttrValue, size_t size) {
  pValue = pAttrValue;
  packetSize = size;
  currentIndex = 1;
  timestampHigh = 0;
  valid = false;

  // check pointer and size is valid
  // packet must include header and message timestamp e.g. size must >= 2
  if (pValue == nullptr || size < 2) {
    return;
  }

  // check if header and message timestamp is valid
  // first and second byte of BLE packet must start with bit "1"
  // i.e. these bytes are not data byte of MIDI
  if (isDataByte(pValue[0]) || isDataByte(pValue[1])) {
    return;
  }

  timestampHigh = TIMESTAMP_HIGH_MASK & pValue[0];
  valid = true;
}

BLEMidiMessage BLEMidiPacket::firstMessage() {
  currentIndex = 1;
  return nextMessage();
}

BLEMidiMessage BLEMidiPacket::nextMessage() {
  if (!hasNextMessage()) {
    return BLEMidiMessage(nullptr, timestampHigh, 0);
  }

  size_t start = currentIndex;
  // pValue[pStart] expects to timestampLow, so no check.
  currentIndex++;

  // check buffer size
  if (currentIndex >= packetSize) {
    return BLEMidiMessage(pValue + start, timestampHigh, 1);
  }

  if (!isDataByte(pValue[currentIndex])) {
    // a byte next to timestampLow can be MIDI status or system message.
    currentIndex++;
  }

  while((currentIndex < packetSize) && isDataByte(pValue[currentIndex])) {
    currentIndex++;
  }
  return BLEMidiMessage(pValue + start, timestampHigh, currentIndex - start);
}
  
bool BLEMidiPacket::hasNextMessage() {
  return currentIndex < packetSize;
}

bool BLEMidiPacket::isValid() {
  return valid;
}
  
size_t BLEMidiPacket::getSize() {
  return packetSize;
}
