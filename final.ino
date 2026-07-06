#include <FastLED.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// === ПИНЫ ===
#define MOTOR_LA A2
#define MOTOR_LB A3
#define MOTOR_RA A4
#define MOTOR_RB A5

#define IR_FRONT 10
#define IR_BACK  9

#define BTN_1 8
#define BTN_2 7
#define BTN_3 6
#define BTN_4 5

#define MATRIX_PIN 13
#define BATTERY_PIN A1

// === НАСТРОЙКА МАТРИЦЫ ===
#define NUM_LEDS 64
CRGB leds[NUM_LEDS];
#define COLOR_ORDER GRB

// === НАСТРОЙКА БАТАРЕИ ===
#define BATTERY_MAX 820
#define BATTERY_MIN 650

// === DFPLAYER (SoftwareSerial) ===
SoftwareSerial mySoftwareSerial(12, 11);
DFRobotDFPlayerMini myDFPlayer;

// === ПЕРЕМЕННЫЕ ===
enum RobotMode { MODE_GOOD, MODE_EVIL };
RobotMode currentMode = MODE_GOOD;

bool isBusy = false;
bool isPlaying = false;
bool isIdle = false;

bool lastBtn1 = LOW, lastBtn2 = LOW, lastBtn3 = LOW, lastBtn4 = LOW;
unsigned long lastBtnTime = 0;
const unsigned long debounceDelay = 150;
unsigned long lastDFPlayerCommand = 0;
const unsigned long dfPlayerDelay = 200;
unsigned long lastBatteryCheck = 0;

unsigned long lastRandomMove = 0;
unsigned long lastPhraseTime = 0;
unsigned long nextPhraseDelay = 5000;
unsigned long lastActivityTime = 0;

unsigned long buttonMoveStartTime = 0;
unsigned long buttonMoveDuration = 0;
bool isButtonMoving = false;
int buttonAction = 0;

// Переменные для обычного движения
unsigned long moveStartTime = 0;
unsigned long moveDuration = 0;
bool isMoving = false;

bool wasFrontAbyss = false;
bool wasBackAbyss = false;
bool wasAir = false;
bool airSoundPlayed = false;
unsigned long airTimer = 0;

int batteryPercent = 100;
CRGB lastBatteryColor = CRGB(0, 255, 0);

// ==========================================
//          ЦВЕТ ГЛАЗ
// ==========================================
CRGB getModeEyeColor() {
  if (currentMode == MODE_GOOD) {
    return CRGB(255, 255, 0);
  } else {
    return CRGB(255, 0, 0);
  }
}

// === НАСТРОЙКА ===
void setup() {  
  pinMode(MOTOR_LA, OUTPUT);
  pinMode(MOTOR_LB, OUTPUT);
  pinMode(MOTOR_RA, OUTPUT);
  pinMode(MOTOR_RB, OUTPUT);
  
  pinMode(IR_FRONT, INPUT);
  pinMode(IR_BACK, INPUT);
  
  pinMode(BTN_1, INPUT);
  pinMode(BTN_2, INPUT);
  pinMode(BTN_3, INPUT);
  pinMode(BTN_4, INPUT);
  
  pinMode(BATTERY_PIN, INPUT);

  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    while (true) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(500);
    }
  }
  myDFPlayer.volume(24);
  
  playSound(1);
  
  FastLED.addLeds<WS2812B, MATRIX_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  FastLED.clear();
  FastLED.show();

  playBootSequence();
  
  lastPhraseTime = millis();
  nextPhraseDelay = random(10000, 20000);
  lastDFPlayerCommand = millis();
  lastActivityTime = millis();
  lastBatteryCheck = millis();
  
  batteryPercent = getBatteryPercent();
  lastBatteryColor = getBatteryColor(batteryPercent);
  drawModeEyes();
  FastLED.show();
  
}

// ==========================================
//          ЗВУК
// ==========================================
void playSound(int fileNumber) {
  if (isPlaying) return;
  if (millis() - lastDFPlayerCommand < dfPlayerDelay) return;
  
  lastDFPlayerCommand = millis();
  isPlaying = true;
  
  myDFPlayer.play(fileNumber);
}

void checkDFPlayerStatus() {
  if (isPlaying) {
    if (myDFPlayer.available()) {
      uint8_t type = myDFPlayer.readType();
      if (type == DFPlayerPlayFinished) {
        isPlaying = false;
      }
    }
    if (millis() - lastDFPlayerCommand > 3000) {
      isPlaying = false;
    }
  }
}

