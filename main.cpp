#include <Arduino.h>
#include <LiquidCrystal.h>

// Configuración de los pines

// Botones: un lado al pin, el otro a GND (tierra)
const uint8_t BUTTON_PINS[4] = {2, 3, 4, 5};

// LEDs: pata larga en la resistencia y al pin, pata corta a tierra
const uint8_t LED_PINS[4] = {8, 9, 10, 11};

// Buzzer pequeño
const uint8_t BUZZER_PIN = 6;

// LCD paralelo 16x2: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(A0, A1, A2, A3, A4, A5);

// Puntos necesarios para ganar
const uint8_t WIN_SCORE = 10;

void setup() {}

void loop() {}
