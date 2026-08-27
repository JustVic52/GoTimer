#include <SPI.h>
#include <TFT_eSPI.h>
#include <iostream>
#include <ctime>
#include <Arduino.h>
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

#define TFT_BL_PIN 27
#define SENSOR_PIN 35
#define UMBRAL_PRESION 500
#define BUTTON_W 50
#define BUTTON_H 25
#define BUTTON_CUBE_W 80
#define BUTTON_SUP_Y 0
#define BUTTON_CUBE_X 0
#define BUTTON_SUP1_X 80
#define BUTTON_SUP2_X 130
#define BUTTON_SUP3_X 180
#define CUBE_SELECT1_X 10
#define CUBE_SELECT2_X 80
#define CUBE_SELECT3_X 150
#define CUBE_SELECT4_X 220
#define CUBE_SELECT1_Y 38
#define CUBE_SELECT2_Y 108
#define CUBE_SELECT3_Y 178
#define TIMER_ACTION1_X 10
#define TIMER_ACTION2_X 75
#define TIMER_ACTION3_X 112
#define TIMER_ACTION4_X 150
#define TIMER_ACTION5_X 186
#define TIMER_ACTION6_X 222
#define TIMER_ACTION7_X 256
#define TIMER_ACTION1_Y 175
#define TIMER_ACTION2_Y 205
#define TIMER_ACTION3_Y 206
#define PAGINA_BTN_X 2
#define PAGINA_BTN_Y 30
#define PAGINA_BTN_W 317
#define PAGINA_BTN_H 138
#define FLECHA_X 280
#define FLECHA_Y 218
#define FLECHA_WH 30

TFT_eSPI tft = TFT_eSPI();
enum EstadoTimer { DETENIDO, ESPERANDO, PREPARADO, CORRIENDO };
EstadoTimer estado = DETENIDO;

unsigned long tiempoMano = 0;
unsigned long tiempoInicio = 0;
unsigned long tiempoTranscurrido = 0;
unsigned long ultimoToque = 0;
int pantallaActual = 1;
String mezcla = "";
String selectedCube = " 3x3 ";
int paginaScramble = 0;
int paginaCubos = 1;

void setup() {
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH); 
    
    tft.init();
    //tft.invertDisplay(true);
    tft.setRotation(1); // Modo horizontal (320x240)
    uint16_t calData[5] = { 229, 3433, 369, 3383, 1 };
    //uint16_t calData[5] = { 210, 3456, 371, 3387, 7 }; esto es con tft.setRotation(3);
    tft.setTouch(calData);
    tft.setSwapBytes(true);
    //tft.invertDisplay(true);
    
    analogReadResolution(12);

    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    std::srand(std::time(nullptr));

    //uint16_t calData[5];
    //tft.fillScreen(TFT_BLACK);
    //tft.setTextColor(TFT_WHITE, TFT_BLACK);
    //tft.setTextSize(2);
    //tft.setCursor(20, 100);
    //tft.println("Toca las esquinas");

    // Ejecuta la calibración en pantalla
    //tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);

    //tft.printf("{ %u, %u, %u, %u, %u }\n", 
    //              calData[0], calData[1], calData[2], calData[3], calData[4]);

    //tft.setTouch(calData);

    drawBasic();
    drawTimer();
    mezcla = generarMezcla();
    imprimirAlgoritmo(mezcla);
    mostrarTiempo(0);

}