// ==========================================
//          БАТАРЕЯ
// ==========================================
CRGB getBatteryColor(int percent) {
  if (percent > 70) return CRGB(0, 255, 0);
  else if (percent > 40) return CRGB(255, 255, 0);
  else if (percent > 20) return CRGB(255, 165, 0);
  else return CRGB(255, 0, 0);
}

int getBatteryPercent() {
  int raw = analogRead(BATTERY_PIN);
  debugRawValue = raw;
  
  int mapped = raw;
  if (mapped > BATTERY_MAX) mapped = BATTERY_MAX;
  if (mapped < BATTERY_MIN) mapped = BATTERY_MIN;
  
  int percent = map(mapped, BATTERY_MIN, BATTERY_MAX, 0, 100);
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  debugPercent = percent;
  
  return percent;
}

void drawBatteryIndicator() {
  CRGB currentColor = getBatteryColor(batteryPercent);
  
  lastBatteryColor = currentColor;
  
  leds[0] = currentColor;

  
  FastLED.show();
}

// ==========================================
//          ГЛАВНЫЙ ЦИКЛ
// ==========================================
void loop() {
  unsigned long currentMillis = millis();

  checkSensors();

  checkDFPlayerStatus();

  // Батарея
  if (currentMillis - lastBatteryCheck > 1000) {
    lastBatteryCheck = currentMillis;
    batteryPercent = getBatteryPercent();
    drawBatteryIndicator();
  }

  // Обработка движенийф
  handleMovement(currentMillis);
  handleButtonMovement(currentMillis);  // 🔥 Для движений по кнопкам

  // Кнопки
  if (!isBusy && !isPlaying) {
    handleButtons(currentMillis);
  }
  
  handleIdle(currentMillis);
  
  FastLED.show();
}

// ==========================================
//          СИСТЕМА СПОКОЙСТВИЯ
// ==========================================
void handleIdle(unsigned long current) {
  // Если робот занят, в воздухе или играет звук — выходим из спокойствия
  if (isBusy || wasAir || isPlaying || isMoving || isButtonMoving) {
    if (isIdle) {
      isIdle = false;
      drawModeEyes();  // Возвращаем глаза режима
    }
    return;
  }
  
  // Проверяем, прошло ли 5 секунд без активности
  if (current - lastActivityTime > 5000) {
    // Входим в спокойствие (только один раз)
    if (!isIdle) {
      isIdle = true;

    }
    
    // Случайные движения в спокойствии (не сбрасывают таймер!)
    if (current - lastRandomMove > 3000 + random(0, 3000)) {
      lastRandomMove = current;
      if (!isMoving && !isButtonMoving) {
        int action = random(1, 5);
        int duration = 800 + random(0, 1200);
        startMove(action, duration);
      }
    }
    
    // Случайные фразы в спокойствии
    if (current - lastPhraseTime > nextPhraseDelay) {
      lastPhraseTime = current;
      nextPhraseDelay = random(10000, 20000);  // 🔥 Новый случайный интервал
      playRandomPhrase();
    }
  } else {
    // Если активность была меньше 5 секунд назад — выходим из спокойствия
    if (isIdle) {
      isIdle = false;
      drawModeEyes();
    }
  }
}

// ==========================================
//          ДВИЖЕНИЯ
// ==========================================
void moveForward() {
  digitalWrite(MOTOR_LA, LOW); digitalWrite(MOTOR_LB, HIGH);
  digitalWrite(MOTOR_RA, LOW); digitalWrite(MOTOR_RB, HIGH);
}

void moveBackward() {
  digitalWrite(MOTOR_LA, HIGH); digitalWrite(MOTOR_LB, LOW);
  digitalWrite(MOTOR_RA, HIGH); digitalWrite(MOTOR_RB, LOW);
}

void turnLeft() {
  digitalWrite(MOTOR_LA, LOW); digitalWrite(MOTOR_LB, HIGH);
  digitalWrite(MOTOR_RA, HIGH); digitalWrite(MOTOR_RB, LOW);
}

void turnRight() {
  digitalWrite(MOTOR_LA, HIGH); digitalWrite(MOTOR_LB, LOW);
  digitalWrite(MOTOR_RA, LOW); digitalWrite(MOTOR_RB, HIGH);
}

void stopMotors() {
  digitalWrite(MOTOR_LA, LOW); digitalWrite(MOTOR_LB, LOW);
  digitalWrite(MOTOR_RA, LOW); digitalWrite(MOTOR_RB, LOW);
}

