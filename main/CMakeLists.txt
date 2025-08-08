idf_component_register(SRCS "main.cpp" "ble_midi_packet.cpp" "uart_midi.cpp"
  INCLUDE_DIRS "."
  REQUIRES esp-nimble-cpp esp_driver_uart
)
