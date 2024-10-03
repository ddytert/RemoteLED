#include <SPI.h>
#include "RF24.h"

const unsigned int MAX_PAYLOAD_SIZE = 32;
const unsigned int MAX_NUMBER_PAYLOADS = 32;
const int MAX_COLORS = 10;

// put function declarations here:
void parseMessage(String);
void initReceptionOfMPMG(String);
void assembleMPMG(String);
void loadLEDColorsFromString(String);
void changeLEDColor();
void switchRelais(String);
int hex2int(char, char);

// Variables
const int redPin = 1;   // pin for red LED
const int bluePin = 2;  // pin for blue LED
const int greenpin = 3; // pin for green LED

const int relaisPin = 21;

RF24 radio(9, 10);               // (CE, CSN)
const byte address[6] = "1RF24"; // address / identifier

String multiPayloadMessage = "";
unsigned int numberOfPayloads = 0;
bool isReceivingMPMG = false;
unsigned int payloadIndex = 0;

int numberOfColors;
int colors[MAX_COLORS][3]; // Array of rbg arrays
int currentColorIndex = 0;

unsigned long shineDuration = 1000;
unsigned long transitionDuration = 1000;
bool isTransitioning = false;

void setup()
{
  // Set LED pins
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(greenpin, OUTPUT);

  // Set relais pin
  pinMode(relaisPin, OUTPUT);

  Serial.begin(115200);
  Serial.println("Connecting to nRF24L01...");
  delay(3000);

  if (!radio.begin())
  {
    Serial.println("nRF24L01 module not connected!");
    while (1)
    {
    }
  }
  else
    Serial.println("nRF24L01 module connected!");

  // Set start colors
  String initString = String("02 03 03 000000 000010");
  initString.replace(" ", "");
  loadLEDColorsFromString(initString);

  /* Set the data rate:
   * RF24_250KBPS: 250 kbit per second
   * RF24_1MBPS:   1 megabit per second
   * RF24_2MBPS:   2 megabit per second
   */
  radio.setDataRate(RF24_250KBPS);

  /* Set the power amplifier level rate:
   * RF24_PA_MIN:   -18 dBm
   * RF24_PA_LOW:   -12 dBm
   * RF24_PA_HIGH:   -6 dBm
   * RF24_PA_MAX:     0 dBm (default)
   */
  radio.setPALevel(RF24_PA_MAX); // sufficient for tests side by side

  /* Set the channel x with x = 0...125 => 2400 MHz + x MHz
   * Default: 76 => Frequency = 2476 MHz
   * use getChannel to query the channel
   */
  radio.setChannel(120);

  radio.openReadingPipe(0, address); // set the address
  radio.startListening();            // set as receiver

  /* The default payload size is 32. You can set a fixed payload size which
   * must be the same on both the transmitter (TX) and receiver (RX)side.
   * Alternatively, you can use dynamic payloads, which need to be enabled
   * on RX and TX.
   */
  // radio.setPayloadSize(11);
  radio.enableDynamicPayloads();
}

void loop()
{
  if (radio.available())
  {
    byte len = radio.getDynamicPayloadSize();
    char text[len + 1] = {0};
    radio.read(&text, len);
    if (isReceivingMPMG)
    {
      assembleMPMG(text);
    }
    else
    {
      parseMessage(text);
    }
  }
  changeLEDColor();
}

void parseMessage(String message)
{
  String command = message.substring(0, 4);
  String data = message.substring(4, message.length());

  Serial.print("\nIncoming message: ");
  Serial.println(message);
  Serial.print("Command: ");
  Serial.println(command);
  Serial.print("Data: ");
  Serial.println(data);

  // Example string: "CCOL011122FF"
  if (command == "CCOL") // ChangeCOLor
  {
    loadLEDColorsFromString(data);
  }
  else if (command == "MPMG") // MultiPayloadMessaGe
  {
    initReceptionOfMPMG(data);
  }
  else if (command == "SWRE") // SWitch Relais
  {
    switchRelais(data);
  }
}

void initReceptionOfMPMG(String numberData)
{
  const unsigned int numPayloads = numberData.toInt();
  if (numPayloads <= 0 || numPayloads > MAX_NUMBER_PAYLOADS)
  {
    Serial.print("Message too big - number of payloads = ");
    Serial.println(numPayloads);
    return;
  }
  isReceivingMPMG = true;
  numberOfPayloads = numPayloads;
  multiPayloadMessage = "";
  payloadIndex = 0;
  Serial.print("Start receiving payloads: ");
  Serial.println(numPayloads);
}

void assembleMPMG(String message)
{
  multiPayloadMessage += message;
  // Set index to next message
  payloadIndex++;
  // If it was the last message reset all variables and parse the message
  if (payloadIndex >= numberOfPayloads)
  {
    Serial.print("Assembled message: ");
    Serial.println(multiPayloadMessage);
    payloadIndex = 0;
    numberOfPayloads = 0;
    isReceivingMPMG = false;
    parseMessage(multiPayloadMessage);
  }
}

