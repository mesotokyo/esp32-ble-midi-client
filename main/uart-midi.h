#include "driver/uart.h"

class UartSerial {
public:
  UartSerial();
  void initialize();
  void send(const char* data, size_t size);

private:
  QueueHandle_t uartQueue;
  uart_port_t uartNum;
};

