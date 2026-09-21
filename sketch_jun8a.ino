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

#define DEBOUNCE_MS 300

#define BUFFER_RECORDS 64
#define MAX_BUFFER_SOLVES 20000

#define BUTTON_W 60
#define BUTTON_H 25
#define BUTTON_CUBE_W 80
#define FLECHA_X 280
#define FLECHA_Y 218
#define FLECHA_WH 30

#define SOLVES_GRID_COLS 4
#define SOLVES_GRID_ROWS 6
#define SOLVES_PER_PAGE (SOLVES_GRID_COLS * SOLVES_GRID_ROWS) // 12 solves por pantalla

#define GRID_START_X 8
#define GRID_START_Y 32
#define CELL_W 71
#define CELL_H 24
#define CELL_GAP_X 6
#define CELL_GAP_Y 6

#define ELIMINAR_TIEMPO 0
#define ELIMINAR_SESION 1
#define DESARCHIVAR_TIEMPOS 2
#define ARCHIVAR_SESION 3
#define ARCHIVAR_TIEMPO 4

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
  uint8_t archivado;
  uint8_t penalty;
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

struct PopupData {
  SolveRecord record;
  String scramble;
  int indiceGlobal;
};

struct SessionItem {
  int32_t  tiempo;
  uint8_t  penalty;
  uint32_t indexSD;
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

std::deque<SessionItem> sessionSolves;

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
bool averagesShown = false;
bool mezclaShown = false;
int paginaSolves = 0;
int solveSeleccionada = -1;
bool popupSolveVisible = false;
PopupData solvePopup;
bool popupSiNoVisible = false;
int accionPendiente = -1;
bool popupNumVisible = false;
int parametroN = 0;

inline bool puntoEnArea(int px, int py, int x, int y, int w, int h) {
  return (px >= x && px <= (x + w) && py >= y && py <= (y + h));
}

inline long getTiempoEfectivo(int32_t tiempoBase, uint8_t penalty) {
  if (penalty == 2) return -1; // DNF
  if (penalty == 1) return tiempoBase + 2000; // +2s
  return tiempoBase; // OK
}

void setup() {
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);
  pinMode(SENSOR_PIN, INPUT);

  tft.init();
  //tft.invertDisplay(true);
  //tft.setRotation(1);
  //uint16_t calData[5] = { 229, 3433, 369, 3383, 1 };
  tft.setRotation(3);
  uint16_t calData[5] = { 210, 3456, 371, 3387, 7 };
  //uint16_t calData[5];
  //tft.calibrateTouch(calData, TFT_WIHTE, TFT_BLACK, 15);
  tft.setTouch(calData);
  //for (int i = 0; i < 5; i++) {
  //  tft.println(calData[i]);
  //  if (i < 4) Serial.print(", ");
  //}
  tft.setSwapBytes(true);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdDisponible = SD.begin(SD_CS, sdSPI);

  tft.fillScreen(TFT_BLACK);
  std::srand(esp_random());

  drawBasic();
  drawTimer();
  loadSession();
  mezcla = generarMezcla();
  imprimirAlgoritmo(mezcla);
  //for (int i = 0; i < 30; i++) {
  //  ultimaMezcla = generarMezcla();
  //  long tiempoAleatorio = random(10000, 15001);
  //  registrarTiempo(tiempoAleatorio);
  //}
  mostrarTiempo(0);
}

