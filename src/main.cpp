#include <Arduino.h>
#include <M5EPD.h>
#include "esp_camera.h"

#define screenWidth 960
#define screenHeight 540
#define photoWidth 800
#define photoHeight 540
#define buttonWidth 160
#define buttonHeight 135

void drawButtons(boolean update);
void pushButton(int button);
void capturePhoto();
void receivePhoto();
String nextFileName();
void prevPhoto();
void nextPhoto();
void displayMessage(String message);
void shutdown();

boolean setupCompleted = false;
int fileIndex = 0;
int fileCount = 0;
size_t jpegSize = 0;

M5EPD_Canvas photoCanvas(&M5.EPD);
M5EPD_Canvas buttonCanvas(&M5.EPD);

tp_finger_t lastFingerItem;

void setup()
{
  M5.begin();
  M5.EPD.SetRotation(0);
  M5.EPD.Clear(true);
  photoCanvas.createCanvas(photoWidth, photoHeight);
  buttonCanvas.createCanvas(buttonWidth, buttonHeight);

  Serial1.begin(115200, SERIAL_8N1, G19, G18); // RX: 19, TX: 18
  Serial.println(nextFileName());

  drawButtons(true);
}

void loop()
{

  // Serial port listening
  if (Serial1.available() > 0)
  {
    String line = Serial1.readStringUntil('\n');
    Serial.println(line);

    if (line.startsWith("JPEG_SIZE:"))
    {
      String sizeString = line.substring(strlen("JPEG_SIZE:"));
      jpegSize = sizeString.toInt();
      Serial.printf("jpeg size:%d\n", jpegSize);
    }
    else if (line.startsWith("JPEG_START:"))
    {
      receivePhoto();
    }
  }

  // Button detection
  M5.update();
  if (M5.BtnL.wasPressed())
  {
    prevPhoto();
    return;
  }
  else if (M5.BtnR.wasPressed())
  {
    nextPhoto();
    return;
  }
  else if (M5.BtnP.wasPressed())
  {
    capturePhoto();
    return;
  }

  // Touch detection
  if (M5.TP.avaliable())
  {
    if (!M5.TP.isFingerUp())
    {
      M5.TP.update();
      tp_finger_t fingerItem = M5.TP.readFinger(0);
      if (lastFingerItem.x == fingerItem.x && lastFingerItem.y == fingerItem.y)
        return;
      lastFingerItem = fingerItem;
      if (fingerItem.x == 0 || fingerItem.y == 0)
        return;

      if (fingerItem.x > photoWidth)
      {
        pushButton(fingerItem.y / 135);
      }
    }
  }
}

// Draw buttons
void drawButtons(boolean update)
{
  for (int i = 0; i < 4; i++)
  {
    buttonCanvas.fillCanvas(BLACK);
    buttonCanvas.drawFastVLine(0, 0, buttonHeight, WHITE);
    // buttonCanvas.drawFastHLine(0, buttonHeight-1, buttonWidth, WHITE);
    switch (i)
    {
    case 0:
    {
      buttonCanvas.fillTriangle(80, 30, 80 + 37, 30 + 74, 80 - 37, 30 + 74, (fileIndex > 0 && fileCount != 0) ? WHITE : BLACK);
      break;
    }
    case 1:
    {
      buttonCanvas.fillTriangle(80, 30 + 74, 80 + 37, 30, 80 - 37, 30, (fileIndex < fileCount && fileCount != 0) ? WHITE : BLACK);
      break;
    }
    case 2:
    {
      buttonCanvas.fillCircle(buttonWidth / 2, buttonHeight / 2, 40, WHITE);
      break;
    }
    case 3:
    {
      buttonCanvas.fillRect(45, 33, 70, 70, WHITE);
      break;
    }
    default:
      break;
    }
    buttonCanvas.pushCanvas(photoWidth, 134 * i, UPDATE_MODE_NONE);
  }
  if (update)
    M5.EPD.UpdateArea(photoWidth, 0, buttonWidth, screenHeight, UPDATE_MODE_DU);
}

// Invert button and call function
void pushButton(int button)
{
  buttonCanvas.fillCanvas(WHITE);
  switch (button)
  {
  case 0:
  {
    if (fileIndex > 0 && fileCount != 0)
      buttonCanvas.pushCanvas(photoWidth, 134 * button, UPDATE_MODE_DU);
    prevPhoto();
    break;
  }
  case 1:
  {
    if (fileIndex < fileCount && fileCount != 0)
      buttonCanvas.pushCanvas(photoWidth, 134 * button, UPDATE_MODE_DU);
    nextPhoto();
    break;
  }
  case 2:
  {
    buttonCanvas.pushCanvas(photoWidth, 134 * button, UPDATE_MODE_DU);
    capturePhoto();
    break;
  }
  case 3:
  {
    buttonCanvas.pushCanvas(photoWidth, 134 * button, UPDATE_MODE_DU);
    shutdown();
    break;
  }
  default:
    break;
  }
}

