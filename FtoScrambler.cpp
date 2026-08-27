#include "FtoScrambler.h"
#include <vector>

using namespace std;

#define MIN_SCRAMBLE_MOVES 25
#define MAX_SCRAMBLE_MOVES 28
#define MAX_DIFF_MOVES 16
#define PERMITED_MOVES 14

static const string moves[MAX_DIFF_MOVES] = {
    "L", "R", "U", "D", "F", "B", "BR", "BL",
    "L'", "R'", "U'", "D'", "F'", "B'", "BR'", "BL'"
};
static std::vector<std::string> tempMoves;
static int selection, numMoves;
static string lastMove;

static void getPossibleMoves();

string FtoScrambler::scramble() {
    string scramble = "";
    numMoves = rand() % (MAX_SCRAMBLE_MOVES + 1 - MIN_SCRAMBLE_MOVES) + MIN_SCRAMBLE_MOVES;
    selection = rand() % MAX_DIFF_MOVES;
    scramble += moves[selection];
    lastMove = moves[selection];
    numMoves--;
    for (int i = 0; i < numMoves; i++) {
        getPossibleMoves();
        selection = rand() % PERMITED_MOVES;
        scramble += " " + tempMoves[selection];
        lastMove = tempMoves[selection];
    }

    return scramble;
}

static void getPossibleMoves() {
    if (lastMove == "L" || lastMove == "L'") {
        tempMoves = {
            "R", "U", "D", "F", "B", "BR", "BL",
            "R'", "U'", "D'", "F'", "B'", "BR'", "BL'"
        };
        return;
    }

    if (lastMove == "R" || lastMove == "R'") {
        tempMoves = {
            "L", "U", "D", "F", "B", "BR", "BL",
            "L'", "U'", "D'", "F'", "B'", "BR'", "BL'"
        };
        return;
    }

    if (lastMove == "U" || lastMove == "U'") {
        tempMoves = {
            "L", "R", "D", "F", "B", "BR", "BL",
            "L'", "R'", "D'", "F'", "B'", "BR'", "BL'"
        };
        return;
    }

    if (lastMove == "D" || lastMove == "D'") {
        tempMoves = {
            "L", "R", "U", "F", "B", "BR", "BL",
            "L'", "R'", "U'", "F'", "B'", "BR'", "BL'"
        };
        return;
    }

    if (lastMove == "F" || lastMove == "F'") {
        tempMoves = {
            "L", "R", "U", "D", "B", "BR", "BL",
            "L'", "R'", "U'", "D'", "B'", "BR'", "BL'"
        };
        return;
    }

    if (lastMove == "B" || lastMove == "B'") {
        tempMoves = {
            "L", "R", "U", "D", "F", "BR", "BL",
            "L'", "R'", "U'", "D'", "F'", "BR'", "BL'",
        };
        return;
    }

    if (lastMove == "BR" || lastMove == "BR'") {
        tempMoves = {
            "L", "R", "U", "D", "F", "B", "BL",
            "L'", "R'", "U'", "D'", "F'", "B'", "BL'"
        };
        return;
    }

    if (lastMove == "BL" || lastMove == "BL'") {
        tempMoves = {
            "L", "R", "U", "D", "F", "B", "BR",
            "L'", "R'", "U'", "D'", "F'", "B'", "BR'"
        };
        return;
    }
}

