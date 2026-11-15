#include <WiFi.h>
#include <Wire.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const int PIN_SERVO   = 18;
const int PIN_DOOR    = 4;    
const int PIN_TRIG    = 5;
const int PIN_ECHO    = 19;
const int PIN_LED     = 15;

const int SERVO_LOCKED   = 0;
const int SERVO_UNLOCKED = 90;

const int  CODE_LENGTH          = 6;
const unsigned long CODE_TTL_MS = 5UL * 60UL * 1000UL;  // kodas galioja 5 min.
const unsigned long OPEN_WAIT_MS = 30UL * 1000UL;       // per kiek laiko reikia atidaryti dureles
const float BOX_EMPTY_CM  = 40.0;   // atstumas iki dugno tuščioje dėžutėje
const float PARCEL_MIN_CM = 5.0;    // mažiausias siuntos aukštis, kad laikytume „Pilna“

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo lockServo;

// ---------------- Būsenos ----------------
enum State { LOCKED_IDLE, UNLOCKED_WAIT_OPEN, DOOR_OPEN, CHECKING };
State state = LOCKED_IDLE;

enum BoxContent { BOX_UNKNOWN, BOX_EMPTY, BOX_FULL };
BoxContent boxContent = BOX_UNKNOWN;

// Vienkartinis kodas (tikroje sistemoje saugomas serveryje)
String activeCode = "";
unsigned long codeCreatedAt = 0;
bool codeUsed = true;

String inputBuffer = "";
unsigned long stateSince = 0;

// ---------------- Pagalbinės funkcijos ----------------
void showLcd(const String& line1, const String& line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
}

void setState(State s) {
  state = s;
  stateSince = millis();
}

void lockBox() {
  lockServo.write(SERVO_LOCKED);
  digitalWrite(PIN_LED, LOW);
  Serial.println("[UZRAKTAS] Uzrakinta");
}

void unlockBox() {
  lockServo.write(SERVO_UNLOCKED);
  digitalWrite(PIN_LED, HIGH);
  Serial.println("[UZRAKTAS] Atrakinta");
}

bool isDoorClosed() {
  return digitalRead(PIN_DOOR) == LOW;
}

float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long duration = pulseIn(PIN_ECHO, HIGH, 30000UL);
  if (duration == 0) return -1;          // nėra atsako
  return duration * 0.0343 / 2.0;
}

const char* contentName(BoxContent c) {
  switch (c) {
    case BOX_EMPTY: return "Tuscia";
    case BOX_FULL:  return "Pilna";
    default:        return "Nezinoma";
  }
}

// ---------------- Serverio / programėlės imitacija ----------------
// Savininkas programėlėje paprašo kodo -> serveris sugeneruoja vienkartinį PIN.
void generateOneTimeCode() {
  activeCode = "";
  for (int i = 0; i < CODE_LENGTH; i++) activeCode += String(random(0, 10));
  codeCreatedAt = millis();
  codeUsed = false;
  Serial.println("[PROGRAMELE] Sugeneruotas vienkartinis kodas: " + activeCode +
                 " (galioja " + String(CODE_TTL_MS / 60000UL) + " min.)");
}

// Kodo patikra. Tikroje sistemoje M5Stack siųstų užklausą serveriui (HTTP/MQTT).
bool verifyCode(const String& code) {
  if (codeUsed || activeCode.length() == 0) return false;
  if (millis() - codeCreatedAt > CODE_TTL_MS) {
    Serial.println("[SERVERIS] Kodas nebegalioja (baigesi laikas)");
    codeUsed = true;
    return false;
  }
  if (code != activeCode) return false;
  codeUsed = true;            // kodas vienkartinis
  return true;
}

// Būsenos pranešimas savininkui. Tikroje sistemoje – serveris -> SMS / el. paštas / push.
void notifyOwner(BoxContent content) {
  Serial.print("[PRANESIMAS] Savininkui issiusta: dezute ");
  Serial.println(contentName(content));
}