// Send capture command to start capture photo
void capturePhoto()
{
  if (!setupCompleted)
  {
    // setup capture size and direction
    Serial.printf("SETUP_SIZE:%d\n", FRAMESIZE_SVGA);
    Serial1.printf("SETUP_SIZE:%d\n", FRAMESIZE_SVGA);

    Serial.println("SETUP_VFLIP:FALSE");
    Serial1.println("SETUP_VFLIP:FALSE");

    Serial.println("SETUP_HMIRROR:FALSE");
    Serial1.println("SETUP_HMIRROR:FALSE");

    Serial.println("SETUP_MAXBATCH:10000");
    Serial1.println("SETUP_MAXBATCH:10000");
    setupCompleted = true;
    delay(10);
  }
  Serial.println("CAPTURE:");
  Serial1.println("CAPTURE:");
  return;
}

// Receive, display and save photo
void receivePhoto()
{
  size_t receivedSize = 0;
  byte *buffer = (byte *)malloc(jpegSize * sizeof(byte));
  if (buffer == NULL)
  {
    Serial.println("malloc failed");
    return;
  }
  byte marker1 = 0;
  byte marker2 = 0;
  boolean repeat = true;
  int timeoutCount = 0;
  while (receivedSize < jpegSize)
  {
    if (Serial1.available() > 0)
    {
      size_t readLength = Serial1.readBytes(buffer + receivedSize, jpegSize - receivedSize);
      receivedSize += readLength;
      if (receivedSize == jpegSize)
        break;
      timeoutCount = 0;
    }
    else
    {
      timeoutCount++;
      if (timeoutCount > 100)
      {
        Serial.println("Timeout");
        photoCanvas.drawString("Timeout", 64, 64);
        photoCanvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
        free(buffer);
        return;
      }
      delay(1);
    }
  }
  Serial.printf("jpeg received:%d / %d\n", receivedSize, jpegSize);
  photoCanvas.drawJpg(buffer, jpegSize, 0, 0, photoWidth, photoHeight, 0, 30, JPEG_DIV_NONE);
  photoCanvas.pushCanvas(0, 0, UPDATE_MODE_NONE);
  drawButtons(false);
  M5.EPD.UpdateFull(UPDATE_MODE_GC16);

  Serial.println("create .jpg file");
  String fileName = nextFileName();
  fileIndex = fileCount;
  Serial.println(fileName);
  File jpgFile = SD.open(fileName, FILE_WRITE);
  if (jpgFile)
  {
    jpgFile.write(buffer, jpegSize);
    jpgFile.close();

    Serial.println("wrote .jpg file");
  }
  else
  {
    Serial.println("No SD card");
  }
  free(buffer);
  // buffer = NULL;
}

String nextFileName()
{
  String fileName = "/capture" + String(fileCount) + ".jpg";
  while (SD.exists(fileName))
  {
    fileCount++;
    fileName = "/capture" + String(fileCount) + ".jpg";
  }
  return fileName;
}

void prevPhoto()
{

  if (fileIndex == 0 && fileCount == 0)
    return;
  fileIndex--;
  if (fileIndex < 0)
  {
    fileIndex = 0;
    return;
  }
  String fileName = "/capture" + String(fileIndex) + ".jpg";
  photoCanvas.drawJpgFile(SD, fileName.c_str(), 0, 0, photoWidth, photoHeight, 0, 30, JPEG_DIV_NONE);
  photoCanvas.pushCanvas(0, 0, UPDATE_MODE_NONE);
  drawButtons(false);
  M5.EPD.UpdateFull(UPDATE_MODE_GC16);
}

void nextPhoto()
{
  if (fileIndex == 0 && fileCount == 0)
    return;
  fileIndex++;
  if (fileIndex > fileCount)
  {
    fileIndex = fileCount;
    return;
  }
  String fileName = "/capture" + String(fileIndex) + ".jpg";
  photoCanvas.drawJpgFile(SD, fileName.c_str(), 0, 0, photoWidth, photoHeight, 0, 30, JPEG_DIV_NONE);
  photoCanvas.pushCanvas(0, 0, UPDATE_MODE_NONE);
  drawButtons(false);
  M5.EPD.UpdateFull(UPDATE_MODE_GC16);
}

void displayMessage(String message)
{
  Serial.println(message);
  photoCanvas.fillCanvas(BLACK);
  photoCanvas.drawString(message, 64, 64);
  photoCanvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
}

void shutdown()
{
  delay(500);
  M5.shutdown();
}