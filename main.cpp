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
const uint8_t WIN_SCORE = 3;

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

// Patrón del juego

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
  GameController(PatternManager& pm,
                 LEDDriver& leds,
                 ButtonReader& buttons,
                 Buzzer& buzzer,
                 DisplayLCD& display)
    : pm_(pm), leds_(leds), buttons_(buttons),
      buzzer_(buzzer), display_(display),
      state_(State::IDLE),
      level_(0), indexPattern_(0), indexInput_(0),
      lastChange_(0), ledOn_(false),
      score_(0), highScore_(0),
      won_(false) {}

  void begin() {
    pm_.begin();
    level_ = 0;
    score_ = 0;
    won_ = false;
    display_.begin();
    display_.showPressToStart();
  }

  void loop() {
    buttons_.update();

    switch (state_) {
      case State::IDLE:         handleIdle();         break;
      case State::SHOW_PATTERN: handleShowPattern();  break;
      case State::WAIT_INPUT:   handleWaitInput();    break;
      case State::GAME_OVER:    handleGameOver();     break;
    }
  }

private:
  PatternManager& pm_;
  LEDDriver& leds_;
  ButtonReader& buttons_;
  Buzzer& buzzer_;
  DisplayLCD& display_;

  State state_;
  uint8_t level_;
  uint8_t indexPattern_;
  uint8_t indexInput_;
  unsigned long lastChange_;
  bool ledOn_;
  int score_;
  int highScore_;
  bool won_;

  void changeState(State s) {
    state_ = s;
    lastChange_ = millis();
  }

  void handleIdle() {
    if (buttons_.anyRisingEdge() != 0xFF) {
      leds_.offAll();
      pm_.reset();
      level_ = 1;
      score_ = 0;
      won_ = false;
      pm_.addStep();
      indexPattern_ = 0;
      ledOn_ = false;
      display_.showLevel(level_, highScore_);
      changeState(State::SHOW_PATTERN);
    }
  }

  void handleShowPattern() {
    unsigned long now = millis();
    const unsigned long onTime = 400;
    const unsigned long offTime = 200;

    if (indexPattern_ >= pm_.length()) {
      leds_.offAll();
      indexInput_ = 0;
      changeState(State::WAIT_INPUT);
      return;
    }

    if (!ledOn_) {
      uint8_t ledIdx = pm_.getStep(indexPattern_);
      leds_.offAll();
      leds_.on(ledIdx);
      buzzer_.click(ledIdx);
      ledOn_ = true;
      lastChange_ = now;
    } else {
      if (now - lastChange_ >= onTime) {
        leds_.offAll();
        ledOn_ = false;
        lastChange_ = now + offTime;
        ++indexPattern_;
      }
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

  void handleGameOver() {
    unsigned long now = millis();

    // Parpadeo distinto si ganó o perdió
    if (won_) {
      // Parpadeo más lento
      if ((now / 400) % 2 == 0) {
        for (uint8_t i = 0; i < leds_.count(); ++i) leds_.on(i);
      } else {
        leds_.offAll();
      }
    } else {
      // Parpadeo rápido de "fail"
      if ((now / 200) % 2 == 0) {
        for (uint8_t i = 0; i < leds_.count(); ++i) leds_.on(i);
      } else {
        leds_.offAll();
      }
    }

    // Pulsar cualquier botón para volver a IDLE
    if (buttons_.anyRisingEdge() != 0xFF) {
      leds_.offAll();
      display_.showPressToStart();
      changeState(State::IDLE);
    }
  }
};

// Instancias globales

LEDDriver      leds(LED_PINS, 4);
ButtonReader   buttons(BUTTON_PINS, 4, 25);
Buzzer         buzzer(BUZZER_PIN);
DisplayLCD     display;
PatternManager pattern(4, 50);
GameController game(pattern, leds, buttons, buzzer, display);

// LOOP

void setup() {
  leds.begin();
  buttons.begin();
  buzzer.begin();
  game.begin();
}

void loop() {
  game.loop();
}