void loop() {
  uint16_t touchX = 0, touchY = 0;
  bool tocadoPantalla = tft.getTouch(&touchX, &touchY);

  if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {

    // Popups
    if (popupSiNoVisible) {
      procesarToquePopupSiNo(touchX, touchY);
      return;
    }
    else if (popupNumVisible) {
      procesarToquePopupNum(touchX, touchY);
      return;
    }

    // Barra de pestañas superior
    if (puntoEnArea(touchX, touchY, 0, 0, BUTTON_CUBE_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 0) { pantallaActual = 0; drawCubes(); }
    } 
    // Timer
    else if (puntoEnArea(touchX, touchY, 80, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 1) {
        pantallaActual = 1;
        drawTimer();
        if (mezcla.length() == 0) mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);
        if (ultimaSolve.tiempo != 0) mostrarTiempo(getTiempoEfectivo(ultimaSolve.tiempo, ultimaSolve.penalty));
        else mostrarTiempo(0);
      }
    } 
    // Times
    else if (puntoEnArea(touchX, touchY, 140, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 2) {
        pantallaActual = 2; drawTimes();
        averagesShown = false;
        mezclaShown = false;
      }
    } 
    // Stats
    else if (puntoEnArea(touchX, touchY, 200, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 3) {
        pantallaActual = 3; drawStats();
        averagesShown = false;
        mezclaShown = false;
      }
    }
    // Settings
    else if (puntoEnArea(touchX, touchY, 260, 0, BUTTON_CUBE_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 4) {
        pantallaActual = 4; drawSettings();
        averagesShown = false;
        mezclaShown = false;
      }
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
          if (!sessionSolves.empty() && tiempoTranscurrido != 0) {
            abrirPopupSiNo(ELIMINAR_TIEMPO, 0);
          }
        }
        // Rehacer última mezcla
        else if (puntoEnArea(touchX, touchY, 112, 205, iconW, iconH)) {
          ultimoToque = millis();
          if (ultimaMezcla != "" && tiempoTranscurrido != 0) {
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
          }
        }
        // DNF
        else if (puntoEnArea(touchX, touchY, 150, 206, iconW, iconH)) {
          ultimoToque = millis();
          if (tiempoTranscurrido > 0) {
            uint8_t nuevaPen = (ultimaSolve.penalty == 2) ? 0 : 2;
            actualizarRegistro(nuevaPen);
            mostrarTiempo(getTiempoEfectivo(ultimaSolve.tiempo, ultimaSolve.penalty));
          }
        }
        // +2 segundos
        else if (puntoEnArea(touchX, touchY, 186, 205, iconW, iconH)) {
          ultimoToque = millis();
          if (tiempoTranscurrido > 0) {
            uint8_t nuevaPen = (ultimaSolve.penalty == 1) ? 0 : 1;
            actualizarRegistro(nuevaPen);
            mostrarTiempo(getTiempoEfectivo(ultimaSolve.tiempo, ultimaSolve.penalty));
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
      if (tocadoPantalla && (millis() - ultimoToque > DEBOUNCE_MS)) {
        ultimoToque = millis();

        if (popupSolveVisible) {
          // cerrar
          if (puntoEnArea(touchX, touchY, 275, 36, 22, 22)) {
            cerrarPopupSolve();
          }
          // ver mezcla
          else if (puntoEnArea(touchX, touchY, 20, 176, 42, 42)) {
            //drawMezclaPeque(); //dos métodos pero ambos llaman a generar mezcla en cada scrambler
          }
          // DNF
          else if (puntoEnArea(touchX, touchY, 155, 198, iconW, iconH)) {
            solvePopup.record.penalty = (solvePopup.record.penalty == 2) ? 0 : 2;

            uint32_t idxSD = sessionSolves[solvePopup.indiceGlobal].indexSD;
            String pathDat = getCubePath(".dat");
            File f = SD.open(pathDat.c_str(), "r+");
            if (f) {
              f.seek((size_t)idxSD * sizeof(SolveRecord));
              f.write((const uint8_t*)&solvePopup.record, sizeof(SolveRecord));
              f.close();
            }
            sessionSolves[solvePopup.indiceGlobal].penalty = solvePopup.record.penalty;
            if (solvePopup.record.offsetMezcla == ultimaSolve.offsetMezcla) ultimaSolve = solvePopup.record;
            drawPopupSolve();
          }
          // +2
          else if (puntoEnArea(touchX, touchY, 193, 198, iconW, iconH)) {
            solvePopup.record.penalty = (solvePopup.record.penalty == 1) ? 0 : 1;

            uint32_t idxSD = sessionSolves[solvePopup.indiceGlobal].indexSD;
            String pathDat = getCubePath(".dat");
            File f = SD.open(pathDat.c_str(), "r+");
            if (f) {
              f.seek((size_t)idxSD * sizeof(SolveRecord));
              f.write((const uint8_t*)&solvePopup.record, sizeof(SolveRecord));
              f.close();
            }
            sessionSolves[solvePopup.indiceGlobal].penalty = solvePopup.record.penalty;
            if (solvePopup.record.offsetMezcla == ultimaSolve.offsetMezcla) ultimaSolve = solvePopup.record;
            drawPopupSolve();
          }
          // archivar
          else if (puntoEnArea(touchX, touchY, 235, 198, iconW, iconH)) {
            abrirPopupSiNo(ARCHIVAR_TIEMPO, 0);
          }
          // eliminar
          else if (puntoEnArea(touchX, touchY, 275, 198, iconW, iconH)) {
            abrirPopupSiNo(ELIMINAR_TIEMPO, 0);
          }
          break;
        }
        // eliminar sesión
        if (puntoEnArea(touchX, touchY, 12, 215, iconW, iconH)) {
          abrirPopupSiNo(ELIMINAR_SESION, 0);
        }
        // archivar sesión
        else if (puntoEnArea(touchX, touchY, 62, 215, iconW, iconH)) {
          abrirPopupSiNo(ARCHIVAR_SESION, 0);
        }
        // desarchivar sesión
        else if (puntoEnArea(touchX, touchY, 112, 215, iconW, iconH)) {
          abrirPopupNum();
        }
        // flecha izquierda
        else if (puntoEnArea(touchX, touchY, 160, 216, 40, 24)) {
          if (paginaSolves > 0) {
            paginaSolves--;
            drawTimes();
          }
        }
        // flecha derecha
        else if (puntoEnArea(touchX, touchY, 270, 216, 40, 24)) {
          size_t total = sessionSolves.size();
          int totalPags = (total == 0) ? 1 : ((total - 1) / SOLVES_PER_PAGE + 1);
          if (paginaSolves + 1 < totalPags) {
            paginaSolves++;
            drawTimes();
          }
        }
        // Selección de solve
        else {
          int primerIndice = sessionSolves.size() - 1 - (paginaSolves * SOLVES_PER_PAGE);
          for (int r = 0; r < SOLVES_GRID_ROWS; r++) {
            for (int c = 0; c < SOLVES_GRID_COLS; c++) {
              int x = GRID_START_X + c * (CELL_W + CELL_GAP_X);
              int y = GRID_START_Y + r * (CELL_H + CELL_GAP_Y);
              if (puntoEnArea(touchX, touchY, x, y, CELL_W, CELL_H)) {
                int itemIdx = r * SOLVES_GRID_COLS + c;
                int solveIdx = primerIndice - itemIdx;
                if (solveIdx >= 0 && (size_t)solveIdx < sessionSolves.size()) {
                  abrirPopupSolve(solveIdx);
                }
                break;
              }
            }
          }
        }
      }
      break;
    case 3: // pestaña de stats
      break;
    case 4: // ajustes
      break;
  }

  // Lectura del sensor para el Timer
  // TODO rehacer para cuando tenga el sensor -> digitalRead();
  bool tocadoSensor = (digitalRead(SENSOR_PIN) == HIGH) && pantallaActual == 1;

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

        ultimaMezcla = mezcla;
        mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);
        registrarTiempo(tiempoTranscurrido);
        mostrarTiempo(tiempoTranscurrido);
      }
      break;
  }
}