void loop() {
  uint16_t touchX = 0, touchY = 0;

  if (tft.getTouch(&touchX, &touchY) && estado == DETENIDO) {

    if (millis() - ultimoToque > 300) {

      if (touchX >= BUTTON_CUBE_X && touchX <= (BUTTON_CUBE_X + BUTTON_CUBE_W) &&
          touchY >= BUTTON_SUP_Y && touchY <= (BUTTON_SUP_Y + BUTTON_H)) {

        ultimoToque = millis();

        if (pantallaActual != 0) {
          pantallaActual = 0;
          drawCubes();
        }
      }
      if (touchX >= BUTTON_SUP1_X && touchX <= (BUTTON_SUP1_X + BUTTON_W) &&
          touchY >= BUTTON_SUP_Y && touchY <= (BUTTON_SUP_Y + BUTTON_H)) {

        ultimoToque = millis();

        if (pantallaActual != 1) {
          pantallaActual = 1;
          drawTimer();
          if (mezcla == "") {
            mezcla = generarMezcla();
          }
          imprimirAlgoritmo(mezcla);
          mostrarTiempo(0);
        }
      }
      if (touchX >= BUTTON_SUP2_X && touchX <= (BUTTON_SUP2_X + BUTTON_W) &&
        touchY >= BUTTON_SUP_Y && touchY <= (BUTTON_SUP_Y + BUTTON_H)) {

        ultimoToque = millis();

        if (pantallaActual != 2) {
          pantallaActual = 2;
          drawTimes();
        }
      }
      if (touchX >= BUTTON_SUP3_X && touchX <= (BUTTON_SUP3_X + BUTTON_W) &&
        touchY >= BUTTON_SUP_Y && touchY <= (BUTTON_SUP_Y + BUTTON_H)) {

        ultimoToque = millis();

        if (pantallaActual != 3) {
          pantallaActual = 3;
          drawStats();
        }
      }
    }
  }

  switch (pantallaActual) {
    case 0:
      if (tft.getTouch(&touchX, &touchY) && estado == DETENIDO) {
        if (millis() - ultimoToque > 300) {
          
          // Pulsación sobre el indicador de página
          if (touchX >= FLECHA_X && touchX <= (FLECHA_X + FLECHA_WH) &&
              touchY >= FLECHA_Y && touchY <= (FLECHA_Y + FLECHA_WH)) {
            ultimoToque = millis();
            paginaCubos = (paginaCubos == 1) ? 2 : 1;
            drawCubes();
            break;
          }

          bool cuboSeleccionado = false;

          if (paginaCubos == 1) {
            if (touchX >= CUBE_SELECT1_X && touchX <= (CUBE_SELECT1_X + bigIconW) && 
                touchY >= CUBE_SELECT1_Y && touchY <= (CUBE_SELECT1_Y + bigIconH)) {
              selectedCube = " 2x2 ";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT2_X && touchX <= (CUBE_SELECT2_X + bigIconW) && 
                     touchY >= CUBE_SELECT1_Y && touchY <= (CUBE_SELECT1_Y + bigIconH)) {
              selectedCube = " 3x3 ";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT3_X && touchX <= (CUBE_SELECT3_X + bigIconW) && 
                     touchY >= CUBE_SELECT1_Y && touchY <= (CUBE_SELECT1_Y + bigIconH)) {
              selectedCube = " 4x4 ";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT4_X && touchX <= (CUBE_SELECT4_X + bigIconW) && 
                     touchY >= CUBE_SELECT1_Y && touchY <= (CUBE_SELECT1_Y + bigIconH)) {
              selectedCube = " 5x5 ";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT1_X && touchX <= (CUBE_SELECT1_X + bigIconW) && 
                     touchY >= CUBE_SELECT2_Y && touchY <= (CUBE_SELECT2_Y + bigIconH)) {
              selectedCube = " 6x6 ";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT2_X && touchX <= (CUBE_SELECT2_X + bigIconW) && 
                     touchY >= CUBE_SELECT2_Y && touchY <= (CUBE_SELECT2_Y + bigIconH)) {
              selectedCube = " 7x7 ";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT3_X && touchX <= (CUBE_SELECT3_X + bigIconW) && 
                     touchY >= CUBE_SELECT2_Y && touchY <= (CUBE_SELECT2_Y + bigIconH)) {
              selectedCube = "blind";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT4_X && touchX <= (CUBE_SELECT4_X + bigIconW) && 
                     touchY >= CUBE_SELECT2_Y && touchY <= (CUBE_SELECT2_Y + bigIconH)) {
              selectedCube = " sq1 ";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT1_X && touchX <= (CUBE_SELECT1_X + bigIconW) && 
                     touchY >= CUBE_SELECT3_Y && touchY <= (CUBE_SELECT3_Y + bigIconH)) {
              selectedCube = "pyram";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT2_X && touchX <= (CUBE_SELECT2_X + bigIconW) && 
                     touchY >= CUBE_SELECT3_Y && touchY <= (CUBE_SELECT3_Y + bigIconH)) {
              selectedCube = "clock";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT3_X && touchX <= (CUBE_SELECT3_X + bigIconW) && 
                     touchY >= CUBE_SELECT3_Y && touchY <= (CUBE_SELECT3_Y + bigIconH)) {
              selectedCube = "megam";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT4_X && touchX <= (CUBE_SELECT4_X + bigIconW) && 
                     touchY >= CUBE_SELECT3_Y && touchY <= (CUBE_SELECT3_Y + bigIconH)) {
              selectedCube = "skewb";
              cuboSeleccionado = true;
            }
          }
          else if (paginaCubos == 2) {

            if (touchX >= CUBE_SELECT1_X && touchX <= (CUBE_SELECT1_X + bigIconW) && 
                touchY >= CUBE_SELECT1_Y && touchY <= (CUBE_SELECT1_Y + bigIconH)) {
              selectedCube = " 3oh ";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT2_X && touchX <= (CUBE_SELECT2_X + bigIconW) && 
                     touchY >= CUBE_SELECT1_Y && touchY <= (CUBE_SELECT1_Y + bigIconH)) {
              selectedCube = "4blnd";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT3_X && touchX <= (CUBE_SELECT3_X + bigIconW) && 
                     touchY >= CUBE_SELECT1_Y && touchY <= (CUBE_SELECT1_Y + bigIconH)) {
              selectedCube = "5blnd";
              cuboSeleccionado = true;
            }
            else if (touchX >= CUBE_SELECT4_X && touchX <= (CUBE_SELECT4_X + bigIconW) && 
                     touchY >= CUBE_SELECT1_Y && touchY <= (CUBE_SELECT1_Y + bigIconH)) {
              selectedCube = " fto ";
              cuboSeleccionado = true;
            }
          }

          if (cuboSeleccionado) {
            ultimoToque = millis();
            pantallaActual = 1;
            paginaCubos = 1;
            drawTimer();
            mezcla = generarMezcla();
            imprimirAlgoritmo(mezcla);
            mostrarTiempo(0);
          }
        }
      }
      break;

    case 1:
      if (tft.getTouch(&touchX, &touchY) && estado == DETENIDO) {
        if (millis() - ultimoToque > 300) {
          if ((selectedCube == " 6x6 " || selectedCube == " 7x7 " || selectedCube == "megam") &&
              touchX >= PAGINA_BTN_X && touchX <= (PAGINA_BTN_X + PAGINA_BTN_W) &&
              touchY >= PAGINA_BTN_Y && touchY <= (PAGINA_BTN_Y + PAGINA_BTN_H)) {
            
            ultimoToque = millis();
            paginaScramble = (paginaScramble == 0) ? 1 : 0;
            imprimirAlgoritmo(mezcla);
          }
        }
        if (touchX >= TIMER_ACTION1_X && touchX <= (TIMER_ACTION1_X + bigIconW) && 
            touchY >= TIMER_ACTION1_Y && touchY <= (TIMER_ACTION1_Y + bigIconH)) {
          //TODO drawMezcla();
        }
        if (touchX >= TIMER_ACTION2_X && touchX <= (TIMER_ACTION2_X + iconW) && 
            touchY >= TIMER_ACTION2_Y && touchY <= (TIMER_ACTION2_Y + iconH)) {
          //TODO drawEliminar();
        }
        if (touchX >= TIMER_ACTION3_X && touchX <= (TIMER_ACTION3_X + iconW) && 
            touchY >= TIMER_ACTION2_Y && touchY <= (TIMER_ACTION2_Y + iconH)) {
          mostrarTiempo(0);
        }
        if (touchX >= TIMER_ACTION4_X && touchX <= (TIMER_ACTION4_X + iconW) && 
            touchY >= TIMER_ACTION3_Y && touchY <= (TIMER_ACTION3_Y + iconH)) {
          //TODO if (tiempoTranscurrido > 0) mostrarTiempo(-1); -> impmrimir DNF en rojo
        }
        if (touchX >= TIMER_ACTION5_X && touchX <= (TIMER_ACTION5_X + iconW) && 
            touchY >= TIMER_ACTION2_Y && touchY <= (TIMER_ACTION2_Y + iconH)) {
          if (tiempoTranscurrido > 0) mostrarTiempo(tiempoTranscurrido + 2000);
        }
        if (touchX >= TIMER_ACTION6_X && touchX <= (TIMER_ACTION6_X + iconW) && 
            touchY >= TIMER_ACTION3_Y && touchY <= (TIMER_ACTION3_Y + iconH)) {
          mezcla = generarMezcla();
          imprimirAlgoritmo(mezcla);
        }
        if (touchX >= TIMER_ACTION7_X && touchX <= (TIMER_ACTION7_X + iconW) && 
          touchY >= TIMER_ACTION1_Y && touchY <= (TIMER_ACTION1_Y + iconH)) {
          //TODO drawAverages();
        }
      }
      break;
  }

  int lecturaRaw = analogRead(SENSOR_PIN);
  bool tocado = lecturaRaw > UMBRAL_PRESION;

  switch (estado) {
    case DETENIDO:
      if (tocado) {
        tiempoMano = millis();
        tft.fillCircle(245, 177, 10, TFT_RED);
        estado = ESPERANDO;
      }
      break;

    case ESPERANDO:
      if (!tocado) {
        tft.fillCircle(245, 177, 10, TFT_BLACK);
        estado = DETENIDO;
      }
      else if (millis() - tiempoMano >= 500) {
        tft.fillCircle(245, 177, 10, TFT_GREEN);
        estado = PREPARADO;
      }
      break;

    case PREPARADO:
      if (!tocado) {
        tiempoInicio = millis();
        tft.fillCircle(245, 177, 10, TFT_BLACK);
        estado = CORRIENDO;
      }
      break;

    case CORRIENDO:
      tiempoTranscurrido = millis() - tiempoInicio;
      mostrarTiempo(tiempoTranscurrido);

      if (tocado && !(millis() - tiempoInicio <= 200)) {
        estado = DETENIDO;
        mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);
        mostrarTiempo(tiempoTranscurrido);
        delay(500);
      }
      break;
  }
}

