/*****************************************************************************
 *                                                                            *
 *                      Programa ESP32 com MQTT e DHT22                       *
 *                                                                            *
 *                       Desenvolvido por Kevin da Cruz Souto                 *
 *                                                                            *
 ******************************************************************************
 *                                                                            *
 *  Para verificar a funcionalidade MQTT, configure o cliente do navegador    *
 *  HiveMQ para visualizar as mensagens publicadas pelo cliente MQTT.         *
 *                                                                            *
 *  Passos:                                                                   *
 *  1) Vá para a URL abaixo e clique no botão conectar:                       *
 *     http://www.hivemq.com/demos/websocket-client/                          *
 *                                                                            *
 *  2) Adicione os tópicos de assinatura, um para cada tópico que o ESP32     *
 *     utiliza:                                                               *
 *     - temp-led-data/#                                                      *
 *     - led-state-control/#                                                  *
 *                                                                            *
 *  3) Experimente publicar no tópico temp-led-data com a seguinte mensagem:  *
 *     - Para ligar o LED:   "HIGH_BUTTON, HIGH_MQTT"                         *
 *     - Para desligar o LED: "LOW"                                           *
 *                                                                            *
 ******************************************************************************
 *                                                                            *
 *  Descrição:                                                                *
 *  Este programa utiliza o ESP32 para ler dados de um sensor DHT22, controlar*
 *  um LED usando a biblioteca NeoPixel, e interagir com um botão utilizando  *
 *  interrupções. Os dados do sensor são publicados via MQTT, e o ESP32 pode  *
 *  receber comandos via MQTT para controlar o estado do LED.                 *
 *                                                                            *
 *****************************************************************************/

#include <Wire.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

//--------------------Constantes Globais--------------------
#define BUTTON_PIN 41
#define DHT_PIN 16     //Definição da porta de dados do DHT22

#define DHT_TYPE DHT22 //DHT22-Sensor

//MQTT DATA
#define TOPIC_PUBLISH_DATA         "temp-led-data"
#define TOPIC_SUBSCRIBE_LED_STATE  "led-state-control"
#define PUBLISH_DELAY 3000          //Atraso da publicação (3 segundos)
#define ID_MQTT "esp32_mqtt"        //id mqtt (para identificação de sessão)
//----------------------------------------------------------

//----------------- --Objetos Globais-----------------------
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_NeoPixel LED_RGB(1, 38, NEO_GRBW + NEO_KHZ800);
WiFiClient espClient;         //Cria o objeto espClient
PubSubClient MQTT(espClient); //Instancia o Cliente MQTT passando o objeto espClient
//----------------------------------------------------------

//--------------------Variáveis Globais---------------------
const char *SSID = "Wokwi-GUEST"; //SSID nome da rede WI-FI que deseja se conectar
const char *PASSWORD = "";        //Senha da rede WI-FI que deseja se conectar

//URL do broker MQTT que se deseja utilizar
const char *BROKER_MQTT = "broker.hivemq.com";

int BROKER_PORT = 1883; // Porta do Broker MQTT
unsigned long publishUpdate;

unsigned long last_button_change = 0;
typedef enum {
  BUTTON_LONG_PRESS = 1,
  BUTTON_SHORT_PRESS = 2,
} button_press_t;
//--------------------------------------------------------------------

//----------------------Handler Filas---------------------------------
QueueHandle_t button_pressed_queue;
QueueHandle_t timeInterruptQueue;
QueueHandle_t indexColorInterruptQueue;
QueueHandle_t temperatureQueue;
QueueHandle_t humidityQueue;
QueueHandle_t ledControlQueue;
QueueHandle_t colorIndexMqttQueue;
QueueHandle_t timeMqttQueue;
QueueHandle_t timeInfoQueue;
QueueHandle_t colorIndexInfoQueue;
QueueHandle_t ledStateInfoQueue;
//--------------------------------------------------------------------

//-----------------------Tasks----------------------------------------
void taskRGB(void *pvParameters);
void taskButton(void *pvParameters);
void taskDHT22(void *pvParameters);
void taskPrint(void *pvParameters);

//-----------------------Handler Funções-----------------------------
void initWiFi(void);
void initMQTT(void);
void callbackMQTT(char *topic, byte *payload, unsigned int length);
void reconnectMQTT(void);
void reconnectWiFi(void);
void checkWiFIAndMQTT(void);
//------------------------------------------------------------------