void drawTimes() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(141, 25, 59, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 210, TFT_BLACK);

  tft.pushImage(12,  215, iconW, iconH, binD);
  tft.pushImage(62,  215, iconW, iconH, archivarD);
  tft.pushImage(112, 215, iconW, iconH, desarchivarD);

  tft.drawRect(0, 0, 320, 240, TFT_WHITE);
  tft.drawFastHLine(0, 215, 320, TFT_WHITE);
  tft.drawFastVLine(50, 215, 25, TFT_WHITE);
  tft.drawFastVLine(100, 215, 25, TFT_WHITE);
  tft.drawFastVLine(150, 215, 25, TFT_WHITE);

  popupSolveVisible = false;

  size_t totalSolves = sessionSolves.size();
  int totalPaginas = (totalSolves == 0) ? 1 : ((totalSolves - 1) / SOLVES_PER_PAGE + 1);
  if (paginaSolves >= totalPaginas) paginaSolves = totalPaginas - 1;
  if (paginaSolves < 0) paginaSolves = 0;

  char buf[32];
  tft.setTextFont(1);

  // Barra inferior: botones de paginación e info
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("<", 170, 220);
  tft.drawString(">", 290, 220);

  snprintf(buf, sizeof(buf), "%d/%d", paginaSolves + 1, totalPaginas);
  tft.drawString(buf, 220, 220);

  if (totalSolves == 0) {
    tft.setTextSize(3);
    tft.drawCentreString("Sin solves", 160, 110, 1);
    return;
  }

  // Cuadrícula de 24 celdas
  int primerIndice = totalSolves - 1 - (paginaSolves * SOLVES_PER_PAGE);

  for (int row = 0; row < SOLVES_GRID_ROWS; row++) {
    for (int col = 0; col < SOLVES_GRID_COLS; col++) {
      int itemIdx = row * SOLVES_GRID_COLS + col;
      int solveIdx = primerIndice - itemIdx;

      int x = GRID_START_X + col * (CELL_W + CELL_GAP_X);
      int y = GRID_START_Y + row * (CELL_H + CELL_GAP_Y);

      if (solveIdx >= 0) {
        tft.drawRect(x, y, CELL_W, CELL_H, TFT_WHITE);

        tft.setTextSize(1);
        SessionItem item = sessionSolves[solveIdx];
        if (item.penalty == 2) {
          tft.setTextColor(TFT_RED, TFT_BLACK);
          tft.drawCentreString("DNF", x + (CELL_W / 2), y + 7, 1);
        } else {
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          long tEfectivo = getTiempoEfectivo(item.tiempo, item.penalty);
          formatearTiempoAO(tEfectivo, buf, sizeof(buf));
          if (item.penalty == 1) {
            strncat(buf, "+", sizeof(buf) - strlen(buf) - 1);
          }
          tft.drawCentreString(buf, x + (CELL_W / 2), y + 7, 1);
        }
      }
    }
  }
}

void abrirPopupSolve(int indiceSession) {
  if (!sdDisponible) return;

  uint32_t idxSD = sessionSolves[indiceSession].indexSD;

  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), FILE_READ);
  if (!fileDat) return;

  size_t offsetByte = (size_t)idxSD * sizeof(SolveRecord);
  if (fileDat.size() < offsetByte + sizeof(SolveRecord)) {
    fileDat.close();
    return;
  }

  fileDat.seek(offsetByte);
  fileDat.read((uint8_t*)&solvePopup.record, sizeof(SolveRecord));
  fileDat.close();

  solvePopup.scramble = "";
  solvePopup.indiceGlobal = indiceSession;

  if (solvePopup.record.longMezcla > 0) {
    String pathTxt = getCubePath(".txt");
    File fileTxt = SD.open(pathTxt.c_str(), FILE_READ);
    if (fileTxt) {
      fileTxt.seek(solvePopup.record.offsetMezcla);
      char* buf = new char[solvePopup.record.longMezcla + 1];
      fileTxt.read((uint8_t*)buf, solvePopup.record.longMezcla);
      buf[solvePopup.record.longMezcla] = '\0';
      solvePopup.scramble = String(buf);
      delete[] buf;
      fileTxt.close();
    }
  }

  popupSolveVisible = true;
  drawPopupSolve();
}

void cerrarPopupSolve() {
  popupSolveVisible = false;
  drawTimes();
}

