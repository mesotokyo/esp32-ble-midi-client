// main.cpp
#include <stdio.h>
#include <sstream>
#include <string>

#include "ble_midi_packet.h"
#include "uart_midi.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define TAG "main"

#include <NimBLEDevice.h>
#define DEVICE_NAME "esp32"

const char midiServiceID[] = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
const char midiCharID[] = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

static const NimBLEAdvertisedDevice* advDevice;
static bool doConnect  = false;
static uint32_t scanTimeMs = 5000; /** scan time in milliseconds, 0 = scan forever */

UartSerial serial;

inline  void DEBUG_PACKET(const char* t, BLEMidiMessage& msg) {
  ESP_LOGD(TAG, "[%s] [%d] 0x%02x %02x %02x ...(size: %d)",
           t,
           msg.getTimestamp(),
           msg.getMessageBody()[0],
           msg.getMessageBodySize() > 1 ? msg.getMessageBody()[1] : 0,
           msg.getMessageBodySize() > 2 ? msg.getMessageBody()[2] : 0,
           msg.getMessageBodySize());
}

class ClientCallbacks: public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    ESP_LOGI(TAG, "Connected");
  }

  void onDisconnect(NimBLEClient* pClient, int reason) override {
    ESP_LOGI(TAG, "%s Disconnected, reason = %d - Starting scan",
             pClient->getPeerAddress().toString().c_str(), reason);
    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
  }

  void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
    ESP_LOGI(TAG, "Server Passkey Entry");
    NimBLEDevice::injectPassKey(connInfo, 123456);
  }

  void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pass_key) override {
    ESP_LOGI(TAG, "The passkey YES/NO number: %" PRIu32, pass_key);
    /** Inject false if passkeys don't match. */
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    if (!connInfo.isEncrypted()) {
      ESP_LOGI(TAG, "Encrypt connection failed - disconnecting");
      /** Find the client with the connection handle provided in connInfo */
      NimBLEDevice::getClientByHandle(connInfo.getConnHandle())->disconnect();
      return;
    }
  }
} clientCallbacks;

class ScanCallbacks: public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    ESP_LOGV(TAG, "Advertised Device found: %s", advertisedDevice->toString().c_str());
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(midiServiceID))) {
      ESP_LOGI(TAG, "Found MIDI Service: %s", advertisedDevice->getName().c_str());

      /** stop scan before connecting */
      NimBLEDevice::getScan()->stop();

      /** Save the device reference in a global for the client to use*/
      advDevice = advertisedDevice;

      /** Ready to connect now */
      doConnect = true;
    }
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    ESP_LOGI(TAG, "Scan Ended, reason: %d, device count: %d; Restarting scan",
             reason, results.getCount());
    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
  }
} scanCallbacks;


void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic,
              uint8_t* pData, size_t length, bool isNotify) {
  BLEMidiPacket packet = BLEMidiPacket(pData, length);
  ESP_LOGD(TAG, "<<<< NOTIFY (%d byte(s)) >>>>", packet.getSize());
  while (packet.hasNextMessage()) {
    BLEMidiMessage msg = packet.nextMessage();
    DEBUG_PACKET("notify", msg);
    if (msg.isValid()) {
      serial.send((const char*)msg.getMessageBody(), msg.getMessageBodySize());
    }
  }
}

NimBLEClient* findReusableClient() {
  NimBLEClient* pClient = nullptr;

  // check if resuable client exists
  if (!NimBLEDevice::getCreatedClientCount()) {
    // no reusable client exists
    return nullptr;
  }

  pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());

  if (pClient) {
    /**
     *  Special case when we already know this device, we send false as the
     *  second argument in connect() to prevent refreshing the service database.
     *  This saves considerable time and power.
     */
      if (pClient->connect(advDevice, false)) {
        ESP_LOGI(TAG, "Reconnected client");
        return pClient;
      }

      ESP_LOGI(TAG, "Reconnect failed");
      return nullptr;
    }

  /**
   *  We don't already have a client that knows this device,
   *  check for a client that is disconnected that we can use.
   */
  return NimBLEDevice::getDisconnectedClient();
}

NimBLEClient* prepareClient() {
  NimBLEClient* pClient = findReusableClient();
  if (pClient) {
    return pClient;
  }

  /** No client to reuse, so create a new one. */
  if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
    ESP_LOGD(TAG, "Max clients reached - no more connections available");
    return nullptr;
  }

  ESP_LOGD(TAG, "create new client");
  pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(&clientCallbacks, false);

  /**
   *  Set initial connection parameters:
   *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
   *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
   *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 150 * 10ms = 1500ms timeout
   */
  ESP_LOGD(TAG, "set connection params");
  pClient->setConnectionParams(12, 12, 0, 150);

  /* Set how long we are willing to wait for the connection
      to complete (milliseconds), default is 30000. */
  pClient->setConnectTimeout(5 * 1000);

  if (pClient->connect(advDevice)) {
    ESP_LOGD(TAG, "Connection established.");
    return pClient;
  }

  /** Created a client but failed to connect, don't need to keep it as it has no data */
  NimBLEDevice::deleteClient(pClient);
  ESP_LOGD(TAG, "Failed to connect, deleted client");
  return nullptr;
}

