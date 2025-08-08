#include <cstddef>
#include <cstdint>

class BLEMidiMessage {
public:
  BLEMidiMessage(const uint8_t* pAttrValue, uint8_t timestampHigh, size_t size);
  uint16_t getTimestamp();
  const uint8_t* getMessageBody();
  size_t getMessageBodySize();
  bool isValid();

protected:
  uint16_t timestamp;
  size_t bodySize;
  const uint8_t* pValue;
  bool valid;
};

class BLEMidiPacket {
public:
  BLEMidiPacket(const uint8_t* pAttrValue, size_t size);
  BLEMidiMessage firstMessage();
  BLEMidiMessage nextMessage();
  bool hasNextMessage();
  bool isValid();
  size_t getSize();

private:
  template <typename T>
  inline bool isDataByte(T& data) {
    return !(data >> 7);
  }

protected:
  const uint8_t* pValue;
  size_t packetSize;
  size_t currentIndex;
  uint8_t timestampHigh;
  bool valid;
};
