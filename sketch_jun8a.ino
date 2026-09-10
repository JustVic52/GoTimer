#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <unistd.h>
#include <ctime>
#include <cmath>

#include "ThreeScrambler.h"
#include "TwoScrambler.h"
#include "SkewbScrambler.h"
#include "PyraScrambler.h"
#include "FourScrambler.h"
#include "FiveScrambler.h"
#include "SixSevenScrambler.h"
#include "SqoneScrambler.h"
#include "FtoScrambler.h"
#include "MegaScrambler.h"
#include "ClockScrambler.h"
#include "Iconos.h"

// Pines de hardware
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5
#define TFT_BL_PIN 27
#define SENSOR_PIN 35

#define UMBRAL_PRESION 500
#define DEBOUNCE_MS 300

#define BUFFER_RECORDS 64

// Coordenadas fijas
#define BUTTON_W 50
#define BUTTON_H 25
#define BUTTON_CUBE_W 80
#define FLECHA_X 280
#define FLECHA_Y 218
#define FLECHA_WH 30

TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(HSPI);

enum EstadoTimer { DETENIDO, ESPERANDO, PREPARADO, CORRIENDO };
EstadoTimer estado = DETENIDO;

typedef std::string (*ScrambleFunc)();

struct CubeConfig {
  const char* label;
  const char* file;
  const uint16_t* icon;
  int page;
  int x;
  int y;
  ScrambleFunc scrambler;
};

struct SolveRecord {
  int32_t tiempo;
  int32_t ao5;
  int32_t ao12;
  uint8_t archivado;
  uint32_t offsetMezcla;
  uint32_t longMezcla;
};

struct SessionStats {
  uint32_t count = 0;
  uint32_t dnfs = 0;
  int32_t  bestSingle = -1;
  int32_t  worstSingle = -1;
  int32_t  bestAo5 = -1;
  int32_t  bestAo12 = -1;
  int32_t  currentAo5 = -1;
  int32_t  currentAo12 = -1;
  int32_t  media = -1;
  float    desviacion = 0.0f;
};

// Generadores específicos con parámetros adaptados a la firma ScrambleFunc
std::string getScramble6x6() { return SixSevenScrambler::scramble(80); }
std::string getScramble7x7() { return SixSevenScrambler::scramble(100); }

const CubeConfig CUBOS[] = {
  // Página 1
  {" 2x2 ", "2x2", twobuttonD, 1, 10,  38, TwoScrambler::scramble},
  {" 3x3 ", "3x3", threebuttonD, 1, 80,  38, ThreeScrambler::scramble},
  {" 4x4 ", "4x4", fourbuttonD, 1, 150, 38, FourScrambler::scramble},
  {" 5x5 ", "5x5", fivebuttonD, 1, 220, 38, FiveScrambler::scramble},
  {" 6x6 ", "6x6", sixbuttonD, 1, 10,  108, getScramble6x6},
  {" 7x7 ", "7x7", sevenbuttonD, 1, 80,  108, getScramble7x7},
  {"blind", "blind", blindbuttonD, 1, 150, 108, ThreeScrambler::scramble},
  {" sq1 ", "sq1", sqonebuttonD, 1, 220, 108, SqoneScrambler::scramble},
  {"pyram", "pyram", pyrabuttonD, 1, 10,  178, PyraScrambler::scramble},
  {"clock", "clock", clockbuttonD, 1, 80,  178, ClockScrambler::scramble},
  {"megam", "megam", megabuttonD, 1, 150, 178, MegaScrambler::scramble},
  {"skewb", "skewb", skewbbuttonD, 1, 220, 178, SkewbScrambler::scramble},
  // Página 2
  {" 3oh ", "3oh", ohbuttonD, 2, 10,  38, ThreeScrambler::scramble},
  {"4blnd", "4blnd", fourbldbuttonD, 2, 80,  38, FourScrambler::scramble},
  {"5blnd", "5blnd", fivebldbuttonD, 2, 150, 38, FiveScrambler::scramble},
  {" fto ", "fto", ftobuttonD, 2, 220, 38, FtoScrambler::scramble}
};
const size_t TOTAL_CUBOS = sizeof(CUBOS) / sizeof(CUBOS[0]);

