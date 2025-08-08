#include "uart-midi.h"

UartSerial::UartSerial() {
  // Setup UART buffered IO with event queue
  uartNum = UART_NUM_0;
}

void UartSerial::initialize() {
  const int uartBufferSize = (1024 * 2);

  // Install UART driver using an event queue here
  ESP_ERROR_CHECK(uart_driver_install(uartNum, // port number
                                      uartBufferSize, // size of RX ring buffer
                                      uartBufferSize, // size of TX ring buffer
                                      10, // event queue size
                                      &uartQueue, // event queue
                                      0)); // interrupt flag

  uart_config_t uartConfig = {
    .baud_rate = 31250,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };

  ESP_ERROR_CHECK(uart_set_line_inverse(uartNum, UART_SIGNAL_TXD_INV));

  // Configure UART parameters
  ESP_ERROR_CHECK(uart_param_config(uartNum, &uartConfig));

  // TX: D6/GPIO43 - U0RXD - 49
  // RX: D2/GPOI44 - U0RXD - 50
  //ESP_ERROR_CHECK(uart_set_pin(uartNum, 49, 50,
  // UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void UartSerial::send(const char* data, size_t size) {
  uart_write_bytes(uartNum, data, size);
}
