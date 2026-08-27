#include "SqoneScrambler.h"
#include <vector>
#include <cstdlib>

using namespace std;

#define MIN_SCRAMBLE_MOVES 13
#define MAX_SCRAMBLE_MOVES 16

static vector<int> upLayer;
static vector<int> downLayer;
static vector<string> tempMoves;
static int selection, numMoves;

static bool permiteSlash(const vector<int>& capa) {
    return (capa[0] != 3) && (capa[6] != 3) && (capa[11] != 2) && (capa[5] != 2);
}

static void rotarCapa(vector<int>& capa, int pasos) {
    int shift = (pasos % 12 + 12) % 12;
    vector<int> temp(12);
    for (int i = 0; i < 12; i++) {
        temp[(i + shift) % 12] = capa[i];
    }
    capa = temp;
}

static vector<int> getValidTurns(const vector<int>& capa) {
    vector<int> validos;
    for (int g = -5; g <= 6; g++) {
        vector<int> copia = capa;
        rotarCapa(copia, g);
        if (permiteSlash(copia)) {
            validos.push_back(g);
        }
    }
    return validos;
}

static void slash() {
    for (int i = 0; i < 6; i++) {
        int temp = upLayer[i];
        upLayer[i] = downLayer[i];
        downLayer[i] = temp;
    }
}

static void getPossibleMoves() {
    tempMoves.clear();
    vector<int> girosU = getValidTurns(upLayer);
    vector<int> girosD = getValidTurns(downLayer);

    for (int u : girosU) {
        for (int d : girosD) {
            if (u == 0 && d == 0) continue;
            tempMoves.push_back("(" + to_string(u) + "," + to_string(d) + ")");
        }
    }
}

static void aplicarGiro(const string& movimiento) {
    int u = 0, d = 0;
    sscanf(movimiento.c_str(), "(%d,%d)", &u, &d);
    rotarCapa(upLayer, u);
    rotarCapa(downLayer, d);
    slash();
}

string SqoneScrambler::scramble() {
    upLayer   = {2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1};
    downLayer = {1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3};

    string scramble = "";
    numMoves = rand() % (MAX_SCRAMBLE_MOVES + 1 - MIN_SCRAMBLE_MOVES) + MIN_SCRAMBLE_MOVES;

    for (int i = 0; i < numMoves; i++) {
        getPossibleMoves();
        if (tempMoves.empty()) {
            upLayer   = {2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1};
            downLayer = {1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3};
            scramble = "";
            i = -1;
            continue;
        }
        selection = rand() % tempMoves.size();
        string move = tempMoves[selection];
        aplicarGiro(move);

        if (scramble.empty()) {
            scramble += move + " /";
        } else {
            scramble += " " + move + " /";
        }
    }

    return scramble;
}