void drawTimes() {
    tft.drawFastHLine(0,25,320,TFT_WHITE);
    tft.fillRect(131, 25, 49, 5, TFT_BLACK);
    tft.fillRect(1, 26, 318, 210, TFT_BLACK);

}

void drawStats() {
    tft.drawFastHLine(0,25,320,TFT_WHITE);
    tft.fillRect(181, 25, 49, 5, TFT_BLACK);
    tft.fillRect(1, 26, 318, 210, TFT_BLACK);

}

void drawTimer() {
    tft.drawFastHLine(0,25,320,TFT_WHITE);
    tft.fillRect(81, 25, 49, 5, TFT_BLACK);
    tft.fillRect(1, 26, 318, 210, TFT_BLACK);
    //Aquí meto todos los mini logos
    tft.fillRect(1, 1, 79, 23, TFT_BLACK);
    tft.setCursor(12,6);
    tft.print(selectedCube);

    //separadores inferiores
    tft.drawFastHLine(0,165,320,TFT_WHITE);

    //imágenes
    tft.pushImage(TIMER_ACTION1_X, TIMER_ACTION1_Y, bigIconW, bigIconH, vermezclaD);
    tft.pushImage(TIMER_ACTION2_X, TIMER_ACTION2_Y, iconW, iconH, eliminarD);
    tft.pushImage(TIMER_ACTION3_X, TIMER_ACTION2_Y, iconW, iconH, againD);
    tft.pushImage(TIMER_ACTION4_X, TIMER_ACTION3_Y, iconW, iconH, dnfD);
    tft.pushImage(TIMER_ACTION5_X, TIMER_ACTION2_Y, iconW, iconH, plustwoD);
    tft.pushImage(TIMER_ACTION6_X, TIMER_ACTION3_Y, iconW, iconH, nuevamezclaD);
    tft.pushImage(TIMER_ACTION7_X, TIMER_ACTION1_Y, bigIconW, bigIconH, bigsolvesD);
}