int cuboActual = 1; // Índice por defecto (3x3)
unsigned long tiempoMano = 0;
unsigned long tiempoInicio = 0;
unsigned long tiempoTranscurrido = 0;
unsigned long ultimoToque = 0;
unsigned long debounceFinTimer = 0;

int pantallaActual = 1;
String mezcla = "";
String ultimaMezcla = "";
int paginaScramble = 0;
int paginaCubos = 1;
bool sdDisponible = false;
bool esDnf = false;
bool averagesShown = false;
bool mezclaShown = false;

// Prototipos
void drawBasic();
void drawTimer();
void drawTimes();
void drawStats();
void drawCubes();
void drawAverages();
void drawMezcla();
void mostrarTiempo(long ms);
void imprimirAlgoritmo(const String& algoritmo);
String generarMezcla();
String getCubePath();
void actualizarRegistro(long ms);
bool addLinea(const char* path, const String& linea);
bool eliminarUltimaLinea(const char* path);

inline bool puntoEnArea(int px, int py, int x, int y, int w, int h) {
  return (px >= x && px <= (x + w) && py >= y && py <= (y + h));
}

void setup() {
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  //tft.setRotation(1);
  //uint16_t calData[5] = { 229, 3433, 369, 3383, 1 };
  tft.setRotation(3);
  uint16_t calData[5] = { 210, 3456, 371, 3387, 7 };
  tft.setTouch(calData);
  tft.setSwapBytes(true);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdDisponible = SD.begin(SD_CS, sdSPI);

  analogReadResolution(12);
  tft.fillScreen(TFT_BLACK);
  std::srand(std::time(nullptr));

  drawBasic();
  drawTimer();
  mezcla = generarMezcla();
  imprimirAlgoritmo(mezcla);
  mostrarTiempo(0);
}

