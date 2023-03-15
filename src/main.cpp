#include <Arduino.h>
#include <ESP32Encoder.h>

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


#include <TFT_eSPI.h> 
#include "gauge1.h"
#include "gauge2.h"
#include "gauge3.h"
#include "gauge4.h"
#include "gauge5.h"
#include "gauge6.h"
#include "font.h"

TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite img = TFT_eSprite(&tft);
TFT_eSprite ln = TFT_eSprite(&tft);

double rad=0.01745;
int angle;

int sx=120;
int sy=120;
int r=76;

float x[360];
float y[360];
float x2[360];
float y2[360];

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

int chosenOne=0;
int minValue[6]={0,20,0,0,0,80};
int maxValue[6]={40,100,60,80,70,160};
int dbounce=0;

/* push button 13 2 15 */
//#include <Arduino_GFX_Library.h>

//#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin
//#define TFT_BL   32  // LED back-light
//#define TFT_MOSI 23 // In some display driver board, it might be written as "SDA" and so on.
//#define TFT_SCLK 18
//#define TFT_CS   14  // Chip select control pin
//#define TFT_DC   27  // Data Command control pin
//#define TFT_RST  33  // Reset pin (could connect to Arduino RESET pin)

//Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC /* DC */, TFT_CS /* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, GFX_NOT_DEFINED /* MISO */);
//Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 0 /* row offset 1 */, 0 /* col offset 2 */, 0 /* row offset 2 */);

#include "BLEDevice.h"

// Timer variables
static unsigned long lastTime = 0;
static unsigned long ConnectTime = 0;

// BLE UART 서비스와 Rx/Tx UUID
static BLEUUID    serviceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static BLEUUID    rxUUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");  //Server 입장에서의 RX이므로 Client는 Tx이다.
static BLEUUID    txUUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");  //Server 입장에서의 Tx이므로 Client는 Rx이다.

//연결 및 장치 검색 상태 변수
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;

// Rx 캐릭터리스틱, Client 입장에선 Tx 즉, 여기서는 데이터 전송을 이것으로 한다.
static BLERemoteCharacteristic* pRxCharacteristic;
// Tx 캐릭터리스틱, Client 입장에선 Rx 즉, 여기서는 데이터를 송신할때 사용한다.
static BLERemoteCharacteristic* pTxCharacteristic;
// 연결할 장치의 정보를 저장하는 변수
static BLEAdvertisedDevice* myDevice;

// BLE 연결 상태 콜백 함수 
// 연결되었거나 연결이 끊어졌거나
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
    }

    void onDisconnect(BLEClient* pclient) {
        connected = false;
        doScan = true;
        Serial.println("onDisconnect");
    }
};

// 장치 검색 콜백함수, 새로운 장치가 검색되면 한번씩 들어옴
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");    
    Serial.print(advertisedDevice.haveServiceUUID());
    Serial.print(" , ");
    Serial.println(advertisedDevice.toString().c_str());
    // 어드버타이징 정보에 서비스 UUID가 있는지 확인하고 있다면 BLE UART 서비스 UUID인지 확인
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      //BLE UART 서비스 UUID라면 우리가 찾으려던 녀석!!
      Serial.print("A device to be connected has been found.");
      //검색을 중지하고...
      BLEDevice::getScan()->stop();
      //해당 장치 정보를 myDevice에 저장
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      //연결을 시작!
      doConnect = true;
      doScan = false;
    }
  }
};


// RX 처리 콜백 함수
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);  // 이게 데이터!
}