void drawCubes() {
    tft.drawFastHLine(0,25,320,TFT_WHITE);
    tft.fillRect(1, 25, 79, 5, TFT_BLACK);
    tft.fillRect(1, 26, 318, 210, TFT_BLACK);

    //minilogos
    if (paginaCubos == 1) {
      tft.pushImage(CUBE_SELECT1_X, CUBE_SELECT1_Y, bigIconW, bigIconH, twobuttonD);
      tft.pushImage(CUBE_SELECT2_X, CUBE_SELECT1_Y, bigIconW, bigIconH, threebuttonD);
      tft.pushImage(CUBE_SELECT3_X, CUBE_SELECT1_Y, bigIconW, bigIconH, fourbuttonD);
      tft.pushImage(CUBE_SELECT4_X, CUBE_SELECT1_Y, bigIconW, bigIconH, fivebuttonD);
      tft.pushImage(CUBE_SELECT1_X, CUBE_SELECT2_Y, bigIconW, bigIconH, sixbuttonD);
      tft.pushImage(CUBE_SELECT2_X, CUBE_SELECT2_Y, bigIconW, bigIconH, sevenbuttonD);
      tft.pushImage(CUBE_SELECT3_X, CUBE_SELECT2_Y, bigIconW, bigIconH, blindbuttonD);
      tft.pushImage(CUBE_SELECT4_X, CUBE_SELECT2_Y, bigIconW, bigIconH, sqonebuttonD);
      tft.pushImage(CUBE_SELECT1_X, CUBE_SELECT3_Y, bigIconW, bigIconH, pyrabuttonD);
      tft.pushImage(CUBE_SELECT2_X, CUBE_SELECT3_Y, bigIconW, bigIconH, clockbuttonD);
      tft.pushImage(CUBE_SELECT3_X, CUBE_SELECT3_Y, bigIconW, bigIconH, megabuttonD);
      tft.pushImage(CUBE_SELECT4_X, CUBE_SELECT3_Y, bigIconW, bigIconH, skewbbuttonD);
    }
    else {
      tft.pushImage(CUBE_SELECT1_X, CUBE_SELECT1_Y, bigIconW, bigIconH, ohbuttonD);
      tft.pushImage(CUBE_SELECT2_X, CUBE_SELECT1_Y, bigIconW, bigIconH, fourbldbuttonD);
      tft.pushImage(CUBE_SELECT3_X, CUBE_SELECT1_Y, bigIconW, bigIconH, fivebldbuttonD);
      tft.pushImage(CUBE_SELECT4_X, CUBE_SELECT1_Y, bigIconW, bigIconH, ftobuttonD);
    }

    String indicador = (paginaCubos == 1) ? "[>]" : "[<]";
    tft.drawString(indicador, FLECHA_X, FLECHA_Y, 1);
}