//-----------------------Funções Conexão---------------------------
/* Inicializa e conecta-se na rede WI-FI desejada */
void initWiFi(void)
{
  vTaskDelay(10 / portTICK_PERIOD_MS);
  Serial.println("[initWifi] Conexao WI-FI Iniciada");
  Serial.print("[initWifi] Tentando se conectar a rede: ");
  Serial.println(SSID);
  Serial.println("[initWifi] Aguarde...");

  reconnectWiFi();
}

/* Inicializa os parâmetros de conexão MQTT(endereço do broker, porta e seta
  função de callback) */
void initMQTT(void)
{
  MQTT.setServer(BROKER_MQTT, BROKER_PORT); //Informa qual broker e porta deve ser conectado
  MQTT.setCallback(callbackMQTT);           //Atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)
}

/* Função de callback  esta função é chamada toda vez que uma informação
   de um dos tópicos subescritos chega */
void callbackMQTT(char *topic, byte *payload, unsigned int length)
{
  String msg;
  int ledControl;
  
  int timeMqtt;
  int colorIndexMqtt; 

  //Obtem a string do payload recebido
  for (int i = 0; i < length; i++) 
  {
    char c = (char)payload[i];
    msg += c;
  }

  //Serial.printf("Chegou a seguinte string via MQTT: %s do topico: %s\n", msg, topic);

  if (msg.equals("LOW")) 
  {
    ledControl = 0;
    if(xQueueSend(ledControlQueue, &ledControl, portMAX_DELAY) != pdPASS) {}
    //Serial.print("LED RGB desligado por comando MQTT");
  }

  if (msg.equals("HIGH_BUTTON")) 
  {
    ledControl = 1;
    if(xQueueSend(ledControlQueue, &ledControl, portMAX_DELAY) != pdPASS) {}
    //Serial.print("LED RGB ligado por comando MQTT e o Button controla o RGB");
  }
  
  if (msg.equals("HIGH_MQTT")) 
  {
    ledControl = 2;
    if(xQueueSend(ledControlQueue, &ledControl, portMAX_DELAY) != pdPASS) {}
    //Serial.print("LED RGB desligado por comando MQTT e o MQTT controla o RGB");
  }

  if (msg.equals("1000")) 
  {
    timeMqtt = 1000;
    if(xQueueSend(timeMqttQueue, &timeMqtt, portMAX_DELAY) != pdPASS) {}
    //Serial.print("LED RGB desligado por comando MQTT e o MQTT controla o RGB");
  }

  if (msg.equals("300")) 
  {
    timeMqtt = 300;
    if(xQueueSend(timeMqttQueue, &timeMqtt, portMAX_DELAY) != pdPASS) {}
    //Serial.print("LED RGB desligado por comando MQTT e o MQTT controla o RGB");
  }

  if (msg.equals("GREEN")) 
  {
    colorIndexMqtt = 0;
    if(xQueueSend(colorIndexMqttQueue, &colorIndexMqtt, portMAX_DELAY) != pdPASS) {}
   // Serial.print("LED RGB desligado por comando MQTT e o MQTT controla o RGB");
  }

  if (msg.equals("YELLOW")) 
  {
    colorIndexMqtt = 1;
    if(xQueueSend(colorIndexMqttQueue, &colorIndexMqtt, portMAX_DELAY) != pdPASS) {}
    //Serial.print("LED RGB desligado por comando MQTT e o MQTT controla o RGB");
  }

  if (msg.equals("RED")) 
  {
    colorIndexMqtt = 2;
    if(xQueueSend(colorIndexMqttQueue, &colorIndexMqtt, portMAX_DELAY) != pdPASS) {}
    //Serial.print("LED RGB desligado por comando MQTT e o MQTT controla o RGB");
  }

  if (msg.equals("BLUE")) 
  {
    colorIndexMqtt = 3;
    if(xQueueSend(colorIndexMqttQueue, &colorIndexMqtt, portMAX_DELAY) != pdPASS) {}
    //Serial.print("LED RGB desligado por comando MQTT e o MQTT controla o RGB");
  }
}

/* Reconecta-se ao broker MQTT (caso ainda não esteja conectado ou em caso de a conexão cair)
   em caso de sucesso na conexão ou reconexão, o subscribe dos tópicos é refeito. */
void reconnectMQTT(void)
{
  while (!MQTT.connected()) 
  {
    Serial.print("[reconnectMQTT] Tentando se conectar ao Broker MQTT: ");
    Serial.println(BROKER_MQTT);
    if (MQTT.connect(ID_MQTT)) 
    {
      Serial.println("[reconnectMQTT] Conectado com sucesso ao broker MQTT!");
      MQTT.subscribe(TOPIC_SUBSCRIBE_LED_STATE);
    } 
    else 
    {
      Serial.println("[reconnectMQTT] Falha ao reconectar no broker.");
      Serial.println("[reonnectMQTT] Nova tentativa de conexao em 2 segundos.");
      delay(2000);
    }
  }
}