// 🔥 Обычное движение (для случайных движений и датчиков)
void startMove(int action, unsigned long duration) {
  isMoving = true;
  moveStartTime = millis();
  moveDuration = duration;
  
  switch (action) {
    case 1: moveForward(); break;
    case 2: moveBackward(); break;
    case 3: turnLeft(); break;
    case 4: turnRight(); break;
  }
}

void handleMovement(unsigned long current) {
  if (isMoving) {
    if (current - moveStartTime >= moveDuration) {
      stopMotors();
      isMoving = false;
      // 🔥 НЕ сбрасываем lastActivityTime!
    }
  }
}

// 🔥 Движение по кнопкам (отдельный таймер)
void startButtonMove(int action, unsigned long duration) {
  isButtonMoving = true;
  buttonMoveStartTime = millis();
  buttonMoveDuration = duration;
  buttonAction = action;
  lastActivityTime = millis();  // 🔥 ТОЛЬКО здесь сбрасываем!
  
  switch (action) {
    case 1: moveForward(); break;
    case 2: moveBackward(); break;
    case 3: turnLeft(); break;
    case 4: turnRight(); break;
  }
}

void handleButtonMovement(unsigned long current) {
  if (isButtonMoving) {
    if (current - buttonMoveStartTime >= buttonMoveDuration) {
      stopMotors();
      isButtonMoving = false;
      // Не сбрасываем lastActivityTime здесь, чтобы дать роботу успокоиться
    }
  }
}

// ==========================================
//          МАТРИЦА
// ==========================================
int getPixelIndex(int x, int y) {
  if (y % 2 == 0) return y * 8 + x;
  else return y * 8 + (7 - x);
}

void drawEye(int startX, int startY, int size, CRGB color) {
  for (int y = startY; y < startY + size; y++) {
    for (int x = startX; x < startX + size; x++) {
      if (x >= 0 && x < 8 && y >= 0 && y < 8) {
        leds[getPixelIndex(x, y)] = color;
      }
    }
  }
}

void drawLine(int x1, int y1, int x2, int y2, CRGB color) {
  if (x1 == x2) {
    for (int y = min(y1, y2); y <= max(y1, y2); y++) {
      if (x1 >= 0 && x1 < 8 && y >= 0 && y < 8) {
        leds[getPixelIndex(x1, y)] = color;
      }
    }
  } else if (y1 == y2) {
    for (int x = min(x1, x2); x <= max(x1, x2); x++) {
      if (x >= 0 && x < 8 && y1 >= 0 && y1 < 8) {
        leds[getPixelIndex(x, y1)] = color;
      }
    }
  }
}

void drawModeEyes() {
  FastLED.clear();
  CRGB eyeColor = getModeEyeColor();
  drawEye(2, 2, 2, eyeColor);
  drawEye(5, 2, 2, eyeColor);
  drawBatteryIndicator();
}

void drawHappyFace() {
  FastLED.clear();
  CRGB eyeColor = getModeEyeColor();
  CRGB color = CRGB(255, 255, 0);
  drawEye(1, 1, 2, eyeColor);
  drawEye(5, 1, 2, eyeColor);
  leds[getPixelIndex(2, 5)] = color;
  leds[getPixelIndex(3, 5)] = color;
  leds[getPixelIndex(4, 5)] = color;
  leds[getPixelIndex(5, 5)] = color;
  leds[getPixelIndex(1, 6)] = color;
  leds[getPixelIndex(6, 6)] = color;
  drawBatteryIndicator();
}

void drawAngryFace() {
  FastLED.clear();
  CRGB eyeColor = getModeEyeColor();
  CRGB color = CRGB(255, 0, 0);
  drawLine(1, 0, 3, 1, color);
  drawLine(4, 1, 6, 0, color);
  drawEye(1, 2, 2, eyeColor);
  drawEye(5, 2, 2, eyeColor);
  drawLine(2, 6, 5, 6, color);
  drawLine(2, 7, 5, 7, color);
  leds[getPixelIndex(2, 6)] = CRGB::Black;
  leds[getPixelIndex(4, 6)] = CRGB::Black;
  drawBatteryIndicator();
}

void drawSadFace() {
  FastLED.clear();
  CRGB eyeColor = getModeEyeColor();
  CRGB color = CRGB(0, 100, 255);
  drawEye(1, 1, 2, eyeColor);
  drawEye(5, 1, 2, eyeColor);
  leds[getPixelIndex(2, 3)] = color;
  leds[getPixelIndex(6, 3)] = color;
  leds[getPixelIndex(2, 6)] = color;
  leds[getPixelIndex(5, 6)] = color;
  drawLine(3, 7, 4, 7, color);
  drawBatteryIndicator();
}