// void loadLEDColorsFromString(const char *colorData)

void loadLEDColorsFromString(String colorsData)
{
  Serial.print("Loading LED colors: ");
  Serial.println(colorsData);
  unsigned int const newNumberOfColors = colorsData.substring(0, 2).toInt();
  if (newNumberOfColors == 0 || newNumberOfColors > MAX_COLORS)
  {
    Serial.print("Error: nuber of colors: ");
    Serial.println(numberOfColors);
    return;
  }
  numberOfColors = newNumberOfColors;

  unsigned int const newShineDuration = colorsData.substring(2, 4).toInt();
  if (newShineDuration == 0 || newShineDuration > 99)
  {
    Serial.print("Error: Shine duration not correct");
    Serial.println(newShineDuration);
    return;
  }
  shineDuration = newShineDuration * 100;

  unsigned int const newTransitionDuration = colorsData.substring(4, 6).toInt();
  if (newTransitionDuration < 0 || newTransitionDuration > 99)
  {
    Serial.print("Error: Transition duration not correct: ");
    Serial.println(newTransitionDuration);
    return;
  }
  transitionDuration = newTransitionDuration * 100;

  Serial.printf("Start parsing %d colors\n", numberOfColors);

  for (int i = 0; i < numberOfColors; i++)
  {
    int redDec, blueDec, greenDec;

    int offset = i * 6 + 6;
    // Get one color from color info string
    String colorData = colorsData.substring(offset, offset + 6);
    Serial.printf("\nColor #%d = %s\n", i + 1, colorData);

    redDec = hex2int(colorData[0], colorData[1]);
    blueDec = hex2int(colorData[2], colorData[3]);
    greenDec = hex2int(colorData[4], colorData[5]);

    if (redDec < 0 || redDec > 255 || blueDec < 0 || blueDec > 255 || greenDec < 0 || greenDec > 255)
    {
      Serial.println("Error parsing colors!");
      return;
    }

    colors[i][0] = redDec;
    colors[i][1] = blueDec;
    colors[i][2] = greenDec;

    Serial.printf("\nRed: %d", redDec);
    Serial.printf("\nBlue: %d", blueDec);
    Serial.printf("\nGreen: %d", greenDec);

    currentColorIndex = 0;
  }
}

void changeLEDColor()
{
  static unsigned long lastUpdate, lastUpdateShort = millis();

  unsigned long timeElapsed = millis() - lastUpdate;
  unsigned long timeElapsedShort = millis() - lastUpdateShort;

  if (!isTransitioning)
  {
    if (timeElapsed > shineDuration)
    {
      isTransitioning = true;
      lastUpdate = millis();
    }
  }
  else
  {
    // Mix colors during transition
    if (timeElapsed <= transitionDuration)
    {
      // Limit writing to analog pins to every 5 milliseconds
      if (timeElapsedShort < 5)
      {
        return;
      }
      lastUpdateShort = millis();

      int newColorIndex = currentColorIndex == numberOfColors - 1 ? 0 : currentColorIndex + 1;
      int *oldColor = colors[currentColorIndex];
      int *newColor = colors[newColorIndex];
      // Get transition value
      float tValue = float(timeElapsed) / float(transitionDuration);
      // Mix colors
      int mixedRed = int(float(oldColor[0]) * (1.0 - tValue) + float(newColor[0]) * tValue);
      int mixedBlue = int(float(oldColor[1]) * (1.0 - tValue) + float(newColor[1]) * tValue);
      int mixedGreen = int(float(oldColor[2]) * (1.0 - tValue) + float(newColor[2]) * tValue);
      // Write color to pins
      analogWrite(redPin, mixedRed);
      analogWrite(bluePin, mixedBlue);
      analogWrite(greenpin, mixedGreen);
    }
    else
    {
      isTransitioning = false;

      lastUpdate = millis();
      currentColorIndex++;
      if (currentColorIndex >= numberOfColors)
      {
        currentColorIndex = 0;
      }
      analogWrite(redPin, colors[currentColorIndex][0]);
      analogWrite(bluePin, colors[currentColorIndex][1]);
      analogWrite(greenpin, colors[currentColorIndex][2]);
    }
  }
}

void switchRelais(String data)
{
  if (data == "ON")
  {
    digitalWrite(relaisPin, HIGH);
  }
  else if (data == "OFF")
  {
    digitalWrite(relaisPin, LOW);
  }
}

// makes a number from two ascii hex characters
int hex2int(char a, char b)
{
  a = (a <= '9') ? a - '0' : (a & 0x7) + 9;
  b = (b <= '9') ? b - '0' : (b & 0x7) + 9;
  return (a << 4) + b;
}