/* Verifica o estado das conexões WiFI e ao broker MQTT.
  Em caso de desconexão (qualquer uma das duas), a conexão é refeita. */
void checkWiFIAndMQTT(void)
{
  if (!MQTT.connected())
    reconnectMQTT(); // se não há conexão com o Broker, a conexão é refeita

  reconnectWiFi(); // se não há conexão com o WiFI, a conexão é refeita
}

void reconnectWiFi(void)
{
  //se já está conectado a rede WI-FI, nada é feito.
  //Caso contrário, são efetuadas tentativas de conexão
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(SSID, PASSWORD); //Conecta na rede WI-FI

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("[reconnectWifi] Conectado com sucesso na rede ");
  Serial.print(SSID);
  Serial.println("");
  Serial.print("[reconnectWifi] IP obtido: ");
  Serial.println(WiFi.localIP());
}
//----------------------------------------------------------------

/* Interrupção BUTTON e discriminação tempo de pressionamento */
void interruptISR()
{
  int state = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if(state == HIGH)
  {
    button_press_t press;
    if(now - last_button_change >= 3000)
    {
      press = BUTTON_LONG_PRESS;
    }
    else
    {
      press = BUTTON_SHORT_PRESS;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(button_pressed_queue, &press, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }

  last_button_change = now;
}

void setup()
{
  Serial.begin(115200);

  // Inicialização do sensor DHT22
  dht.begin();

  LED_RGB.begin();
  LED_RGB.setBrightness(150);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), interruptISR, CHANGE);

  //Filas para task BUTTON
  button_pressed_queue = xQueueCreate(10, sizeof(button_press_t));
  timeInterruptQueue = xQueueCreate(10, sizeof(int));
  indexColorInterruptQueue = xQueueCreate(10, sizeof(int));

  //Filas para task controle led
  ledControlQueue = xQueueCreate(10, sizeof(int));
  timeMqttQueue = xQueueCreate(10, sizeof(int));
  colorIndexMqttQueue = xQueueCreate(10, sizeof(int));

  //Filas para task Info
  temperatureQueue = xQueueCreate(10, sizeof(float));
  humidityQueue = xQueueCreate(10, sizeof(float));
  timeInfoQueue = xQueueCreate(10, sizeof(int));
  colorIndexInfoQueue = xQueueCreate(10, sizeof(int));
  ledStateInfoQueue = xQueueCreate(10, sizeof(int));

  //Inicializa a conexao Wi-Fi
  initWiFi();

  //Inicializa a conexao ao broker MQTT
  initMQTT();

  xTaskCreatePinnedToCore(taskRGB, "RGB Task", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskButton, "Button Task", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskDHT22, "DHT22 Task", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskPrint, "Print Task", 8192, NULL, 1, NULL, 0);
}

void loop()
{
  vTaskDelete(NULL);
}

void taskRGB(void *pvParameters) 
{
  (void) pvParameters;

  int timeReceived = 1000; 
  int indexColorReceived = 0;
  int ledControl = 1;
  
  int timeMqtt = 300;
  int indexColorMqtt = 0;

  int ledState = 1;

  //Array de cores
  uint32_t colors[] = 
  {
    LED_RGB.Color(0, 255, 0),   //Cor verde
    LED_RGB.Color(255, 255, 0), //Cor amarela
    LED_RGB.Color(255, 0, 0),   //Cor vermelha
    LED_RGB.Color(0, 0, 255)    //Cor azul
       
  };

  while (true) 
  {
    if(xQueueReceive(ledControlQueue, &ledControl, 0) == pdPASS){}

    switch(ledControl)
    {
      case 0:
        LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(0, 0, 0)));
        LED_RGB.show(); //Apagar LED
        
        ledState = 0;
        if(xQueueSend(ledStateInfoQueue, &ledState, 0) != pdPASS){}
      break;

      case 1:
        if(xQueueReceive(timeInterruptQueue, &timeReceived, 0) == pdPASS){}
        if(xQueueReceive(indexColorInterruptQueue, &indexColorReceived, 0) == pdPASS){}
        
        LED_RGB.setPixelColor(0, uint32_t(colors[indexColorReceived]));
        LED_RGB.show(); //Acender LED
        vTaskDelay(timeReceived / portTICK_PERIOD_MS); 

        LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(0, 0, 0)));
        LED_RGB.show(); //Apagar LED
        vTaskDelay(timeReceived / portTICK_PERIOD_MS);

        if(xQueueSend(timeInfoQueue, &timeReceived, 0) != pdPASS){}
        if(xQueueSend(colorIndexInfoQueue, &indexColorReceived, 0) != pdPASS){}

        ledState = 1;
        if(xQueueSend(ledStateInfoQueue, &ledState, 0) != pdPASS){}
      break;

      case 2:
        if(xQueueReceive(timeMqttQueue, &timeMqtt, 0) == pdPASS){}
        if(xQueueReceive(colorIndexMqttQueue, &indexColorMqtt, 0) == pdPASS){}
        
        LED_RGB.setPixelColor(0, uint32_t(colors[indexColorMqtt]));
        LED_RGB.show(); //Acender LED
        vTaskDelay(timeMqtt / portTICK_PERIOD_MS); 

        LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(0, 0, 0)));
        LED_RGB.show(); //Apagar LED
        vTaskDelay(timeMqtt / portTICK_PERIOD_MS);

        if(xQueueSend(timeInfoQueue, &timeMqtt, 0) != pdPASS){}
        if(xQueueSend(colorIndexInfoQueue, &indexColorMqtt, 0) != pdPASS){}

        ledState = 1;
        if(xQueueSend(ledStateInfoQueue, &ledState, 0) != pdPASS){}
      break;
    } 
  }
}

