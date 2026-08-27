#include "TwoScrambler.h"
#include <vector>

using namespace std;

#define SCRAMBLE_MOVES 11
#define MAX_DIFF_MOVES 9
#define PERMITED_MOVES 6

static const string moves[MAX_DIFF_MOVES] = {
    "R", "U", "F",
    "R'", "U'", "F'",
    "R2", "U2", "F2"
};
static std::vector<std::string> tempMoves;
static int selection, numMoves;
static string lastMove;

static void getPossibleMoves();

string TwoScrambler::scramble() {
    string scramble = "";
    numMoves = SCRAMBLE_MOVES;
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
    if (lastMove == "R" || lastMove == "R'" || lastMove == "R2") {
        tempMoves = {
            "U", "F",
            "U'", "F'",
            "U2", "F2"
        };
        return;
    }

    if (lastMove == "U" || lastMove == "U'" || lastMove == "U2") {
        tempMoves = {
            "R", "F",
            "R'", "F'",
            "R2", "F2"
        };
        return;
    }

    if (lastMove == "F" || lastMove == "F'" || lastMove == "F2") {
        tempMoves = {
            "R", "U",
            "R'", "U'",
            "R2", "U2"
        };
        return;
    }
}