void drawPopupSolve() {
  // Marco del popup
  tft.fillRect(15, 30, 290, 195, TFT_BLACK);
  tft.drawRect(15, 30, 290, 195, TFT_WHITE);
  tft.drawFastHLine(15, 170, 52, TFT_WHITE);
  tft.drawFastVLine(67, 170, 54, TFT_WHITE);
  tft.drawFastVLine(150, 196, 28, TFT_WHITE);
  tft.drawFastHLine(150, 196, 155, TFT_WHITE);

  tft.pushImage(20, 176, 42, 42, vermezclapequeD);
  tft.pushImage(155, 198, iconW, iconH, dnfD);
  tft.pushImage(193, 198, iconW, iconH, plustwoD);
  tft.pushImage(235, 198, iconW, iconH, archivarD);
  tft.pushImage(275, 198, iconW, iconH, binD);

  char buf[32];
  tft.setTextSize(2);
  tft.drawRect(275, 36, 22, 22, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("X", 287, 40, 1);

  // Tiempo central
  tft.setTextSize(3);
  long tEfectivo = getTiempoEfectivo(solvePopup.record.tiempo, solvePopup.record.penalty);
  
  if (solvePopup.record.penalty == 2) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("DNF", 160, 38, 1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    formatearTiempoAO(tEfectivo, buf, sizeof(buf));
    if (solvePopup.record.penalty == 1) {
      strncat(buf, "+", sizeof(buf) - strlen(buf) - 1);
    }
    tft.drawCentreString(buf, 160, 38, 1);
  }

  tft.drawFastHLine(25, 65, 270, TFT_DARKGREY);

  // Mostrar la mezcla envuelta en líneas
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  int x = 25, y = 70;
  int anchoMax = 270;
  int cursorX = x, cursorY = y;
  int altoLinea = tft.fontHeight(1) + 2;
  int espacioAncho = tft.textWidth(" ", 1);

  String palabra = "";
  for (unsigned int i = 0; i <= solvePopup.scramble.length(); i++) {
    if (i < solvePopup.scramble.length() && solvePopup.scramble[i] != ' ') {
      palabra += solvePopup.scramble[i];
    } else if (palabra.length() > 0) {
      int anchoPalabra = tft.textWidth(palabra, 1);
      if (cursorX + anchoPalabra > x + anchoMax) {
        cursorX = x;
        cursorY += altoLinea;
      }
      if (cursorY <= 170) {
        tft.setCursor(cursorX, cursorY);
        tft.print(palabra);
        cursorX += anchoPalabra + espacioAncho;
      }
      palabra = "";
    }
  }

  tft.setTextSize(2);
}

void drawStatGraph() {
  if (sessionSolves.size() < 2) return;

  int graphX = 20, graphY = 30, graphW = 280, graphH = 120;

  // 2. Encontrar mínimo y máximo para escalar el eje Y
  long tMin = sessionSolves[0].tiempo;
  long tMax = tMin;
  for (SessionItem s : sessionSolves) {
    long t = getTiempoEfectivo(s.tiempo, s.penalty);
    if (t < tMin) tMin = t;
    if (t > tMax) tMax = t;
  }
  if (tMin == tMax) tMax += 1000; // Evitar división por cero si todos los tiempos son idénticos

  // Limpiar fondo del gráfico y dibujar ejes
  tft.fillRect(graphX, graphY, graphW, graphH, TFT_BLACK);
  tft.drawRect(graphX, graphY, graphW, graphH, TFT_WHITE);

  // 3. Submuestreo o distribución horizontal:
  // Si hay más tiempos que píxeles de ancho (GRAPH_W), se debe interpolar/avanzar a saltos.
  // En este ejemplo simple, asumimos N puntos distribuidos uniformemente:
  int prevPixelX = 0;
  int prevPixelY = 0;

  for (size_t i = 0; i < sessionSolves.size(); i++) {
    int px = graphX + (int)((i * (graphW - 1)) / (sessionSolves.size() - 1));
    int py = graphY + graphH - 1 - (int)(((sessionSolves[i].tiempo - tMin) * (graphH - 1)) / (tMax - tMin));

    if (i > 0) {
      tft.drawLine(prevPixelX, prevPixelY, px, py, TFT_GREEN);
    }

    prevPixelX = px;
    prevPixelY = py;
  }
}

SessionStats calcularEstadisticasSesion() {
  SessionStats stats;
  if (sessionSolves.empty()) return stats;

  int64_t suma = 0;
  std::vector<long> validos;
  validos.reserve(sessionSolves.size());

  for (const auto& item : sessionSolves) {
    stats.count++;
    long t = getTiempoEfectivo(item.tiempo, item.penalty);
    if (t < 0) {
      stats.dnfs++;
    } else {
      validos.push_back(t);
      suma += t;
      if (stats.bestSingle == -1 || t < stats.bestSingle) stats.bestSingle = t;
      if (stats.worstSingle == -1 || t > stats.worstSingle) stats.worstSingle = t;
    }
  }

  if (!validos.empty()) {
    stats.media = (int32_t)(suma / validos.size());

    // Desviación estándar
    float sumaVarianza = 0.0f;
    for (long t : validos) {
      float diff = t - stats.media;
      sumaVarianza += diff * diff;
    }
    stats.desviacion = std::sqrt(sumaVarianza / validos.size()) / 1000.0f; // en segundos
  }

  stats.currentAo5  = (sessionSolves.size() >= 5)  ? calcularAO(5)  : -1;
  stats.currentAo12 = (sessionSolves.size() >= 12) ? calcularAO(12) : -1;

  return stats;
}

void dibujarGraficaSesion(long tMin, long tMax) {
  if (sessionSolves.size() < 2 || tMin >= tMax) return;

  int gx = 65, gy = 30, gw = 235, gh = 120;

  tft.drawRect(gx, gy, gw, gh, TFT_WHITE);

  int prevX = -1;
  int prevY = -1;
  size_t total = sessionSolves.size();

  // Número de puntos a dibujar: como máximo el ancho en píxeles
  int numPuntos = (total < (size_t)gw) ? total : gw;

  for (int i = 0; i < numPuntos; i++) {
    size_t idx = (total < (size_t)gw) ? i : (i * (total - 1)) / (gw - 1);

    long t = getTiempoEfectivo(sessionSolves[idx].tiempo, sessionSolves[idx].penalty);
    if (t < 0) continue; // Saltar DNFs

    int px = gx + (int)((i * (gw - 1)) / (numPuntos - 1));
    int py = gy + gh - 1 - (int)(((t - tMin) * (gh - 1)) / (tMax - tMin));

    if (prevX != -1) {
      tft.drawLine(prevX, prevY, px, py, TFT_CYAN);
    }
    prevX = px;
    prevY = py;
  }
}

void drawStats() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(201, 25, 59, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 213, TFT_BLACK);

  SessionStats stats = calcularEstadisticasSesion();

  if (stats.count == 0) {
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Sin datos", 160, 110, 1);
    return;
  }

  char buf[32];
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Fila 1: Count y Best
  snprintf(buf, sizeof(buf), "Solves: %u (DNF: %u)", stats.count, stats.dnfs);
  tft.drawString(buf, 20, 160);

  formatearTiempoAO(stats.bestSingle, buf, sizeof(buf));
  tft.drawString("Best: " + String(buf), 190, 160);

  // Fila 2: Media y Desviación
  formatearTiempoAO(stats.media, buf, sizeof(buf));
  tft.drawString("Media: " + String(buf), 20, 175);

  snprintf(buf, sizeof(buf), "Desviacion: %.2fs", stats.desviacion);
  tft.drawString(buf, 190, 175);

  // Fila 3: Ao5 y Ao12 actuales
  formatearTiempoAO(stats.currentAo5, buf, sizeof(buf));
  tft.drawString("Ao5: " + String(buf), 20, 190);

  formatearTiempoAO(stats.currentAo12, buf, sizeof(buf));
  tft.drawString("Ao12: " + String(buf), 190, 190);

  tft.drawFastHLine(1, 155, 320, TFT_WHITE);

  // Dibujar la gráfica en el espacio inferior (270x130 píxeles)
  if (stats.bestSingle != -1 && stats.worstSingle != -1 && stats.bestSingle != stats.worstSingle) {
    // Etiquetas de escala Y
    formatearTiempoAO(stats.worstSingle, buf, sizeof(buf));
    tft.drawString(buf, 20, 30);
    formatearTiempoAO(stats.bestSingle, buf, sizeof(buf));
    tft.drawString(buf, 20, 142);

    dibujarGraficaSesion(stats.bestSingle, stats.worstSingle);
  }

  int y = 50;
  tft.setTextColor(TFT_CYAN);
  tft.drawString("Solve", 20, y + 15);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("Best", 20, y + 30);
  tft.setTextColor(TFT_RED);
  tft.drawString("Ao5", 20, y + 45);
  tft.setTextColor(TFT_GREEN);
  tft.drawString("Ao12", 20, y + 60);
  tft.setTextColor(TFT_WHITE);
}

