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

class Buzzer {
public:
    explicit Buzzer(uint8_t pin) : pin_(pin) {}

    void begin() {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, LOW);
    }

    void beep(uint16_t ms, unsigned int freq) {
        tone(pin_, freq, ms);
    }

    void click(uint8_t idx) {
        static const unsigned int tones[4] = {800, 950, 1100, 1250};
        if (idx < 4) beep(120, tones[idx]);
    }

    void success() {
        beep(150, 1500);
        delay(50);
        beep(150, 1800);
        delay(50);
        beep(200, 2000);
    }

    void fail() {
        beep(300, 300);
        delay(100);
        beep(250, 200);
    }

private:
    uint8_t pin_;
};

class DisplayLCD {
public:
    void begin() {
        lcd.begin(16, 2);
        lcd.clear();
    }

    void showWelcome(int highScore) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SIMON DICE");
        lcd.setCursor(0, 1);
        lcd.print("High: ");
        lcd.print(highScore);
    }

    void showLevel(uint8_t level, int highScore) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nivel: ");
        lcd.print(level);
        lcd.setCursor(0, 1);
        lcd.print("High: ");
        lcd.print(highScore);
    }

    void showGameOver(int score, int highScore) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Game Over");
        lcd.setCursor(0, 1);
        lcd.print("You: ");
        lcd.print(score);
        lcd.print(" H:");
        lcd.print(highScore);
    }

    void showPressToStart() {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Presiona un boton");
        lcd.setCursor(0, 1);
        lcd.print("para iniciar");
    }

    void showWin(int score, int highScore) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("!GANASTE!");
        lcd.setCursor(0, 1);
        lcd.print("Score: ");
        lcd.print(score);
        lcd.print(" H:");
        lcd.print(highScore);
    }
};

class PatternManager {
public:
    PatternManager(uint8_t colors, uint8_t maxLen = 50)
      : colors_(colors), maxLen_(maxLen), length_(0) {}

    void begin() {
        length_ = 0;
        randomSeed(analogRead(0));
    }

    void reset() {
        length_ = 0;
    }

    void addStep() {
        if (length_ < maxLen_) {
            pattern_[length_] = (uint8_t)random(0, colors_);
            ++length_;
        }
    }

    uint8_t getStep(uint8_t idx) const {
        return pattern_[idx];
    }

    uint8_t length() const {
        return length_;
    }

private:
    uint8_t colors_;
    uint8_t maxLen_;
    uint8_t length_;
    uint8_t pattern_[50];
};

// FSM DEL JUEGO

enum class State {
    IDLE,
    SHOW_PATTERN,
    WAIT_INPUT,
    GAME_OVER
  };

class GameController {
public:
    GameController(...)
    {}

    void begin(){}
    void loop(){}
private:
    void handleIdle(){
        if(buttons.anyRisingEdge() != 0xFF){
            pm.reset();
            pm.addStep();
            state = State::SHOW_PATTERN;
        }
    }

    void handleShowPattern(){
        uint8_t s = pm.get(index);
        leds.on(s);
        delay(300);
        leds.offAll();
        delay(200);
        index++;
        if(index >= pm.length()){
            index = 0;
            state = State::WAIT_INPUT;
        }
    }

    void handleWaitInput() {
        uint8_t btn = buttons_.anyRisingEdge();
        if (btn == 0xFF) return;

        leds_.on(btn);
        buzzer_.click(btn);
        delay(120);
        leds_.off(btn);

        if (btn == pm_.getStep(indexInput_)) {
            ++indexInput_;
            if (indexInput_ >= pm_.length()) {
                // ronda completa
                score_ = pm_.length();
                if (score_ > highScore_) {
                    highScore_ = score_;
                }

                // ganó?
                if (score_ >= WIN_SCORE) {
                    won_ = true;
                    buzzer_.success();
                    display_.showWin(score_, highScore_);
                    changeState(State::GAME_OVER);
                    return;
                }

                level_++;
                pm_.addStep();
                indexPattern_ = 0;
                ledOn_ = false;
                display_.showLevel(level_, highScore_);
                changeState(State::SHOW_PATTERN);
            }
        } else {
            // Falló
            won_ = false;
            buzzer_.fail();
            display_.showGameOver(score_, highScore_);
            changeState(State::GAME_OVER);
        }
    }
    }

    void handleGameOver(){}
};

// Instancias globales

LEDDriver      leds(LED_PINS, 4);
ButtonReader buttons(BUTTON_PINS,4);
Buzzer buzzer(BUZZER_PIN);
DisplayLCD display;

void setup() {
    leds.begin();
    buttons.begin();
    buzzer.begin();
    display.begin();
}

void loop() {}
