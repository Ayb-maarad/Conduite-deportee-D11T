#include <PubSubClient.h>
#include <WiFi.h>
#include <FreeRTOS.h>
#include "task.h"
#include "queue.h"

const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

const int slideSwitchPin = 6;
const int potPin = 5;
const int accelPin = 7;
const int decelPin = 15;
const int parkingPin = 16;

int mappedValue;
int vitesse = 0;
int x = 0;
int speedPosition = 0; 
bool stopRPM = false;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

TaskHandle_t SwitchTaskHandle = NULL;
TaskHandle_t accelTaskHandle = NULL;
TaskHandle_t parkingTaskHandle = NULL;
TaskHandle_t rpmstartTask1Handle = NULL;

void SwitchTask(void* parameter);
void accelTask(void* p);
void rpmstartTask1(void *p);
void  publishMQTT();
void updateVitesse();
void reconnectMQTT();
void updateVitesseWithPot();

void setup() {
  Serial.begin(9600);
  Serial.print("Connecting to WiFi");
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  mqttClient.setServer(mqttServer, mqttPort);

  pinMode(slideSwitchPin, INPUT);
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }

  xTaskCreatePinnedToCore(SwitchTask, "SwitchTask", 4096, NULL, 1, &SwitchTaskHandle, 0);
  xTaskCreatePinnedToCore(accelTask, "accelTask", 4096, NULL, 1, &accelTaskHandle, 1);
  

 
}

void loop() {}

void SwitchTask(void* parameter) {
  bool executed = false;
  int lastSwitchState = digitalRead(slideSwitchPin);

  while (true) {
    int currentSwitchState = digitalRead(slideSwitchPin);
    if (currentSwitchState == 1) {
      
      
      if (!executed) {
        mqttClient.publish("pfe/switch", "true");
        Serial.println("true");

        xTaskCreatePinnedToCore(rpmstartTask1, "rpmstartTask1", 4096, NULL, 2, &rpmstartTask1Handle, 1);

        vTaskSuspend(accelTaskHandle);
        vTaskSuspend(SwitchTaskHandle);
        executed = true;
        stopRPM = false;
        vTaskResume(accelTaskHandle);
      }
    } else {
      if (executed) {
        vTaskSuspend(accelTaskHandle);  
        mqttClient.publish("pfe/switch", "false");
        Serial.println("false");
        executed = false;
      }
    }
    lastSwitchState = currentSwitchState;
    vTaskDelay(50);
  }
}

void accelTask(void* p) {
  bool buttonPressed = false;
  bool buttonPressed2 = false;
  bool executed = false;
  int parking = LOW;

  while (true) {
    int pushaccel = digitalRead(accelPin);
    int pushdecel = digitalRead(decelPin);
    int potValue = analogRead(potPin);
    mappedValue = map(potValue, 0, 4095, 0, 100);
    parking = digitalRead(parkingPin);
   
    if (parking == HIGH && !executed) {
      mqttClient.publish("pfe/parking", "false");
      executed = true;
    }

    if (pushaccel == HIGH && !buttonPressed) {
      buttonPressed = true; 
      speedPosition++; 
      speedPosition = constrain(speedPosition, -3, 3);
      updateVitesse();
      Serial.println(vitesse);
    } else if (pushaccel == LOW) {
      buttonPressed = false; 
    }
    
    if (pushdecel == HIGH && !buttonPressed2) {
      buttonPressed2 = true; 
      speedPosition--; 
      speedPosition = constrain(speedPosition, -3, 3);
      updateVitesse();
    } else if (pushdecel == LOW) {
      buttonPressed2 = false;
    }
  
    if (mappedValue > 0) {
      updateVitesseWithPot();
    } else {
      vitesse = x; 
    }

    publishMQTT();
    vTaskDelay(50);
  }
}

void rpmstartTask1(void *p) {
  int rpm = 0;
  bool increasing = true;

  while (!stopRPM) {
    char message5[5];
    sprintf(message5, "%d", rpm);
    mqttClient.publish("pfe/rpm", message5);

    if (increasing) {
      rpm += 500;
      if (rpm >= 3000) {
        increasing = false;
      }
    } else {
      rpm -= 500;
      if (rpm <= 0) {
        stopRPM = true;
        mqttClient.publish("pfe/svg", "false");
        vTaskResume(SwitchTaskHandle);
      }
    }
    vTaskDelay(70);
  }
  vTaskDelete(NULL); 
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-S3")) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 5 seconds...");
      
    }
  }
}


void updateVitesse() {
  if (speedPosition == 0) {
    vitesse = 0;
    x = 0;
  } else if (speedPosition == 1) {
    vitesse = 4;
    x = 4;
  } else if (speedPosition == 2) {
    vitesse = 7;
    x = 7;
  } else if (speedPosition == 3) {
    vitesse = 12;
    x = 12;
  } else if (speedPosition == -1) {
    vitesse = 5;
    x = 5;
  } else if (speedPosition == -2) {
    vitesse = 8;
    x = 8;
  } else if (speedPosition == -3) {
    vitesse = 14;
    x = 14;
  }
}

void updateVitesseWithPot() {
  if (mappedValue < 20) {
    vitesse -= 0.000001;
  } else if (mappedValue < 40) {
    vitesse -= 0.000005;
  } else if (mappedValue < 60) {
    vitesse -= 0.000008;
  } else if (mappedValue < 90) {
    vitesse -= 0.00001;
  } else if (mappedValue == 100) {
    vitesse -= 0.00005;
  }
}

void publishMQTT() {
  char message3[5];
  sprintf(message3, "%d", vitesse);
  mqttClient.publish("pfe/vitesse", message3);

  char message[2];
  sprintf(message, "%d", speedPosition);
  mqttClient.publish("pfe/transmission", message);

  char message4[3];
  sprintf(message4, "%d", mappedValue);
  mqttClient.publish("pfe/pot", message4);
}