// QR kodo skaitymo vieta (Wokwi neturi QR skaitytuvo).
// Su M5Stack QR Code Unit kodas būtų nuskaitomas per I2C ir perduodamas į verifyCode().
String readQrCode() {
  return "";
}

// ---------------- Wi-Fi ----------------
void connectWiFi() {
  Serial.print("[WIFI] Jungiamasi prie " + String(WIFI_SSID));
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000UL) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " prisijungta" : " nepavyko (veikiama be tinklo)");
}

// ---------------- Įvesties apdorojimas ----------------
void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();
  if (cmd == "KODAS") {
    generateOneTimeCode();
  } else if (cmd == "BUSENA") {
    Serial.println("[BUSENA] Durelės: " + String(isDoorClosed() ? "uzdarytos" : "atidarytos") +
                   ", turinys: " + contentName(boxContent));
  } else if (cmd.length() > 0) {
    Serial.println("Komandos: KODAS, BUSENA");
  }
}

void submitCode(const String& code) {
  if (verifyCode(code)) {
    Serial.println("[DEZUTE] Kodas teisingas");
    unlockBox();
    showLcd("Kodas teisingas", "Atidarykite");
    setState(UNLOCKED_WAIT_OPEN);
  } else {
    Serial.println("[DEZUTE] Neteisingas arba panaudotas kodas");
    showLcd("Neteisingas", "kodas");
    delay(1500);
    showLcd("Iveskite koda:", "");
  }
}

void handleKeypad() {
  char key = keypad.getKey();
  if (!key) return;
  if (key >= '0' && key <= '9') {
    if (inputBuffer.length() < CODE_LENGTH) inputBuffer += key;
    showLcd("Iveskite koda:", inputBuffer);
  } else if (key == '*') {
    inputBuffer = "";
    showLcd("Iveskite koda:", "");
  } else if (key == '#') {
    String code = inputBuffer;
    inputBuffer = "";
    submitCode(code);
  }
}

// ---------------- Pagrindinė logika ----------------
void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  pinMode(PIN_DOOR, INPUT_PULLUP);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED, OUTPUT);

  lcd.init();
  lcd.backlight();
  lockServo.attach(PIN_SERVO);
  lockBox();

  showLcd("Siuntu dezute", "Jungiamasi...");
  connectWiFi();

  Serial.println("Komandos: KODAS – sugeneruoti koda, BUSENA – busena");
  showLcd("Iveskite koda:", "");
  setState(LOCKED_IDLE);
}

void loop() {
  handleSerial();

  switch (state) {
    case LOCKED_IDLE: {
      handleKeypad();
      String qr = readQrCode();
      if (qr.length() > 0) submitCode(qr);
      break;
    }

    case UNLOCKED_WAIT_OPEN:
      if (!isDoorClosed()) {
        Serial.println("[DURELES] Atidarytos");
        showLcd("Idekite siunta", "ir uzdarykite");
        setState(DOOR_OPEN);
      } else if (millis() - stateSince > OPEN_WAIT_MS) {
        Serial.println("[DEZUTE] Durelės neatidarytos laiku – uzrakinama");
        lockBox();
        showLcd("Iveskite koda:", "");
        setState(LOCKED_IDLE);
      }
      break;

    case DOOR_OPEN:
      if (isDoorClosed()) {
        Serial.println("[DURELES] Uzdarytos");
        delay(300);                 // durelių nusistovėjimas
        lockBox();
        setState(CHECKING);
      }
      break;

    case CHECKING: {
      float d = readDistanceCm();
      Serial.println("[JUTIKLIS] Atstumas iki dugno: " + String(d, 1) + " cm");
      if (d < 0) boxContent = BOX_UNKNOWN;
      else boxContent = (d <= BOX_EMPTY_CM - PARCEL_MIN_CM) ? BOX_FULL : BOX_EMPTY;
      notifyOwner(boxContent);
      showLcd("Dezute:", contentName(boxContent));
      delay(2000);
      showLcd("Iveskite koda:", "");
      setState(LOCKED_IDLE);
      break;
    }
  }

  delay(10);
}
