#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <unistd.h>
#include <ctime>
#include <cmath>
#include <deque>
#include <vector>
#include <algorithm>

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
#define MAX_BUFFER_SOLVES 20000

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

std::deque<long> sessionSolves;

int cuboActual = 1; // Índice por defecto (3x3)
unsigned long tiempoMano = 0;
unsigned long tiempoInicio = 0;
unsigned long tiempoTranscurrido = 0;
unsigned long ultimoToque = 0;
unsigned long debounceFinTimer = 0;

int pantallaActual = 1;
String mezcla = "";
String ultimaMezcla = "";
SolveRecord ultimaSolve;
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
void registrarTiempo(long ms);
int32_t calcularAO(int n);
void actualizarRegistro(long ms);
void eliminarUltimaSolve();
void loadSession();

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
  loadSession();
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
            loadSession();
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
        // Eliminar última solve
        else if (puntoEnArea(touchX, touchY, 75, 205, iconW, iconH)) {
          ultimoToque = millis();
          eliminarUltimaSolve();
        }
        // Rehacer última mezcla
        else if (puntoEnArea(touchX, touchY, 112, 205, iconW, iconH)) {
          if (ultimaMezcla != "") {
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
            imprimirAlgoritmo(mezcla);
          }
        }
      }
      break;
    case 2: // pestaña de solves
      break;
    case 3: // pestaña de stats
      break;
    case 4: // ajustes
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

        ultimaMezcla = mezcla;
        mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);
        registrarTiempo(tiempoTranscurrido);
        mostrarTiempo(tiempoTranscurrido);
      }
      break;
  }
}

// Dibujado de pantallas
void drawTimes() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(131, 25, 49, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 210, TFT_BLACK);

  tft.drawFastHLine(0, 215, 320, TFT_WHITE);
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

  int32_t ao5    = (sessionSolves.size() >= 5)    ? calcularAO(5)    : -2;
  int32_t ao12   = (sessionSolves.size() >= 12)   ? calcularAO(12)   : -2;
  int32_t ao50   = (sessionSolves.size() >= 50)   ? calcularAO(50)   : -2;
  int32_t ao100  = (sessionSolves.size() >= 100)  ? calcularAO(100)  : -2;
  int32_t ao1000 = (sessionSolves.size() >= 1000) ? calcularAO(1000) : -2;

  int32_t bestSingle = -1;
  int64_t sumaTiempos = 0;
  uint32_t validos = 0;

  for (size_t i = 0; i < sessionSolves.size(); i++) {
    long t = sessionSolves[i];
    if (t >= 0) {
      if (bestSingle == -1 || t < bestSingle) {
        bestSingle = t;
      }
      sumaTiempos += t;
      validos++;
    }
  }

  int32_t mediaAritmetica = (validos > 0) ? (int32_t)(sumaTiempos / validos) : -2;

  char buf[16];
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Columna Izquierda (Ao5, Ao12, Ao50, Ao100)
  tft.drawString("Ao5: ", 106, 85);
  formatearTiempoAO(ao5, buf, sizeof(buf));
  tft.drawString(buf, 146, 85);

  tft.drawString("Ao12: ", 106, 105);
  formatearTiempoAO(ao12, buf, sizeof(buf));
  tft.drawString(buf, 146, 105);

  tft.drawString("Ao50: ", 106, 125);
  formatearTiempoAO(ao50, buf, sizeof(buf));
  tft.drawString(buf, 146, 125);

  tft.drawString("Ao100: ", 106, 145);
  formatearTiempoAO(ao100, buf, sizeof(buf));
  tft.drawString(buf, 146, 145);

  // Columna Derecha (Ao1k, Media, Best, Cuenta)
  tft.drawString("Ao1k: ", 220, 85);
  formatearTiempoAO(ao1000, buf, sizeof(buf));
  tft.drawString(buf, 265, 85);

  tft.drawString("Media: ", 220, 105);
  formatearTiempoAO(mediaAritmetica, buf, sizeof(buf));
  tft.drawString(buf, 265, 105);

  tft.drawString("Best: ", 220, 125);
  formatearTiempoAO((bestSingle != -1) ? bestSingle : -2, buf, sizeof(buf));
  tft.drawString(buf, 265, 125);

  tft.drawString("Cuenta: ", 220, 145);
  snprintf(buf, sizeof(buf), "%u", (unsigned int)sessionSolves.size());
  tft.drawString(buf, 265, 145);

  tft.setTextSize(2);
}