void loop() {
  uint16_t touchX = 0, touchY = 0;
  bool tocadoPantalla = tft.getTouch(&touchX, &touchY);

  if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {
    // Barra de pestañas superior
    if (puntoEnArea(touchX, touchY, 0, 0, BUTTON_CUBE_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 0) { pantallaActual = 0; drawCubes(); }
    } 
    else if (puntoEnArea(touchX, touchY, 80, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 1) {
        pantallaActual = 1;
        drawTimer();
        if (mezcla.length() == 0) mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);
        mostrarTiempo(0);
      }
    } 
    else if (puntoEnArea(touchX, touchY, 130, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 2) { pantallaActual = 2; drawTimes(); }
    } 
    else if (puntoEnArea(touchX, touchY, 180, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 3) { pantallaActual = 3; drawStats(); }
    }
  }

  switch (pantallaActual) {
    case 0: // Selección de Cubos
      if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {
        if (puntoEnArea(touchX, touchY, FLECHA_X, FLECHA_Y, FLECHA_WH, FLECHA_WH)) {
          ultimoToque = millis();
          paginaCubos = (paginaCubos == 1) ? 2 : 1;
          drawCubes();
          break;
        }

        for (size_t i = 0; i < TOTAL_CUBOS; i++) {
          if (CUBOS[i].page == paginaCubos && puntoEnArea(touchX, touchY, CUBOS[i].x, CUBOS[i].y, bigIconW, bigIconH)) {
            ultimoToque = millis();
            cuboActual = i;
            pantallaActual = 1;
            paginaCubos = 1;
            drawTimer();
            mezcla = generarMezcla();
            imprimirAlgoritmo(mezcla);
            mostrarTiempo(0);
            break;
          }
        }
      }
      break;

    case 1: // Cronómetro
      if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {
        bool esGrande = (cuboActual >= 4 && cuboActual <= 5) || cuboActual == 10; // 6x6, 7x7, megaminx

        // Cambio de página en mezclas largas
        if (esGrande && puntoEnArea(touchX, touchY, 2, 30, 317, 138)) {
          ultimoToque = millis();
          paginaScramble = 1 - paginaScramble;
          if (averagesShown) {
            tft.fillRect(100, 75, 219, 90, TFT_BLACK);
            tft.drawFastVLine(245, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            averagesShown = false;
          }
          if (mezclaShown) {
            tft.fillRect(1, 50, 220, 115, TFT_BLACK);
            tft.drawFastVLine(75, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            mezclaShown = false;
          }
          imprimirAlgoritmo(mezcla);
        }
        // Ver mezcla
        else if (puntoEnArea(touchX, touchY, 10, 175, bigIconW, bigIconH)) {
          ultimoToque = millis();
          mezclaShown = !mezclaShown;
          if (mezclaShown) {
            if (averagesShown) {
              tft.fillRect(100, 75, 219, 90, TFT_BLACK);
              tft.drawFastVLine(245, 165, 60, TFT_BLACK);
              tft.drawFastHLine(0, 165, 320, TFT_WHITE);
              averagesShown = false;
            }
            drawMezcla();
          } else {
            tft.fillRect(1, 50, 220, 115, TFT_BLACK);
            tft.drawFastVLine(75, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            imprimirAlgoritmo(mezcla);
          }
        }
        // Rehacer última mezcla
        else if (puntoEnArea(touchX, touchY, 112, 205, iconW, iconH)) {
          if (ultimaMezcla != "") {
            imprimirAlgoritmo(ultimaMezcla);
            mostrarTiempo(0);
          }
        }
        // DNF
        else if (puntoEnArea(touchX, touchY, 150, 206, iconW, iconH)) {
          if (tiempoTranscurrido > 0) {
            esDnf = !esDnf;
            long t = esDnf ? -1 : tiempoTranscurrido;
            mostrarTiempo(t);
            actualizarRegistro(t);
          }
        }
        // +2 segundos
        else if (puntoEnArea(touchX, touchY, 186, 205, iconW, iconH)) {
          if (tiempoTranscurrido > 0) {
            tiempoTranscurrido += 2000;
            mostrarTiempo(tiempoTranscurrido);
            actualizarRegistro(tiempoTranscurrido);
          }
        }
        // Nueva mezcla
        else if (puntoEnArea(touchX, touchY, 222, 206, iconW, iconH)) {
          ultimoToque = millis();
          mezcla = generarMezcla();
          imprimirAlgoritmo(mezcla);
        }
        // Ver medias (Averages)
        else if (puntoEnArea(touchX, touchY, 256, 175, bigIconW, bigIconH)) {
          ultimoToque = millis();
          averagesShown = !averagesShown;
          if (averagesShown) {
            if (mezclaShown) {
              tft.fillRect(1, 50, 220, 115, TFT_BLACK);
              tft.drawFastVLine(75, 165, 60, TFT_BLACK);
              tft.drawFastHLine(0, 165, 320, TFT_WHITE);
              mezclaShown = false;
            }
            drawAverages();
          } else {
            tft.fillRect(100, 75, 219, 90, TFT_BLACK);
            tft.drawFastVLine(245, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            if (CUBOS[cuboActual].label == " 4x4 " || CUBOS[cuboActual].label == " 5x5 " || CUBOS[cuboActual].label == " 6x6 " || CUBOS[cuboActual].label == " 7x7 " || CUBOS[cuboActual].label == "clock" 
              || CUBOS[cuboActual].label == "4blnd" || CUBOS[cuboActual].label == "5blnd" || CUBOS[cuboActual].label == "megam" || CUBOS[cuboActual].label == " fto " || CUBOS[cuboActual].label == " sq1 ") {
              imprimirAlgoritmo(mezcla);
            }
          }
        }
      }
      break;
  }

  // Lectura del sensor para el Timer
  bool tocadoSensor = analogRead(SENSOR_PIN) > UMBRAL_PRESION;

  switch (estado) {
    case DETENIDO:
      if (tocadoSensor && (millis() > debounceFinTimer)) {
        tiempoMano = millis();
        tft.fillCircle(245, 177, 10, TFT_RED);
        estado = ESPERANDO;
      }
      break;

    case ESPERANDO:
      if (!tocadoSensor) {
        tft.fillCircle(245, 177, 10, TFT_BLACK);
        estado = DETENIDO;
      } else if (millis() - tiempoMano >= 500) {
        tft.fillCircle(245, 177, 10, TFT_GREEN);
        estado = PREPARADO;
      }
      break;

    case PREPARADO:
      if (!tocadoSensor) {
        tiempoInicio = millis();
        tft.fillCircle(245, 177, 10, TFT_BLACK);
        estado = CORRIENDO;
      }
      break;

    case CORRIENDO:
      tiempoTranscurrido = millis() - tiempoInicio;
      mostrarTiempo(tiempoTranscurrido);

      if (tocadoSensor && (tiempoTranscurrido > 200)) {
        estado = DETENIDO;
        debounceFinTimer = millis() + 500;
        esDnf = false;

        actualizarRegistro(tiempoTranscurrido);
        ultimaMezcla = mezcla;
        mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);
        mostrarTiempo(tiempoTranscurrido);
      }
      break;
  }
}

// Helpers SD
String getCubePath(String appendage) {
  return "/" + String(CUBOS[cuboActual].file) + appendage;
}

void actualizarRegistro(long ms) {
  String path = getCubePath(".txt");
  eliminarUltimaLinea(path.c_str());
  addLinea(path.c_str(), String(ms) + "," + mezcla);
}

// Dibujado de pantallas
void drawTimes() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(131, 25, 49, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 210, TFT_BLACK);
}

void drawStats() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(181, 25, 49, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 210, TFT_BLACK);
}

