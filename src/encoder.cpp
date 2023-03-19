
#include "Arduino.h"
#include <encoder.h>

extern bool loopTaskWDTEnabled;
extern TaskHandle_t loopTaskHandle;


static IRAM_ATTR void enc_cb(void* arg) {
    ESP32Encoder* enc = (ESP32Encoder*) arg;
    //Serial.printf("Enc count: %d\n", encoder.getCount());
    static bool leds = false;
    digitalWrite(LED_BUILTIN, (int)leds);
    leds = !leds;
}
ESP32Encoder encoder(true, enc_cb);

void encoder_Init()
{
    // Encoder Init
    loopTaskWDTEnabled = true;
    pinMode(LED_BUILTIN, OUTPUT);

    ESP32Encoder::useInternalWeakPullResistors=UP;
    encoder.attachSingleEdge(26, 25);
    encoder.clearCount();
    encoder.setFilter(100);
    encoder.setCount(50);
}
