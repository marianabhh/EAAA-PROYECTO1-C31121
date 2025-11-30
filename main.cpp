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

// Clases para los componentes de hardware :)

class LEDDriver {
public:
    LEDDriver(const uint8_t* pins, uint8_t count) : pins_(pins), count_(count) {}

    void begin() {
        for (uint8_t i = 0; i < count_; ++i) {
            pinMode(pins_[i], OUTPUT);
            digitalWrite(pins_[i], LOW);
        }
    }

    void on(uint8_t idx) {
        if (idx < count_) digitalWrite(pins_[idx], HIGH);
    }

    void off(uint8_t idx) {
        if (idx < count_) digitalWrite(pins_[idx], LOW);
    }

    void offAll() {
        for (uint8_t i = 0; i < count_; ++i) {
            digitalWrite(pins_[i], LOW);
        }
    }

    uint8_t count() const { return count_; }

private:
    const uint8_t* pins_;
    uint8_t count_;
};

class ButtonReader {
public:
    ButtonReader(const uint8_t* pins, uint8_t count, uint16_t debounceMs = 25)
      : pins_(pins), count_(count), debounceMs_(debounceMs) {
        for (uint8_t i = 0; i < 4; ++i) {
            curr_[i] = prev_[i] = HIGH;
            lastChange_[i] = 0;
            edge_[i] = false;
        }
    }

    void begin() {
        for (uint8_t i = 0; i < count_; ++i) {
            pinMode(pins_[i], INPUT_PULLUP);
            curr_[i] = prev_[i] = digitalRead(pins_[i]);
            lastChange_[i] = millis();
            edge_[i] = false;
        }
    }

    void update() {
        unsigned long now = millis();
        for (uint8_t i = 0; i < count_; ++i) {
            uint8_t r = digitalRead(pins_[i]);
            edge_[i] = false;
            if (r != curr_[i] && (now - lastChange_[i] >= debounceMs_)) {
                prev_[i] = curr_[i];
                curr_[i] = r;
                lastChange_[i] = now;
                if (prev_[i] == HIGH && curr_[i] == LOW) {
                    edge_[i] = true;
                }
            }
        }
    }

    bool isPressed(uint8_t idx) const {
        return (idx < count_) ? (curr_[idx] == LOW) : false;
    }

    bool risingEdge(uint8_t idx) const {
        return (idx < count_) ? edge_[idx] : false;
    }

    uint8_t anyRisingEdge() const {
        for (uint8_t i = 0; i < count_; ++i) {
            if (edge_[i]) return i;
        }
        return 0xFF;
    }

private:
    const uint8_t* pins_;
    uint8_t count_;
    uint16_t debounceMs_;
    uint8_t curr_[4];
    uint8_t prev_[4];
    unsigned long lastChange_[4];
    bool edge_[4];
};

// Instancias globales

LEDDriver      leds(LED_PINS, 4);
ButtonReader buttons(BUTTON_PINS,4);

void setup() {
    leds.begin();
    buttons.begin();
}

void loop() {}