void drawSurprised() {
  FastLED.clear();
  CRGB eyeColor = getModeEyeColor();
  CRGB color = CRGB(255, 100, 255);
  drawEye(0, 1, 3, eyeColor);
  drawEye(5, 1, 3, eyeColor);
  drawEye(3, 5, 2, color);
  drawBatteryIndicator();
}

void drawEvilLaugh() {
  FastLED.clear();
  CRGB eyeColor = getModeEyeColor();
  CRGB color = CRGB(200, 0, 100);
  for (int i = 0; i < 8; i++) {
    leds[getPixelIndex(i, 2)] = color;
    leds[getPixelIndex(i, 3)] = color;
  }
  drawEye(1, 2, 2, eyeColor);
  drawEye(5, 2, 2, eyeColor);
  drawLine(1, 6, 6, 6, color);
  drawLine(1, 7, 6, 7, color);
  leds[getPixelIndex(2, 6)] = CRGB::Black;
  leds[getPixelIndex(5, 6)] = CRGB::Black;
  drawBatteryIndicator();
}

void drawScary() {
  FastLED.clear();
  CRGB eyeColor = getModeEyeColor();
  CRGB color = CRGB(255, 0, 50);
  for (int i = 0; i < 64; i++) {
    if (random(0, 100) > 70) {
      leds[i] = color;
    }
  }
  drawEye(2, 1, 2, eyeColor);
  drawEye(4, 1, 2, eyeColor);
  drawBatteryIndicator();
}

// ==========================================
//          ВКЛЮЧЕНИЕ
// ==========================================
void playBootSequence() {
  isBusy = true;
  
  for (int i = 0; i < 64; i++) {
    FastLED.clear();
    leds[i] = CRGB(255, 150, 0);
    FastLED.show();
    delay(15);
  }
  delay(200);
  FastLED.clear();
  FastLED.show();
  
  delay(200);
  
  drawModeEyes();
  FastLED.show();
  isBusy = false;
}

// ==========================================
//          ДАТЧИКИ
// ==========================================
void checkSensors() {
  bool front = digitalRead(IR_FRONT);
  bool back = digitalRead(IR_BACK);

  // === ВОЗДУХ ===
  if (front == LOW && back == LOW) {
    if (!wasAir) {
      wasAir = true;
      airSoundPlayed = false;
      airTimer = millis();
      lastActivityTime = millis();  // 🔥 Сбрасываем таймер спокойствия
    }
    
    if (!airSoundPlayed || (millis() - airTimer > 5000)) {
      if (!isPlaying) {
        airSoundPlayed = true;
        airTimer = millis();
        playSound(4);
      }
    }
    
    if (!isBusy && !isMoving && !isButtonMoving) {
      int r = random(1, 5);
      int dur = 200 + random(0, 300);
      startMove(r, dur);
    }
    
    drawAngryFace();
    
    wasFrontAbyss = false;
    wasBackAbyss = false;
    return;
  } else {
    if (wasAir) {
      wasAir = false;
      airSoundPlayed = false;
      airTimer = 0;
      stopMotors();
      drawModeEyes();
    }
  }

  // === ПРОПАСТЬ СПЕРЕДИ ===
  if (front == LOW && back == HIGH) {
    if (!isBusy && !wasFrontAbyss) {      
      isBusy = true;
      wasFrontAbyss = true;
      lastActivityTime = millis();  // 🔥 Сбрасываем таймер спокойствия
      playSound(2);
      
      startMove(2, 800);
      unsigned long startTime = millis();
      while (millis() - startTime < 800) {
        handleMovement(millis());
      }
      stopMotors();
      isBusy = false;
    }
    wasBackAbyss = false;
    return;
  } else {
    wasFrontAbyss = false;
  }

  // === ПРОПАСТЬ СЗАДИ ===
  if (front == HIGH && back == LOW) {
    if (!isBusy && !wasBackAbyss) {
      isBusy = true;
      wasBackAbyss = true;
      lastActivityTime = millis();  // 🔥 Сбрасываем таймер спокойствия
      playSound(3);
      
      startMove(1, 800);
      unsigned long startTime = millis();
      while (millis() - startTime < 800) {
        handleMovement(millis());
        delay(10);
      }
      stopMotors();
      isBusy = false;
    }
    wasFrontAbyss = false;
    return;
  } else {
    wasBackAbyss = false;
  }

  if (front == HIGH && back == HIGH) {
    wasFrontAbyss = false;
    wasBackAbyss = false;
  }
}

