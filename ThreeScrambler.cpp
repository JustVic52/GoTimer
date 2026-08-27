#include "ThreeScrambler.h"
#include <vector>

using namespace std;

#define MIN_SCRAMBLE_MOVES 17
#define MAX_SCRAMBLE_MOVES 22
#define MAX_DIFF_MOVES 18
#define PERMITED_MOVES 15

static const string moves[MAX_DIFF_MOVES] = {
    "L", "R", "U", "D", "F", "B",
    "L'", "R'", "U'", "D'", "F'", "B'",
    "L2", "R2", "U2", "D2", "F2", "B2"
};
static std::vector<std::string> tempMoves;
static int selection, numMoves;
static string lastMove;

static void getPossibleMoves();

string ThreeScrambler::scramble() {
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
    if (lastMove == "L" || lastMove == "L'" || lastMove == "L2") {
        tempMoves = {
            "R", "U", "D", "F", "B",
            "R'", "U'", "D'", "F'", "B'",
            "R2", "U2", "D2", "F2", "B2"
        };
        return;
    }

    if (lastMove == "R" || lastMove == "R'" || lastMove == "R2") {
        tempMoves = {
            "L", "U", "D", "F", "B",
            "L'", "U'", "D'", "F'", "B'",
            "L2", "U2", "D2", "F2", "B2"
        };
        return;
    }

    if (lastMove == "U" || lastMove == "U'" || lastMove == "U2") {
        tempMoves = {
            "L", "R", "D", "F", "B",
            "L'", "R'", "D'", "F'", "B'",
            "L2", "R2", "D2", "F2", "B2"
        };
        return;
    }

    if (lastMove == "D" || lastMove == "D'" || lastMove == "D2") {
        tempMoves = {
            "L", "R", "U", "F", "B",
            "L'", "R'", "U'", "F'", "B'",
            "L2", "R2", "U2", "F2", "B2"
        };
        return;
    }

    if (lastMove == "F" || lastMove == "F'" || lastMove == "F2") {
        tempMoves = {
            "L", "R", "U", "D", "B",
            "L'", "R'", "U'", "D'", "B'",
            "L2", "R2", "U2", "D2", "B2"
        };
        return;
    }

    if (lastMove == "B" || lastMove == "B'" || lastMove == "B2") {
        tempMoves = {
            "L", "R", "U", "D", "F",
            "L'", "R'", "U'", "D'", "F'",
            "L2", "R2", "U2", "D2", "F2"
        };
        return;
    }
}