void formatearTiempoAO(int32_t ms, char* buffer, size_t len) {
  if (ms == -1) {
    snprintf(buffer, len, "DNF");
  } else if (ms == -2) {
    snprintf(buffer, len, "--");
  } else {
    unsigned long minutos = ms / 60000;
    unsigned long segundos = (ms % 60000) / 1000;
    unsigned long centesimas = (ms % 1000) / 10;
    if (minutos > 0) {
      snprintf(buffer, len, "%lu:%02lu.%02lu", minutos, segundos, centesimas);
    } else {
      snprintf(buffer, len, "%lu.%02lu", segundos, centesimas);
    }
  }
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
  tft.setTextSize(2);

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
void registrarTiempo(long ms) {
  if (!sdDisponible) return;

  addToSession(ms);

  int32_t tiempo = ms;
  int32_t ao5 = -1;
  int32_t ao12 = -1;
  if (sessionSolves.size() >= 5) {
    ao5 = calcularAO(5);
  }
  if (sessionSolves.size() >= 12) {
    ao12 = calcularAO(12);
  }

  String path = getCubePath(".txt");
  File file = SD.open(path.c_str(), FILE_APPEND);
  if (!file) return;
  uint32_t offsetMezcla = file.position();
  uint32_t longMezcla = ultimaMezcla.length();
  file.print(ultimaMezcla);
  file.close();

  SolveRecord solve = { 
    (int32_t)ms, 
    ao5, 
    ao12, 
    0,
    offsetMezcla, 
    longMezcla 
  };
  ultimaSolve = solve;
  path = getCubePath(".dat");
  file = SD.open(path.c_str(), FILE_APPEND);
  if (!file) return;
  file.write((const uint8_t*)&solve, sizeof(SolveRecord));
  file.close();
}

int32_t calcularAO(int n) {
  if (sessionSolves.size() < (size_t)n) return -1;

  std::vector<long> ventana(sessionSolves.end() - n, sessionSolves.end());

  int dnfs = 0;
  for (size_t i = 0; i < ventana.size(); i++) {
    if (ventana[i] < 0) {
      dnfs++;
      ventana[i] = 2147483647;
    }
  }

  if (dnfs > 1) return -1;

  std::sort(ventana.begin(), ventana.end());

  int64_t suma = 0;
  for (int i = 1; i < n - 1; i++) {
    suma += ventana[i];
  }

  return (int32_t)(suma / (n - 2));
}

void addToSession(long ms) {
  if (sessionSolves.size() >= MAX_BUFFER_SOLVES) {
    sessionSolves.pop_front();
  }
  sessionSolves.push_back(ms);
}

String getCubePath(String appendage) {
  return "/" + String(CUBOS[cuboActual].file) + appendage;
}

void actualizarRegistro(long ms) {
  if (!sdDisponible || sessionSolves.empty()) return;

  sessionSolves.back() = ms;
  int32_t ao5 = calcularAO(5);
  int32_t ao12 = calcularAO(12);

  String path = getCubePath(".dat");
  File file = SD.open(path.c_str(), "r+");
  if (!file) return;

  size_t size = file.size();
  if (size < sizeof(SolveRecord)) {
    file.close();
    return;
  }

  size_t posUltimo = size - sizeof(SolveRecord);
  file.seek(posUltimo);

  SolveRecord previo;
  file.read((uint8_t*)&previo, sizeof(SolveRecord));

  SolveRecord solve = { 
    (int32_t)ms, 
    ao5, 
    ao12, 
    previo.archivado,
    previo.offsetMezcla, 
    previo.longMezcla 
  };

  file.seek(posUltimo);
  file.write((const uint8_t*)&solve, sizeof(SolveRecord));
  file.close();

  ultimaSolve = solve;
}

void eliminarUltimaSolve() {
  if (!sdDisponible || sessionSolves.empty()) return;

  sessionSolves.pop_back();

  String path = getCubePath(".dat");
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) return;

  size_t size = file.size();
  if (size < sizeof(SolveRecord)) {
    file.close();
    return;
  }

  SolveRecord registroAEliminar;
  file.seek(size - sizeof(SolveRecord));
  file.read((uint8_t*)&registroAEliminar, sizeof(SolveRecord));
  file.close();

  String posixTxt = "/sd" + getCubePath(".txt");
  truncate(posixTxt.c_str(), registroAEliminar.offsetMezcla);

  String posix = "/sd" + path;
  truncate(posix.c_str(), size - sizeof(SolveRecord));

  tiempoTranscurrido = 0;
  esDnf = false;
  ultimaSolve = {};
  mostrarTiempo(0);
}

void loadSession() {
  sessionSolves.clear();
  ultimaSolve = {};
  ultimaMezcla = "";

  if (!sdDisponible) return;

  String path = getCubePath(".dat");
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) return;

  size_t totalRecords = file.size() / sizeof(SolveRecord);
  if (totalRecords == 0) {
    file.close();
    return;
  }

  SolveRecord bloque[BUFFER_RECORDS];

  for (size_t i = 0; i < totalRecords; i += BUFFER_RECORDS) {
    size_t aLeer = std::min((size_t)BUFFER_RECORDS, totalRecords - i);
    file.read((uint8_t*)bloque, aLeer * sizeof(SolveRecord));

    for (size_t j = 0; j < aLeer; j++) {
      if (bloque[j].archivado == 0) {
        addToSession(bloque[j].tiempo);
        ultimaSolve = bloque[j];
      }
    }
  }
  file.close();

  if (ultimaSolve.longMezcla > 0) {
    path = getCubePath(".txt");
    File file = SD.open(path.c_str(), FILE_READ);
    if (file) {
      file.seek(ultimaSolve.offsetMezcla);
      char* buf = new char[ultimaSolve.longMezcla + 1];
      file.read((uint8_t*)buf, ultimaSolve.longMezcla);
      buf[ultimaSolve.longMezcla] = '\0';
      ultimaMezcla = String(buf);
      delete[] buf;
      file.close();
    }
  }
}