// ==========================================
//          КНОПКИ (БЕЗ DELAY!)
// ==========================================
void handleButtons(unsigned long current) {
  bool btn1 = digitalRead(BTN_1);
  bool btn2 = digitalRead(BTN_2);
  bool btn3 = digitalRead(BTN_3);
  bool btn4 = digitalRead(BTN_4);
  
  bool press1 = (btn1 == HIGH && lastBtn1 == LOW);
  bool press2 = (btn2 == HIGH && lastBtn2 == LOW);
  bool press3 = (btn3 == HIGH && lastBtn3 == LOW);
  bool press4 = (btn4 == HIGH && lastBtn4 == LOW);
  
  lastBtn1 = btn1;
  lastBtn2 = btn2;
  lastBtn3 = btn3;
  lastBtn4 = btn4;
  
  if (current - lastBtnTime < debounceDelay) return;
  if (isPlaying) return;
  if (isButtonMoving) return;  // Не даем нажимать пока движется
  
  // === КНОПКА 1 - СМЕНА РЕЖИМА ===
  if (press1) {
    lastBtnTime = current;
    isBusy = true;
    lastActivityTime = millis();  // 🔥 Сбрасываем таймер спокойствия
    
    if (currentMode == MODE_GOOD) {
      currentMode = MODE_EVIL;
      playSound(5);
      drawAngryFace();
    } else {
      currentMode = MODE_GOOD;
      playSound(6);
      drawHappyFace();
    }
    
    FastLED.show();
    
    // 🔥 Вместо delay используем таймер
    unsigned long startWait = millis();
    while (millis() - startWait < 400) {
      checkDFPlayerStatus();  // Продолжаем слушать DFPlayer
      delay(10);
    }
    
    drawModeEyes();
    FastLED.show();
    isBusy = false;
    return;
  }

  // === КНОПКА 2 ===
  if (press2) {
    lastBtnTime = current;
    isBusy = true;
    lastActivityTime = millis();  // 🔥 Сбрасываем таймер спокойствия
    
    int action = random(1, 5);
    int duration = 300 + random(0, 1200);
    
    Serial.print(F("  BTN2: action="));
    Serial.print(action);
    Serial.print(F(" duration="));
    Serial.println(duration);
    
    if (currentMode == MODE_GOOD) {
      playSound(7);
      drawHappyFace();
    } else {
      playSound(10);
      drawAngryFace();
    }
    FastLED.show();
    
    // 🔥 Запускаем неблокирующее движение
    startButtonMove(action, duration);
    
    // 🔥 Ждем окончания движения через обработчик в loop
    // Вместо delay просто устанавливаем флаг и выходим
    isBusy = false;
    return;
  }

  // === КНОПКА 3 ===
  if (press3) {
    lastBtnTime = current;
    isBusy = true;
    lastActivityTime = millis();  // 🔥 Сбрасываем таймер спокойствия
    
    int action = random(1, 5);
    int duration = 300 + random(0, 1200);
    
    if (currentMode == MODE_GOOD) {
      playSound(8);
      drawSadFace();
    } else {
      playSound(11);
      drawEvilLaugh();
    }
    FastLED.show();
    
    startButtonMove(action, duration);
    isBusy = false;
    return;
  }

  // === КНОПКА 4 ===
  if (press4) {
    lastBtnTime = current;
    isBusy = true;
    lastActivityTime = millis();  // 🔥 Сбрасываем таймер спокойствия
    
    int action = random(1, 5);
    int duration = 300 + random(0, 1200);
    
    Serial.print(F("  BTN4: action="));
    Serial.print(action);
    Serial.print(F(" duration="));
    Serial.println(duration);
    
    if (currentMode == MODE_GOOD) {
      playSound(9);
      drawSurprised();
    } else {
      playSound(12);
      drawScary();
    }
    FastLED.show();
    
    startButtonMove(action, duration);
    isBusy = false;
    return;
  }
}

// ==========================================
//          СЛУЧАЙНЫЕ ФРАЗЫ
// ==========================================
void playRandomPhrase() {
  if (isPlaying) return;
  
  if (currentMode == MODE_GOOD) {
    int phrase = random(1, 6);
    switch (phrase) {
      case 1: playSound(13); break;
      case 2: playSound(14); break;
      case 3: playSound(15); break;
      case 4: playSound(19); break;
      case 5: playSound(20); break;
    }
  } else {
    int phrase = random(1, 6);
    switch (phrase) {
      case 1: playSound(16); break;
      case 2: playSound(17); break;
      case 3: playSound(18); break;
      case 4: playSound(21); break;
      case 5: playSound(22); break;
    }
  }
}
