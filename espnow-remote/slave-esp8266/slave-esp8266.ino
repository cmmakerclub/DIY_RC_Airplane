#define CMMC_USE_ALIAS


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <CMMC_SimplePair.h>
#include <CMMC_ESPNow.h>
#include <LittleFS.h>
#include <ArduinoJSON.h>
#include <CMMC_LED.h>
#include <CMMC_Utils.h>
#include <Ticker.h>
#include <Button2.h>

#define LED_PIN (2)
CMMC_ESPNow espNow;
CMMC_LED led(LED_PIN, LOW);
CMMC_SimplePair instance;
Ticker blinker;

#define BUTTON_PIN              (0)
Button2 button = Button2(BUTTON_PIN);


void flip() {
  led.toggle();
}



void dump(const u8* data, size_t size) {
  for (size_t i = 0; i < size - 1; i++) {
    Serial.printf("%02x ", data[i]);
  }
  Serial.printf("%02x", data[size - 1]);
  Serial.println();
}

uint8_t master_mac[6];
char master_mac_string[13];
bool ready = false;
uint32_t sent_count = 0;

bool saveConfig() {
  StaticJsonDocument<200> doc;
  CMMC::macByteToString(master_mac, master_mac_string);
  doc["master_mac"] = String(master_mac_string);
  File configFile = LittleFS.open("config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  Serial.print(F("Serializing to file. Size = "));
  uint16 size = serializeJson(doc, configFile);

  Serial.println(size);
  configFile.close();
  return true;
}

bool loadConfig() {
  File configFile = LittleFS.open("config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  // Serial.println(String(buf));

  StaticJsonDocument<200> doc;
  auto error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }

  const char* mac = doc["master_mac"];
  Serial.print("MAC=");
  Serial.println(mac);

  if (mac != NULL) {
    uint8_t macx[6];
    CMMC::convertMacStringToUint8(mac, macx);
    memcpy(master_mac, macx, 6);
    ready = true;
  }



  // Real world application would store these values in some variables for
  // later use.

  return true;
}

void listDir(const char * dirname) {
  Serial.printf("Listing directory: %s\n", dirname);

  Dir root = LittleFS.openDir(dirname);

  while (root.next()) {
    File file = root.openFile("r");
    Serial.print("  FILE: ");
    Serial.print(root.fileName());
    Serial.print("  SIZE: ");
    Serial.println(file.size());
    file.close();
  }
}

void evt_callback(u8 status, u8* sa, const u8* data) {
  if (status == 0) {
    Serial.printf("[CSP_EVENT_SUCCESS] STATUS: %d\r\n", status);
    Serial.printf("WITH KEY: "); dump(data, 16);
    Serial.printf("FROM MAC: "); dump(sa, 6);
    memcpy(master_mac, sa, 6);
    saveConfig();
    delay(100);
    ESP.restart();
  }
  else {
    Serial.printf("[CSP_EVENT_ERROR] %d: %s\r\n", status, (const char*)data);
  }
}

typedef struct __attribute((__packed__)) {
  uint8_t outzxc[3];
} CMMC_SENSOR_DATA_T;

CMMC_SENSOR_DATA_T data;

void start_config_mode() {
  instance.debug([](const char* c) {
    Serial.println(c);
  });

  instance.begin(SLAVE_MODE, evt_callback);
  instance.start();
}

void tripleClick(Button2& btn) {
    ready = false;
    Serial.println("triple click\n");
    blinker.attach_ms(100, flip);
    esp_now_deinit();
    start_config_mode();
}




void setup()
{
  wifi_station_set_auto_connect(0);

  led.init();
  Serial.begin(115200);
  pinMode(12, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
  LittleFS.begin();

  button.setDoubleClickHandler(tripleClick);

  // delay(2000);
  // delay(5000);
  // 60 01 94 3f f7 aa
  // File configFile = LittleFS.open("mac.json", "w");
  Serial.println("Mounting FS...");
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  // saveConfig();
  listDir("/");
  loadConfig();

  if (ready) {
    espNow.init(NOW_MODE_SLAVE);
    espNow.enable_retries(true);
    // static CMMC_LED *led;
    // static ESPNowModule* module;
    // led = ((CMMC_Legend*) os)->getBlinker();
    // led->detach();
    espNow.on_message_sent([](uint8_t *macaddr, u8 status) {
      sent_count++;
      led.toggle();
    });
    // module = this;
    espNow.on_message_recv([](uint8_t * macaddr, uint8_t * data, uint8_t len) { });
  }
  else {

  }
}


uint32_t count = 0;
uint32_t prev = millis();
void loop()
{
  count++;
  button.loop();

  if (ready) {
    if (sent_count > 0 &&  sent_count % 100 == 0 ) {
      Serial.printf("diff = %lu, rate = %f\r\n", millis() - prev, (float)sent_count / ( (millis() - prev) / 1000.0));
      sent_count = 0;
      count = 0;
      prev = millis();
    }
    data.outzxc[0] = (!digitalRead(12))*255;
    data.outzxc[1] = (!digitalRead(13))*255;
    data.outzxc[2] = data.outzxc[0]+data.outzxc[1];
    dump(master_mac, 6);
    esp_now_send(master_mac, (u8*) &data, sizeof(data));
    delay(20);
  }

}