void drawSettings() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(261, 25, 58, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 213, TFT_BLACK);
}

void drawTimer() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(81, 25, 59, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 213, TFT_BLACK);
  tft.fillRect(1, 1, 79, 23, TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(CUBOS[cuboActual].label, 11, 6, 1);
  
  tft.drawFastHLine(0, 165, 320, TFT_WHITE);

  tft.pushImage(10,  175, bigIconW, bigIconH, vermezclaD);
  tft.pushImage(75,  205, iconW,    iconH,    binD);
  tft.pushImage(112, 205, iconW,    iconH,    againD);
  tft.pushImage(150, 206, iconW,    iconH,    dnfD);
  tft.pushImage(186, 205, iconW,    iconH,    plustwoD);
  tft.pushImage(222, 205, iconW,    iconH,    nuevamezclaD);
  tft.pushImage(256, 175, bigIconW, bigIconH, bigsolvesD);
}

void drawAverages() {
  tft.fillRect(120, 55, 199, 110, TFT_BLACK);
  tft.fillRect(1, 75, 129, 90, TFT_BLACK);
  tft.fillRect(246, 163, 73, 5, TFT_BLACK);
  tft.drawFastHLine(120, 55, 318, TFT_WHITE);
  tft.drawFastHLine(44, 75, 77, TFT_WHITE);
  tft.drawFastVLine(120, 55, 20, TFT_WHITE);
  tft.drawFastVLine(44, 75, 90, TFT_WHITE);
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
    long t = getTiempoEfectivo(sessionSolves[i].tiempo, sessionSolves[i].penalty);
    if (t >= 0) {
      if (bestSingle == -1 || t < bestSingle) {
        bestSingle = t;
      }
      sumaTiempos += t;
      validos++;
    }
  }

  long lastTimes[5] = {-2, -2, -2, -2, -2};
  int total = sessionSolves.size();
  for (int i = 0; i < 5 && i < total; i++) {
    const auto& solve = sessionSolves[total - 1 - i];
    lastTimes[i] = getTiempoEfectivo(solve.tiempo, solve.penalty);
  }

  int32_t mediaAritmetica = (validos > 0) ? (int32_t)(sumaTiempos / validos) : -2;

  char buf[16];
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Columna Izquierda (Ao5, Ao12, Ao50, Ao100)
  tft.drawString("Ao5: ", 54, 85);
  formatearTiempoAO(ao5, buf, sizeof(buf));
  tft.drawString(buf, 92, 85);

  tft.drawString("Ao12: ", 54, 105);
  formatearTiempoAO(ao12, buf, sizeof(buf));
  tft.drawString(buf, 92, 105);

  tft.drawString("Ao50: ", 54, 125);
  formatearTiempoAO(ao50, buf, sizeof(buf));
  tft.drawString(buf, 92, 125);

  tft.drawString("Ao100: ", 54, 145);
  formatearTiempoAO(ao100, buf, sizeof(buf));
  tft.drawString(buf, 92, 145);

  // Columna Derecha (Ao1k, Media, Best, Cuenta)
  tft.drawString("Ao1000: ", 155, 85);
  formatearTiempoAO(ao1000, buf, sizeof(buf));
  tft.drawString(buf, 200, 85);

  tft.drawString("Media: ", 155, 105);
  formatearTiempoAO(mediaAritmetica, buf, sizeof(buf));
  tft.drawString(buf, 200, 105);

  tft.drawString("Best: ", 155, 125);
  formatearTiempoAO((bestSingle != -1) ? bestSingle : -2, buf, sizeof(buf));
  tft.drawString(buf, 200, 125);

  tft.drawString("Cuenta: ", 155, 145);
  snprintf(buf, sizeof(buf), "%u", (unsigned int)sessionSolves.size());
  tft.drawString(buf, 200, 145);

  tft.drawString("Resultados recientes: ", 130, 65);
  for (int i = 0; i < 5; i++) {
    formatearTiempoAO(lastTimes[i], buf, sizeof(buf));
    tft.drawString(buf, 265, 65 + (i * 20));
  }

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
  tft.fillRect(1, 26, 318, 213, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  for (size_t i = 0; i < TOTAL_CUBOS; i++) {
    if (CUBOS[i].page == paginaCubos) {
      tft.pushImage(CUBOS[i].x, CUBOS[i].y, bigIconW, bigIconH, CUBOS[i].icon);
    }
  }
  
  tft.drawString((paginaCubos == 1) ? "[>]" : "[<]", FLECHA_X, FLECHA_Y, 1);
}