void drawBasic() {
    tft.setTextSize(2);    
    //Toma niño un cuadrao
    tft.drawFastHLine(0,0,320,TFT_WHITE);
    tft.drawFastHLine(0,239,320,TFT_WHITE);
    tft.drawFastVLine(0,0,240,TFT_WHITE);
    tft.drawFastVLine(319,0,240,TFT_WHITE);

    //separadores superiores
    tft.drawFastHLine(0,25,320,TFT_WHITE);
    tft.drawFastVLine(80,0,25,TFT_WHITE);
    tft.drawFastVLine(130,0,25,TFT_WHITE);
    tft.drawFastVLine(180,0,25,TFT_WHITE);
    tft.drawFastVLine(230,0,25,TFT_WHITE);

    //imágenes
    tft.pushImage(92, 1, iconW, iconH, cronoD);
    tft.pushImage(142, 1, iconW, iconH, solvesD);
    tft.pushImage(192, 1, iconW, iconH, estatsD);
}

String generarMezcla() {
  paginaScramble = 0;
  if (selectedCube == " 2x2 ") return TwoScrambler::scramble().c_str();
  else if (selectedCube == " 3x3 ") return ThreeScrambler::scramble().c_str();
  else if (selectedCube == " 4x4 ") return FourScrambler::scramble().c_str();
  else if (selectedCube == " 5x5 ") return FiveScrambler::scramble().c_str();
  else if (selectedCube == " 6x6 ") return SixSevenScrambler::scramble(80).c_str();
  else if (selectedCube == " 7x7 ") return SixSevenScrambler::scramble(100).c_str();
  else if (selectedCube == "blind") return ThreeScrambler::scramble().c_str();
  else if (selectedCube == " sq1 ") return SqoneScrambler::scramble().c_str();
  else if (selectedCube == "pyram") return PyraScrambler::scramble().c_str();
  else if (selectedCube == "clock") return ClockScrambler::scramble().c_str();
  else if (selectedCube == "megam") return MegaScrambler::scramble().c_str();
  else if (selectedCube == "skewb") return SkewbScrambler::scramble().c_str();
  else if (selectedCube == " 3oh ") return ThreeScrambler::scramble().c_str(); 
  else if (selectedCube == " fto ") return FtoScrambler::scramble().c_str();
  else if (selectedCube == "4blnd") return FourScrambler::scramble().c_str();
  else if (selectedCube == "5blnd") return FiveScrambler::scramble().c_str();
}

