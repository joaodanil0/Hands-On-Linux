
#define LED_PIN 2
#define LDR_PIN 4
// #define LED2_PIN 15
#define LDR_MAX_VALUE 3500

// #define DEBUG_UART_TX 17
// #define DEBUG_UART_RX 16

int ledValue = 10;
int ldrValue = 23;
bool led2state = false;

void setup() {
  Serial.begin(9600);
  // Serial2.begin(9600, SERIAL_8N1, DEBUG_UART_RX, DEBUG_UART_TX);

  pinMode(LED_PIN, OUTPUT);
  // pinMode(LED2_PIN, OUTPUT);

  ledUpdate(ledValue);
  // delay(1000);
  // processCommand("GET_LDR");
}

// Função loop será executada infinitamente pelo ESP32
void loop() {
  //Obtenha os comandos enviados pela serial
  //e processe-os com a função processCommand

  if (Serial.available() > 0) {
    led2state = !led2state;
    // digitalWrite(LED2_PIN, led2state);
    String aux = Serial.readString();
    // Serial2.println("-------->" + aux);
    processCommand(aux);
  }

  // Serial.println(analogRead(LDR_PIN));
  // delay(100);
}

int map(int x, int inMin, int inMax, int outMin, int outMax) {
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

String extractCommand(String command) {
  return command.substring(0, 7);
}

String extractValue(String command) {
  return command.substring(8);
}

int checkRange(int value) {
  return value >= 0 && value <= 100 ? value : -1;
}

void processCommand(String command) {
  // compare o comando com os comandos possíveis e execute a ação correspondente
  // Serial.println(String(command));
  if (command == "GET_LED") {
    // delay(500);
    Serial.println("RES GET_LED " + String(ledValue));
    // Serial2.println("RES GET_LED " + String(ledValue));
  } 
  else if (command == "GET_LDR") {
    // delay(500);
    int aux = ldrGetValue();
    Serial.println("RES GET_LDR " + String(aux));
    // Serial2.println("RES GET_LDR " + String(aux));
  } 
  else if (extractCommand(command) == "SET_LED") {
    int value = extractValue(command).toInt();
    value = checkRange(value);

    if (value == -1) {
      Serial.println("RES SET_LED -1");
      // Serial2.println("RES SET_LED -1");
    } 
    else {
      ledUpdate(value);
      Serial.println("RES SET_LED 1");
      // Serial2.println("RES SET_LED 1");
    }
  } 
  else {
    Serial.println("ERR Unknown command");
    // Serial2.print("ERR Unknown command");
  }
}

// Função para atualizar o valor do LED
void ledUpdate(int value) {
  // Valor deve convertar o valor recebido pelo comando SET_LED para 0 e 255
  // Normalize o valor do LED antes de enviar para a porta correspondente
  ledValue = value;
  int normalizedValue = map(ledValue, 0, 100, 0, 255);
  analogWrite(LED_PIN, normalizedValue);
}

// Função para ler o valor do LDR
int ldrGetValue() {
  // Leia o sensor LDR e retorne o valor normalizado entre 0 e 100
  // faça testes para encontrar o valor maximo do ldr (exemplo: aponte a lanterna do celular para o sensor)
  // Atribua o valor para a variável ldrMax e utilize esse valor para a normalização
  ldrValue = analogRead(LDR_PIN);
  return map(ldrValue, 0, LDR_MAX_VALUE, 0, 100);
}