void drawBasic() {
  tft.drawRect(0, 0, 320, 240, TFT_WHITE);
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.drawFastVLine(80,  0, 25, TFT_WHITE);
  tft.drawFastVLine(140, 0, 25, TFT_WHITE);
  tft.drawFastVLine(200, 0, 25, TFT_WHITE);
  tft.drawFastVLine(260, 0, 25, TFT_WHITE);

  tft.pushImage(97,  1, iconW, iconH, cronoD);
  tft.pushImage(157, 1, iconW, iconH, solvesD);
  tft.pushImage(217, 1, iconW, iconH, estatsD);
  tft.pushImage(277, 1, iconW, iconH, settingsD);
}

void abrirPopupSiNo(int type, int n) {
  popupSiNoVisible = true;
  accionPendiente = type;
  parametroN = n;

  String option = "";
  switch (type) {
    case ELIMINAR_TIEMPO:
      option = "eliminar este tiempo";
      break;
    case ELIMINAR_SESION:
      option = "eliminar sesion";
      break;
    case DESARCHIVAR_TIEMPOS:
      option = String("desarch. ") + n + " tiempos";
      break;
    case ARCHIVAR_SESION:
      option = "archivar sesion";
      break;
    case ARCHIVAR_TIEMPO:
      option = "archivar tiempo";
      break;
  }

  // Cuadro del popup
  tft.fillRect(15, 60, 290, 120, TFT_BLACK);
  tft.drawRect(15, 60, 290, 120, TFT_WHITE);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.drawCentreString("Confirmar:", 160, 75, 1);
  tft.drawCentreString(option + "?", 160, 95, 1);

  // Botón SÍ
  tft.drawRect(60, 130, 80, 30, TFT_WHITE);
  tft.setTextSize(2);
  tft.drawCentreString("SI", 100, 137, 1);

  // Botón NO
  tft.drawRect(180, 130, 80, 30, TFT_WHITE);
  tft.drawCentreString("NO", 220, 137, 1);
}