void drawTimer() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(81, 25, 49, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 210, TFT_BLACK);
  tft.fillRect(1, 1, 79, 23, TFT_BLACK);
  tft.setCursor(12, 6);
  tft.print(CUBOS[cuboActual].label);

  tft.drawFastHLine(0, 165, 320, TFT_WHITE);

  tft.pushImage(10,  175, bigIconW, bigIconH, vermezclaD);
  tft.pushImage(75,  205, iconW,    iconH,    eliminarD);
  tft.pushImage(112, 205, iconW,    iconH,    againD);
  tft.pushImage(150, 206, iconW,    iconH,    dnfD);
  tft.pushImage(186, 205, iconW,    iconH,    plustwoD);
  tft.pushImage(222, 206, iconW,    iconH,    nuevamezclaD);
  tft.pushImage(256, 175, bigIconW, bigIconH, bigsolvesD);
}

void drawAverages() {
  tft.fillRect(100, 75, 219, 90, TFT_BLACK);
  tft.fillRect(246, 163, 73, 5, TFT_BLACK);
  tft.drawFastHLine(100, 75, 220, TFT_WHITE);
  tft.drawFastVLine(100, 75, 90, TFT_WHITE);
  tft.drawFastVLine(245, 165, 30, TFT_WHITE);

  //TODO hay que calcular y escribir las averages
}

void drawMezcla() {
  tft.fillRect(1, 50, 220, 115, TFT_BLACK);
  tft.fillRect(1, 163, 74, 5, TFT_BLACK);
  tft.drawFastHLine(1, 50, 220, TFT_WHITE);
  tft.drawFastVLine(220, 50, 115, TFT_WHITE);
  tft.drawFastVLine(75, 165, 30, TFT_WHITE);

  //TODO hay que escribir el código para que se muestre la mezcla
}

void drawCubes() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(1, 25, 79, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 210, TFT_BLACK);

  for (size_t i = 0; i < TOTAL_CUBOS; i++) {
    if (CUBOS[i].page == paginaCubos) {
      tft.pushImage(CUBOS[i].x, CUBOS[i].y, bigIconW, bigIconH, CUBOS[i].icon);
    }
  }

  tft.drawString((paginaCubos == 1) ? "[>]" : "[<]", FLECHA_X, FLECHA_Y, 1);
}

