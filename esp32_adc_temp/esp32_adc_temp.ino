// ADC voltage + DS18B20 temperature -> USB serial (no BLE). 115200 baud.
//   ADC : GPIO34 (ADC1_CH6), 11 dB attenuation (~0-3.1 V), sampled every 5 ms
//   10 NPLC filter: 40 samples x 5 ms = 200 ms = 10 mains cycles at 50 Hz, averaged
//   DS18B20 DQ on GPIO21 (4.7k pull-up), read on core 0 so its ~750 ms conversion
//   and bit-banged 1-Wire never delay the 5 ms ADC sampling on core 1.
// Output, one line per 200 ms:
//   <ADC average, raw 12-bit code>,<tempC>
// tempC is the latest DS18B20 reading; -127.00 when no valid reading in the last 3 s.
// Lines starting with '#' are status messages.
#include <OneWire.h>

const uint8_t ADC_PIN = 34;
const uint8_t DQ_PIN = 21;
const uint32_t SAMPLE_US = 5000;
const int N_AVG = 40;                 // 10 NPLC @ 50 Hz
const uint32_t TEMP_STALE_MS = 3000;

volatile float g_temp = -127.0f;
volatile uint32_t g_temp_ms = 0;      // millis() of last valid reading, 0 = never

void tempTask(void *) {
  OneWire ow(DQ_PIN);
  uint8_t sp[9];
  for (;;) {
    if (!ow.reset()) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    ow.skip();
    ow.write(0x44, 1);                // CONVERT T, keep bus powered (parasite-safe)
    vTaskDelay(pdMS_TO_TICKS(800));   // 12-bit conversion: 750 ms max
    ow.depower();
    if (!ow.reset()) continue;
    ow.skip();
    ow.write(0xBE);                   // READ SCRATCHPAD
    ow.read_bytes(sp, 9);
    if (OneWire::crc8(sp, 8) != sp[8]) continue;
    float t = (int16_t)((sp[1] << 8) | sp[0]) / 16.0f;
    if (t == 85.0f && g_temp_ms == 0) continue;   // power-on reset value
    g_temp = t;
    g_temp_ms = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_PIN, ADC_11db);
  pinMode(DQ_PIN, INPUT_PULLUP);      // backup for the external 4.7k
  xTaskCreatePinnedToCore(tempTask, "ds18b20", 4096, nullptr, 1, nullptr, 0);
  Serial.println("# ADC GPIO34 5ms x40 (10NPLC), DS18B20 GPIO21");
  Serial.println("# adc,tempC");
}

void loop() {
  static uint32_t next = micros();
  static uint32_t sum = 0;
  static int n = 0;

  if ((int32_t)(micros() - next) < 0) return;
  next += SAMPLE_US;
  sum += analogRead(ADC_PIN);
  if (++n < N_AVG) return;

  float adc = sum / (float)N_AVG;
  sum = 0; n = 0;
  uint32_t tms = g_temp_ms;
  float t = (tms != 0 && millis() - tms < TEMP_STALE_MS) ? g_temp : -127.0f;
  Serial.print(adc, 2);
  Serial.print(',');
  Serial.println(t, 4);
}