void taskButton(void *pvParameters)
{
  (void) pvParameters;

  button_press_t press;
  
  int index_blink = 1;
  int index_color = 1;
  int time_blink[2] = {1000, 300};

  while (true) 
  {
    if(xQueueReceive(button_pressed_queue, &press, portMAX_DELAY))
    {
      //Serial.print("Botão pressionado!");

      switch(press)
      {
        case 1:
          //Serial.println(" Pressionamento longo!");

          if(xQueueSend(timeInterruptQueue, &time_blink[index_blink], portMAX_DELAY) != pdPASS) {}
          index_blink = (index_blink + 1) % 2;
        
        break;
        case 2:
          //Serial.println(" Pressionamento curto!");
          
          if(xQueueSend(indexColorInterruptQueue, &index_color, portMAX_DELAY) != pdPASS) {}
          index_color = (index_color + 1) % 4; 
       
        break;
      }
    }
  }
}

void taskDHT22(void *pvParameters)
{
  (void) pvParameters;

  while (true) 
  {
    // Obtenção dos dados de temperatura e umidade do sensor
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    // Envia um dado para a fila
    if (xQueueSend(temperatureQueue, &temperature, portMAX_DELAY) != pdPASS){}

    if(xQueueSend(humidityQueue, &humidity, portMAX_DELAY) != pdPASS){}

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void taskPrint(void *pvParameters)
{
  (void) pvParameters;

  float temperature;
  float humidity;
  int ledState = 1;
  int ledTemp = 1000;
  int ledColor = 0;

  while (true) 
  {

    if (xQueueReceive(temperatureQueue, &temperature, portMAX_DELAY) == pdPASS){}
    if (xQueueReceive(humidityQueue, &humidity, portMAX_DELAY) == pdPASS){}
    if (xQueueReceive(timeInfoQueue, &ledTemp, 0) == pdPASS){}
    if (xQueueReceive(colorIndexInfoQueue, &ledColor, 0) == pdPASS){}
    if (xQueueReceive(ledStateInfoQueue, &ledState, 0) == pdPASS){}

    /* Repete o ciclo após 3 segundos */
    if ((millis() - publishUpdate) >= PUBLISH_DELAY) {
      
      publishUpdate = millis();
      // Verifica o funcionamento das conexões WiFi e ao broker MQTT
      checkWiFIAndMQTT();
      
      //Cria um buffer para o JSON
      DynamicJsonDocument jsonBuffer(256);
      JsonObject json = jsonBuffer.to<JsonObject>();

      //Adiciona os dados ao objeto JSON
      json["temperature"] = temperature;
      json["humidity"] = humidity;
      json["led_state"] = ledState;
      json["led_temp"] = ledTemp;
      json["led_color"] = ledColor;

      //Serializa o objeto JSON para uma string
      char jsonString[256];
      serializeJson(json, jsonString, sizeof(jsonString));

      //Imprime a string JSON no serial
      Serial.println(jsonString);

      //Publica a string JSON no tópico MQTT
      MQTT.publish(TOPIC_PUBLISH_DATA, jsonString);

      //Keep-alive da comunicação com broker MQTT
      MQTT.loop();
    }
  }
}