void mostrarTiempo(unsigned long ms) {
  unsigned long minutos = ms / 60000;
  unsigned long segundos = (ms % 60000) / 1000;
  unsigned long centesimas = (ms % 1000) / 10;

  char buffer[16];
  if (minutos > 0) {
    sprintf(buffer, "%lu:%02lu.%02lu", minutos, segundos, centesimas);
  } else {
    sprintf(buffer, "%lu.%02lu", segundos, centesimas);
  }

  tft.setTextSize(3);
  tft.drawCentreString(buffer, 158, 175, 1);
  tft.setTextSize(2); 
}

void imprimirAlgoritmo(String algoritmo) {
  tft.fillRect(1, 26, 316, 137, TFT_BLACK);
  tft.setTextFont(1);
  int x = 10;
  int y = 30;
  int fuente = 1;
  int cursorX = x;
  int cursorY = y;
  int anchoMax = 310;
  int espacioAncho = tft.textWidth(" ", fuente);
  int altoLinea = tft.fontHeight(fuente);

  bool requierePaginacion = (selectedCube == " 6x6 " || selectedCube == " 7x7 " || selectedCube == "megam");

  int movActual = 0;
  String movimiento = "";

  for (unsigned int i = 0; i <= algoritmo.length(); i++) {
    if (i < algoritmo.length() && algoritmo[i] != ' ') {
      movimiento += algoritmo[i];
    } 
    else if (movimiento.length() > 0) {
      movActual++;

      int anchoMovimiento = tft.textWidth(movimiento, fuente);

      if (cursorX + anchoMovimiento > x + anchoMax) {
        cursorX = x;
        cursorY += altoLinea;
      }

      bool colisionIndicador = requierePaginacion && (cursorY >= 135) && (cursorX + anchoMovimiento >= 240);
      bool dentroDePantalla = (cursorY <= 150);

      bool dibujar = (!requierePaginacion || 
                     (paginaScramble == 0 && movActual <= 50) ||
                     (paginaScramble == 1 && movActual > 50)) && !colisionIndicador && dentroDePantalla;

      if (dibujar) {
        tft.setCursor(cursorX, cursorY);
        tft.print(movimiento);
        cursorX += anchoMovimiento + espacioAncho;
      }

      movimiento = "";
    }
  }

  if (requierePaginacion) {
    String number = (paginaScramble == 0) ? "[1/2]" : "[2/2]";
    tft.drawString(number, 250, 145, 1);
  }
}