void drawBasic() {
  tft.setTextSize(2);
  tft.drawRect(0, 0, 320, 240, TFT_WHITE);
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.drawFastVLine(80,  0, 25, TFT_WHITE);
  tft.drawFastVLine(130, 0, 25, TFT_WHITE);
  tft.drawFastVLine(180, 0, 25, TFT_WHITE);
  tft.drawFastVLine(230, 0, 25, TFT_WHITE);

  tft.pushImage(92,  1, iconW, iconH, cronoD);
  tft.pushImage(142, 1, iconW, iconH, solvesD);
  tft.pushImage(192, 1, iconW, iconH, estatsD);
}

String generarMezcla() {
  paginaScramble = 0;
  return CUBOS[cuboActual].scrambler().c_str();
}

void mostrarTiempo(long ms) {
  tft.fillRect(75, 175, 150, 25, TFT_BLACK);
  tft.setTextSize(3);

  if (ms < 0) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("DNF", 158, 175, 1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  } else {
    unsigned long minutos = ms / 60000;
    unsigned long segundos = (ms % 60000) / 1000;
    unsigned long centesimas = (ms % 1000) / 10;

    char buffer[16];
    if (minutos > 0) {
      snprintf(buffer, sizeof(buffer), "%lu:%02lu.%02lu", minutos, segundos, centesimas);
    } else {
      snprintf(buffer, sizeof(buffer), "%lu.%02lu", segundos, centesimas);
    }
    tft.drawCentreString(buffer, 158, 175, 1);
  }
  tft.setTextSize(2);
}

void imprimirAlgoritmo(const String& algoritmo) {
  tft.fillRect(1, 26, 316, 137, TFT_BLACK);
  tft.setTextFont(1);

  int x = 10, y = 30;
  int cursorX = x, cursorY = y;
  int anchoMax = 310;
  int espacioAncho = tft.textWidth(" ", 1);
  int altoLinea = tft.fontHeight(1);

  bool requierePaginacion = (cuboActual >= 4 && cuboActual <= 5) || cuboActual == 10;
  int movActual = 0;
  String movimiento = "";

  for (unsigned int i = 0; i <= algoritmo.length(); i++) {
    if (i < algoritmo.length() && algoritmo[i] != ' ') {
      movimiento += algoritmo[i];
    } else if (movimiento.length() > 0) {
      movActual++;
      int anchoMov = tft.textWidth(movimiento, 1);

      if (cursorX + anchoMov > x + anchoMax) {
        cursorX = x;
        cursorY += altoLinea;
      }

      bool colision = requierePaginacion && (cursorY >= 135) && (cursorX + anchoMov >= 240);
      bool dentro = (cursorY <= 150);
      bool dibujar = (!requierePaginacion || 
                     (paginaScramble == 0 && movActual <= 50) ||
                     (paginaScramble == 1 && movActual > 50)) && !colision && dentro;

      if (dibujar) {
        tft.setCursor(cursorX, cursorY);
        tft.print(movimiento);
        cursorX += anchoMov + espacioAncho;
      }
      movimiento = "";
    }
  }

  if (requierePaginacion) {
    tft.drawString((paginaScramble == 0) ? "[1/2]" : "[2/2]", 250, 145, 1);
  }
}

// Manejo de SD
bool addLinea(const char* path, const String& linea) {
  if (!sdDisponible) return false;
  File file = SD.open(path, FILE_APPEND);
  if (!file) return false;
  file.println(linea);
  file.close();
  return true;
}

bool eliminarUltimaLinea(const char* path) {
  if (!sdDisponible) return false;

  File file = SD.open(path, FILE_READ);
  if (!file) return false;

  size_t tamano = file.size();
  if (tamano == 0) {
    file.close();
    return false;
  }

  size_t bytesALeer = (tamano > 512) ? 512 : tamano;
  size_t offset = tamano - bytesALeer;

  file.seek(offset);
  uint8_t buffer[512];
  file.read(buffer, bytesALeer);
  file.close();

  int i = bytesALeer - 1;
  while (i >= 0 && (buffer[i] == '\n' || buffer[i] == '\r')) i--;
  while (i >= 0 && buffer[i] != '\n') i--;

  size_t nuevoTamano = (i < 0) ? 0 : (offset + i + 1);
  String posixPath = "/sd" + String(path);
  return truncate(posixPath.c_str(), nuevoTamano) == 0;
}