bool procesarToquePopupSiNo(uint16_t touchX, uint16_t touchY) {
  if (!popupSiNoVisible) return false;

  // Botón SÍ
  if (puntoEnArea(touchX, touchY, 60, 130, 80, 30)) {
    ultimoToque = millis();
    popupSiNoVisible = false;

    switch (accionPendiente) {
      case ELIMINAR_TIEMPO:
        if (pantallaActual == 1) {
          eliminarUltimaSolve();
        } else if (pantallaActual == 2) {
          eliminarSolve();
        }
        break;
      case ELIMINAR_SESION:
        modificarSesion(ELIMINAR_SESION);
        break;
      case DESARCHIVAR_TIEMPOS:
        desarchivarTiempos();
        break;
      case ARCHIVAR_SESION:
        modificarSesion(ARCHIVAR_SESION);
        break;
      case ARCHIVAR_TIEMPO:
        solvePopup.record.archivado = solvePopup.record.archivado ? 0 : 1;
        uint32_t idxSD = sessionSolves[solvePopup.indiceGlobal].indexSD;
        String pathDat = getCubePath(".dat");
        File f = SD.open(pathDat.c_str(), "r+");
        if (f) {
          f.seek((size_t)idxSD * sizeof(SolveRecord));
          f.write((const uint8_t*)&solvePopup.record, sizeof(SolveRecord));
          f.close();
        }
        loadSession();
        cerrarPopupSolve();
        break;
    }

    // Redibujar la pantalla que corresponda
    if (pantallaActual == 1) {
      drawTimer();
      imprimirAlgoritmo(mezcla);
      mostrarTiempo(tiempoTranscurrido);
      mezclaShown = false;
      averagesShown = false;
    } else if (pantallaActual == 2) {
      popupSolveVisible = false;
      drawTimes();
    }
    return true;
  }

  // Botón NO
  if (puntoEnArea(touchX, touchY, 180, 130, 80, 30)) {
    ultimoToque = millis();
    popupSiNoVisible = false;

    if (pantallaActual == 1) {
      drawTimer();
      imprimirAlgoritmo(mezcla);
      mostrarTiempo(tiempoTranscurrido);
      mezclaShown = false;
      averagesShown = false;
    } else if (pantallaActual == 2) {
      if (popupSolveVisible) {
        drawPopupSolve();
      } else {
        drawTimes();
      }
    }
    return true;
  }

  return true; // Si está visible pero se pulsa fuera, bloquea otros toques
}