NimBLEClient* connectToServer() {
  NimBLEClient* pClient = prepareClient();

  if (!pClient) {
      ESP_LOGD(TAG, "Failed to connect");
      return nullptr;
  }

  if (!pClient->isConnected() && !pClient->connect(advDevice)) {
    ESP_LOGD(TAG, "Failed to connect");
    return nullptr;
  }

  ESP_LOGI(TAG, "Connected to: %s RSSI: %d",
           pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

  return pClient;
}

// check if the client (MIDI peripheral) has MIDI service/characteristic
bool checkMidiServiceAndCharacteristic(NimBLEClient* pClient) {
  NimBLERemoteService* pSvc = nullptr;
  NimBLERemoteCharacteristic* pChr = nullptr;
  //NimBLERemoteDescriptor* pDsc = nullptr;

  if (!pClient) {
    ESP_LOGW(TAG, "client is not available");
    return false;
  }

  pSvc = pClient->getService(midiServiceID);
  if (!pSvc) {
    ESP_LOGW(TAG, "MIDI service is not available");
    return false;
  }
    
  pChr = pSvc->getCharacteristic(midiCharID);
  if (!pChr) {
    ESP_LOGW(TAG, "MIDI Characteristic is not available");
    return false;
  }
  
  return true;
}

bool subscribe(NimBLEClient* pClient) {
  NimBLERemoteService* pSvc = nullptr;
  NimBLERemoteCharacteristic* pChr = nullptr;

  pSvc = pClient->getService(midiServiceID);
  if (pSvc) {
    pChr = pSvc->getCharacteristic(midiCharID);
  }
  if (!pChr) {
    return false;
  }
  if (!pChr->canNotify()) {
    return false;
  }

  if (!pChr->subscribe(true, notifyCB)) {
    ESP_LOGW(TAG, "subscrive failed.");
    return false;
  }

  return true;
}

bool communicateWithServer(NimBLEClient* pClient, uint16_t* pLastTimestamp) {
  NimBLERemoteService* pSvc = nullptr;
  NimBLERemoteCharacteristic* pChr = nullptr;

  if (!pClient->isConnected()) {
    ESP_LOGW(TAG, "connection lost.");
    return false;
  }

  pSvc = pClient->getService(midiServiceID);
  if (pSvc) {
    pChr = pSvc->getCharacteristic(midiCharID);
  }

  if (pChr) {
    if (pChr->canRead()) {
      NimBLEAttValue v = pChr->readValue();
      BLEMidiPacket packet = BLEMidiPacket(v.data(), v.size());

      ESP_LOGD(TAG, "<<<< READ (%d byte(s)) >>>>", packet.getSize());
      while (packet.hasNextMessage()) {
        BLEMidiMessage msg = packet.nextMessage();
        if (msg.getTimestamp() == *pLastTimestamp) {
          ESP_LOGD(TAG, "(read same timestamp packet)");
        } else {
          *pLastTimestamp = msg.getTimestamp();
          DEBUG_PACKET("read", msg);
          if (msg.isValid()) {
            serial.send((const char*)msg.getMessageBody(), msg.getMessageBodySize());
          }
        }
      }
    }
  }
  return true;

  /*
    if (pChr->canWrite()) {
      if (pChr->writeValue("Tasty")) {
        ESP_LOGI(TAG, "Wrote new value to: %s", pChr->getUUID().toString().c_str());
      } else {
        pClient->disconnect();
        return false;
      }

      if (pChr->canRead()) {
        ESP_LOGI(TAG, "The value of: %s is now: %s",
                 pChr->getUUID().toString().c_str(), pChr->readValue().c_str());
      }
    }
    */
}

extern "C" void app_main(void) {
  esp_log_level_set(TAG, ESP_LOG_VERBOSE);
  ESP_LOGD(TAG, "Starting NimBLE Client");

  // start UART serial
  ESP_LOGD(TAG, "Initialize serial port");
  serial.initialize();
  
  /** Initialize NimBLE and set the device name */
  ESP_LOGD(TAG, "Initialize NimBLE Device");
  NimBLEDevice::init("NimBLE-Client");

  /**
   * Set the IO capabilities of the device, each option will trigger a different pairing method.
   *  BLE_HS_IO_KEYBOARD_ONLY   - Passkey pairing
   *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
   *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
   */
  // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
  // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

  /**
   * 2 different ways to set security - both calls achieve the same result.
   *  no bonding, no man in the middle protection, BLE secure connections.
   *  These are the default values, only shown here for demonstration.
   */
  // NimBLEDevice::setSecurityAuth(false, false, true);
  // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);

  /** Optional: set the transmit power */
  NimBLEDevice::setPower(3); /** 3dbm */

  /** Set the callbacks to call when scan events occur, no duplicates */
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&scanCallbacks, false);

  /** Set scan interval (how often) and window (how long) in milliseconds */
  pScan->setInterval(100);
  pScan->setWindow(100);

  /**
   * Active scan will gather scan response data from advertisers
   *  but will use more energy from both devices
   */
  pScan->setActiveScan(true);

  /** Start scanning for advertisers */
  pScan->start(scanTimeMs);
  ESP_LOGD(TAG, "Scanning for peripherals");

  /** Loop here until we find a device we want to connect to */
  while (true) {
    vTaskDelay(10 / portTICK_PERIOD_MS);

    if (doConnect) {
      doConnect = false;
      /** Found a device we want to connect to, do it now */
      NimBLEClient* pClient = connectToServer();
      if (pClient) {
        if (!subscribe(pClient)) {
          ESP_LOGW(TAG, "subscribe failed.");
        }
        uint16_t lastTs = 0;
        if (checkMidiServiceAndCharacteristic(pClient)) {
            while (communicateWithServer(pClient, &lastTs)) {
              vTaskDelay(10 / portTICK_PERIOD_MS);
            }
          }
      } else {
        ESP_LOGI(TAG, "No available connection");
      }

        ESP_LOGI(TAG, "restarting scan");
      NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
  }
}