//장치와 연결을 시도하는 함수, 여기가 키(KEY)!!!
bool connectToServer() {
    //myDevice는 연결할 장치의 정보가 담긴 변수이다.
    //아래 장치 스캔에서 해당 변수에 정보를 넣을 것이다.    
    Serial.print("Forming a connection to ");
    //BLE 맥주소 표시
    Serial.println(myDevice->getAddress().toString().c_str());
    
    //서버(Pheriphral)과 연결할 클라이언트(Central) 클래스 생성
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    //연결 상태 이벤트를 받기 위한 콜백 함수 설정 
    pClient->setClientCallbacks(new MyClientCallback());
    Serial.println(" - Connected to server");

    //드디어 서버(Pheriphral) 장치와 연결시도!!!
    pClient->connect(myDevice);
    Serial.println(" - Connected to server111");
    
    //연결이되면 서비스 정보 받기
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    //서비스 정보가 null이라면 문제가 있으니 연결해제하고 종료
    if (pRemoteService == nullptr) {        
		//서비스 UUID를 출력
		Serial.println("Failed to find our service UUID: ");
		Serial.println(serviceUUID.toString().c_str());
		pClient->disconnect();
		return false;      
	}
    //Client 입장에서의 Rx 캐릭터리스틱 클래스 얻기
    pTxCharacteristic = pRemoteService->getCharacteristic(txUUID);
    if (pTxCharacteristic == nullptr) {
      //마찬가지로 못 가져오면 문제가 있으니 연결해제하고 종료
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(rxUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
	Serial.println(" - Found our characteristic");
    // Tx 캐릭터리스틱이 Notify를 지원하는지 확인.
    if(pTxCharacteristic->canNotify()) {    
      //데이터를 받을 콜백함수 등록
      pTxCharacteristic->registerForNotify(notifyCallback);
    }

    // Client 입장에서의 Tx 캐릭터리스틱 클래스 얻기
    pRxCharacteristic = pRemoteService->getCharacteristic(rxUUID);
    if (pRxCharacteristic == nullptr) {
      //null이면 문제 있음..
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(txUUID.toString().c_str());
      pClient->disconnect();
      return false;
    } 
    // Rx 캐릭터리스틱에는 데이터를 보내야하므로 write 를 지원해야한다. 
    // write를 지원하는지 확인!
    if(!pRxCharacteristic->canWrite()) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(txUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    //여기까지 왔으면 연결 성공했다고 변수 설정
    connected = true;
    return true;
}

void task_gui(void *pvParameters)
{
    //min angle 136 or 137
    //max angle 43
    int a1,a2;
    int result=0;

    //ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
    //ledcAttachPin(BUILTIN_LED, pwmLedChannelTFT);
    //ledcWrite(pwmLedChannelTFT, 10);

    pinMode(13,INPUT_PULLUP);

    tft.init();
    tft.setRotation(2);
    tft.setSwapBytes(1);
    img.setSwapBytes(1);
    tft.fillScreen(TFT_ORANGE);
    img.createSprite(240, 240);

    tft.setPivot(60,60);
    img.setTextDatum(4);
    img.setTextColor(TFT_BLACK,0xAD55);
    img.setFreeFont(&Orbitron_Medium_28);

    int i=0;
    int a=136;

    while(a!=44)
    {
        x[i]=r*cos(rad*a)+sx;
        y[i]=r*sin(rad*a)+sy;
        x2[i]=(r-20)*cos(rad*a)+sx;
        y2[i]=(r-20)*sin(rad*a)+sy;
        i++;
        a++;
        if(a==360)
        a=0;
    }

    //img.pushImage(0,0,240,240,gauge1);
    //img.pushSprite(0, 0);

    while(1)
    {
        if(digitalRead(13)==0)
        {
            if(dbounce==0)
            {
                dbounce=1;
                chosenOne++;
                if(chosenOne>=6) chosenOne=0;
            }
        }else dbounce=0;

        result=map(encoder.getCount(),0,100,minValue[chosenOne],maxValue[chosenOne]);
        //result=map(analogRead(14),0,4095,minValue[chosenOne],maxValue[chosenOne]);
        //result=32;
        angle=map(result,minValue[chosenOne],maxValue[chosenOne],0,267);
    


        if(chosenOne==0) img.pushImage(0,0,240,240,gauge1);
        if(chosenOne==1) img.pushImage(0,0,240,240,gauge2);
        if(chosenOne==2) img.pushImage(0,0,240,240,gauge3);
        if(chosenOne==3) img.pushImage(0,0,240,240,gauge4);
        if(chosenOne==4) img.pushImage(0,0,240,240,gauge5);
        if(chosenOne==5) img.pushImage(0,0,240,240,gauge6);

        if(chosenOne==5) img.drawFloat(result/10.00,2,120,114);
        else if(chosenOne==4) img.drawString(String(result*100),120,114);
        else img.drawString(String(result),120,114);
        //img.drawString(String(analogRead(22)), 30,10,2);

        a1=angle-4;
        a2=angle+4;

        if(a1<0) a1=angle-4+359;
        if(a2>=359) a2=angle+4-359;

        if(result<=minValue[chosenOne]+4)
        img.fillTriangle(x[angle],y[angle],x2[angle],y2[angle],x2[a2+2],y2[a2+2],TFT_RED);
        else if(result>=maxValue[chosenOne]-4)
        img.fillTriangle(x[angle],y[angle],x2[a1-2],y2[a1-2],x2[angle],y2[angle],TFT_RED);
        else
        img.fillTriangle(x[angle],y[angle],x2[a1],y2[a1],x2[a2],y2[a2],TFT_RED);


        img.pushSprite(0, 0);

        delay(10);
    }    
}


void setup() {
    Serial.begin(115200);                       // Initialize Serial to log output
    while (!Serial) ;

    // Encoder Init
    loopTaskWDTEnabled = true;
    pinMode(LED_BUILTIN, OUTPUT);
    ESP32Encoder::useInternalWeakPullResistors=UP;
    encoder.attachSingleEdge(26, 25);
    encoder.clearCount();
    encoder.setFilter(100);
    encoder.setCount(50);


    //gfx->begin();
    //gfx->fillScreen(BLACK);

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    //gfx->setCursor(10, 10);
    //gfx->setTextColor(RED);
    //gfx->println("Hello World!");

    delay(1000); // 5 seconds

	//BLE 클래스 초기화
	BLEDevice::init("");
	//스캔 클래스 생성
	BLEScan* pBLEScan = BLEDevice::getScan();
	//장치 검색되면 호출할 콜백함수 등록
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());


	//pBLEScan->start(0); //0은 무제한 검색
	// scan interval과 window 사이의 상관관계는 논문 찾아봐야 함.
	pBLEScan->setInterval(1349);
	pBLEScan->setWindow(449);	
	pBLEScan->setActiveScan(false);
	pBLEScan->start(5, false);

    xTaskCreatePinnedToCore(task_gui, "Task_GUI", 4096, NULL, 50, NULL, 1);
}



void loop() {
	//gfx->setCursor(random(gfx->width()), random(gfx->height()));
    //gfx->setTextColor(random(0xffff), random(0xffff));
    //gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
    //gfx->println("Hello World!");

	//doConnect가 true 연결을 해야함.
	if (doConnect == true) {
	//연결을 시도하는 함수 호출
		if (connectToServer()) {
			//무사히 연결되었다면!
			Serial.println("We are now connected to the BLE Server.");
		} else {
			//연결 실패시
			Serial.println("We have failed to connect to the server; there is nothin more we will do.");
		}
		//더이상 연결시도 안하기 위해 변수 false로 변경
		doConnect = false;
	}

	//장치가 연결되었다면
	if (connected) {
		//부팅후 현재까지의 시간
		String newValue = "Time since boot: " + String(millis()/1000);    
		Serial.println("Setting new characteristic value to \"" + newValue + "\"");    
		//그대로 서버(Pheriphral)에 보냄.
		pRxCharacteristic->writeValue(newValue.c_str(), newValue.length());
	}
	else if(doScan){
		//연결이 해제되고 doScan이 true면 스캔 시작
		BLEDevice::getScan()->start(0);
	}

	//10ms 한번씩 체크
	if(millis() - lastTime > 100) {
		lastTime = millis();
		ConnectTime++;
		Serial.print("tm: ");
		Serial.println(ConnectTime);
	}
	//1초 대기후 루프 재시작
	delay(1000);
    //update_needle();
    //Serial.printf("Enc count: %d\n", encoder.getCount());

//    delay(20); // 1 second
}
