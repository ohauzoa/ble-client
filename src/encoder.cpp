#include <Arduino.h>

#include "AiEsp32RotaryEncoder.h"

#define ROTARY_ENCODER_A_PIN 25
#define ROTARY_ENCODER_B_PIN 26
#define ROTARY_ENCODER_BUTTON_PIN 0
#define ROTARY_ENCODER_VCC_PIN 0

#define ROTARY_ENCODER_STEPS 4

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
const int table[51] = {0, 1, 4, 9, 14, 20, 25, 30, 36, 41, 46, 52, 57, 62, 67, 73, 78, 84, 89, 94, 99, 104, 110, 115, 121, 126, 128, 133, 139, 144, 149, 155, 160, 165, 170, 176, 181, 187, 192, 197, 202, 208, 213, 218, 224, 230, 235, 241, 246, 251, 255};

float getAmpair()
{
    return (float)rotaryEncoder.readEncoder() / 100.0;
}

void rotary_onButtonClick()
{
    static unsigned long lastTimePressed = 0;
    if (millis() - lastTimePressed < 200)
        return;
    lastTimePressed = millis();

    printf("Radio station set to %f MHz\n", getAmpair());
}

void IRAM_ATTR readEncoderISR()
{
	rotaryEncoder.readEncoder_ISR();
}


void encoderTask(void *pvParameters)
{
    //we must initialize rotary encoder
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    //rotaryEncoder.setBoundaries(3 * 100, 10 * 100, true); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
    rotaryEncoder.setBoundaries(0, 50, true); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
    rotaryEncoder.setAcceleration(30);
    rotaryEncoder.setEncoderValue(0); //set default to 92.1 MHz
	//rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
	//rotaryEncoder.setAcceleration(250); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration

    while(1)
    {
        if (rotaryEncoder.encoderChanged())
        {
            int val = rotaryEncoder.readEncoder();
            //val = map(val, 0, 1000, 0, 255);
            printf("%d.%d A, %d\n", val/10, val%10, table[val]);
    //        tftOutput(val);
    //        dacWrite(25, table[val]);
        //changeFrequency(val);
        }
        if (rotaryEncoder.isEncoderButtonClicked())
        {
            rotary_onButtonClick();
        }

        delay(10);
    }
}

void encoderInit(void)
{
    xTaskCreatePinnedToCore( encoderTask, "encoder", 5000, NULL, 20 | portPRIVILEGE_BIT, NULL, 0);
}