void abrirPopupNum() {
  popupNumVisible = true;

  tft.fillRect(40, 40, 240, 160, TFT_BLACK);
  tft.drawRect(40, 40, 240, 160, TFT_WHITE);

  tft.drawFastHLine(40, 80, 240, TFT_WHITE);
  tft.drawFastHLine(40, 120, 240, TFT_WHITE);
  tft.drawFastHLine(40, 160, 240, TFT_WHITE);
  tft.drawFastVLine(100, 80, 120, TFT_WHITE);
  tft.drawFastVLine(160, 80, 120, TFT_WHITE);
  tft.drawFastVLine(220, 80, 120, TFT_WHITE);

  tft.setTextSize(2);
  tft.drawRect(253, 45, 22, 22, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("X", 265, 49, 1);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.drawString("Desarchivar:", 45, 53);
  tft.drawString("(max. 20k)", 45, 62);
  tft.setTextSize(2);
  tft.drawString("1", 65, 92);
  tft.drawString("2", 125, 92);
  tft.drawString("3", 185, 92);
  tft.drawString("4", 65, 132);
  tft.drawString("5", 125, 132);
  tft.drawString("6", 185, 132); // jaja
  tft.drawString("7", 65, 172); // sixseven
  tft.drawString("8", 125, 172);
  tft.drawString("9", 185, 172);
  tft.drawString("0", 245, 132);
  tft.drawString("<-", 237, 92);
  tft.drawString("OK", 238, 172);
}

void procesarToquePopupNum(uint16_t touchX, uint16_t touchY) {
  if (!popupNumVisible) return;

  static String bufferTeclado = "0";
  char digito = '\0';

  if (puntoEnArea(touchX, touchY, 40, 80, 60, 40)) digito = '1';
  else if (puntoEnArea(touchX, touchY, 100, 80, 60, 40)) digito = '2';
  else if (puntoEnArea(touchX, touchY, 160, 80, 60, 40)) digito = '3';
  else if (puntoEnArea(touchX, touchY, 40, 120, 60, 40)) digito = '4';
  else if (puntoEnArea(touchX, touchY, 100, 120, 60, 40)) digito = '5';
  else if (puntoEnArea(touchX, touchY, 160, 120, 60, 40)) digito = '6';
  else if (puntoEnArea(touchX, touchY, 40, 160, 60, 40)) digito = '7';
  else if (puntoEnArea(touchX, touchY, 100, 160, 60, 40)) digito = '8';
  else if (puntoEnArea(touchX, touchY, 160, 160, 60, 40)) digito = '9';
  else if (puntoEnArea(touchX, touchY, 220, 120, 60, 40)) digito = '0';
  // Retroceso
  else if (puntoEnArea(touchX, touchY, 220, 80, 60, 40)) {
    ultimoToque = millis();
    if (bufferTeclado.length() > 0) {
      bufferTeclado.remove(bufferTeclado.length() - 1);
    }
  }
  // OK
  else if (puntoEnArea(touchX, touchY, 220, 160, 60, 40)) {
    parametroN = bufferTeclado.toInt() < 20000 ? bufferTeclado.toInt() : 20000;
    bufferTeclado = "";
    popupNumVisible = false;
    drawTimes();
    abrirPopupSiNo(DESARCHIVAR_TIEMPOS, parametroN);
    return;
  }
  // cerrar
  else if (puntoEnArea(touchX, touchY, 253, 45, 22, 22)) {
    ultimoToque = millis();
    bufferTeclado = "";
    popupNumVisible = false;
    drawTimes();
    return;
  }
  else {
    return;
  }

  if (digito != '\0') {
    ultimoToque = millis();
    if (bufferTeclado == "0") bufferTeclado = "";
    if (bufferTeclado.length() < 5) {
      bufferTeclado += digito;
    }
  }

  tft.fillRect(160, 41, 89, 38, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int bufferText = 0;
  if (bufferTeclado.length() == 0) bufferText = tft.textWidth(" ", 1);
  else bufferText = tft.textWidth(" ", 1) * bufferTeclado.length();
  tft.drawString(bufferTeclado.length() > 0 ? bufferTeclado : "0", 245 - bufferText, 52);
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
  tft.fillRect(1, 26, 318, 139, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

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

void registrarTiempo(long ms) {
  if (!sdDisponible) return;

  String pathDat = getCubePath(".dat");
  File fileDatCheck = SD.open(pathDat.c_str(), FILE_READ);
  uint32_t nuevoIndexSD = 0;
  if (fileDatCheck) {
    nuevoIndexSD = fileDatCheck.size() / sizeof(SolveRecord);
    fileDatCheck.close();
  }

  addToSession((int32_t)ms, 0, nuevoIndexSD);

  String pathTxt = getCubePath(".txt");
  File fileTxt = SD.open(pathTxt.c_str(), FILE_APPEND);
  if (!fileTxt) return;
  
  uint32_t offsetMezcla = fileTxt.size();
  uint32_t longMezcla = ultimaMezcla.length();
  fileTxt.print(ultimaMezcla);
  fileTxt.close();

  SolveRecord solve = { 
    (int32_t)ms, 
    0, // archivado
    0, // penalty
    offsetMezcla, 
    longMezcla 
  };
  ultimaSolve = solve;

  File fileDat = SD.open(pathDat.c_str(), FILE_APPEND);
  if (!fileDat) return;
  fileDat.write((const uint8_t*)&solve, sizeof(SolveRecord));
  fileDat.close();
}

int32_t calcularAO(int n) {
  if (sessionSolves.size() < (size_t)n) return -1;

  std::vector<long> ventana;
  ventana.reserve(n);

  int dnfs = 0;
  for (auto it = sessionSolves.end() - n; it != sessionSolves.end(); ++it) {
    long t = getTiempoEfectivo(it->tiempo, it->penalty);
    if (t < 0) {
      dnfs++;
      ventana.push_back(2147483647);
    } else {
      ventana.push_back(t);
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

void addToSession(int32_t ms, uint8_t penalty, uint32_t indexSD) {
  if (sessionSolves.size() >= MAX_BUFFER_SOLVES) {
    sessionSolves.pop_front();
  }
  sessionSolves.push_back({ms, penalty, indexSD});
}

String getCubePath(String appendage) {
  return "/" + String(CUBOS[cuboActual].file) + appendage;
}

void actualizarRegistro(uint8_t nuevaPenalty) {
  if (!sdDisponible || sessionSolves.empty()) return;

  ultimaSolve.penalty = nuevaPenalty;
  sessionSolves.back().penalty = nuevaPenalty;

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
  file.write((const uint8_t*)&ultimaSolve, sizeof(SolveRecord));
  file.close();
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
  ultimaSolve = {};
  mostrarTiempo(0);
}

void eliminarSolve() {
  if (!sdDisponible || sessionSolves.empty()) return;

  uint8_t nuevoEstado = 2;
  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), "r+");
  if (!fileDat) return;
  fileDat.seek(sessionSolves[solvePopup.indiceGlobal].indexSD * sizeof(SolveRecord) + sizeof(int32_t));
  fileDat.write(&nuevoEstado, sizeof(uint8_t));
  fileDat.close();
  loadSession();
  cerrarPopupSolve();
}

void modificarSesion(int type) {
  if (!sdDisponible || sessionSolves.empty()) return;

  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), "r+");
  if (!fileDat) return;

  int estado = -1;
  switch (type) {
    case ARCHIVAR_SESION:
      estado = 1;
      break;
    case ELIMINAR_SESION:
      estado = 2;
      break;
  }
  for (int i = 0; i < sessionSolves.size(); i++) {
    size_t offsetArchivado = (size_t)sessionSolves[i].indexSD * sizeof(SolveRecord) + sizeof(int32_t);
    fileDat.seek(offsetArchivado);
    fileDat.write((const uint8_t*)&estado, sizeof(uint8_t));
  }
  fileDat.close();
  sessionSolves.clear();
  ultimaSolve = {};
  paginaSolves = 0;
  tiempoTranscurrido = 0;
  drawTimes();
}

void desarchivarTiempos() {
  if (parametroN <= 0 || !sdDisponible) return;

  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), "r+");
  if (!fileDat) return;

  size_t totalRecords = fileDat.size() / sizeof(SolveRecord);
  if (totalRecords == 0) {
    fileDat.close();
    return;
  }

  int desarchivados = 0;
  int pos = totalRecords - 1;
  uint8_t estado = 0;
  uint8_t nuevoEstado = 0;

  while (desarchivados < parametroN && pos >= 0) {
    size_t offsetArchivado = (size_t)pos * sizeof(SolveRecord) + sizeof(int32_t);
    
    fileDat.seek(offsetArchivado);
    fileDat.read(&estado, sizeof(uint8_t));

    if (estado == 1) {
      fileDat.seek(offsetArchivado);
      fileDat.write(&nuevoEstado, sizeof(uint8_t));
      desarchivados++;
    }
    pos--;
  }
  fileDat.close();

  loadSession();
  drawTimes();
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

  uint32_t registroIdx = 0;
  for (size_t i = 0; i < totalRecords; i += BUFFER_RECORDS) {
    size_t aLeer = std::min((size_t)BUFFER_RECORDS, totalRecords - i);
    file.read((uint8_t*)bloque, aLeer * sizeof(SolveRecord));

    for (size_t j = 0; j < aLeer; j++) {
      if (bloque[j].archivado == 0) {
        addToSession(bloque[j].tiempo, bloque[j].penalty, registroIdx);
      }
      registroIdx++;
    }
  }
  file.close();
}