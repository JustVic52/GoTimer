#include "FiveScrambler.h"
#include <vector>

using namespace std;

#define SCRAMBLE_MOVES 60
#define MAX_DIFF_MOVES 36
#define PERMITED_MOVES 33

static const string moves[MAX_DIFF_MOVES] = {
    "L", "R", "U", "D", "F", "B",
    "L'", "R'", "U'", "D'", "F'", "B'",
    "L2", "R2", "U2", "D2", "F2", "B2",
    "Lw", "Rw", "Uw", "Dw", "Fw", "Bw",
    "Lw'", "Rw'", "Uw'", "Dw'", "Fw'", "Bw'",
    "Lw2", "Rw2", "Uw2", "Dw2", "Fw2", "Bw2"
};
static std::vector<std::string> tempMoves;
static int selection, numMoves;
static string lastMove;

static void getPossibleMoves();

string FiveScrambler::scramble() {
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
    if (lastMove == "L" || lastMove == "L'" || lastMove == "L2") {
        tempMoves = {
            "R", "U", "D", "F", "B",
            "R'", "U'", "D'", "F'", "B'",
            "R2", "U2", "D2", "F2", "B2",
            "Lw", "Rw", "Uw", "Dw", "Fw", "Bw",
            "Lw'", "Rw'", "Uw'", "Dw'", "Fw'", "Bw'",
            "Lw2", "Rw2", "Uw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "R" || lastMove == "R'" || lastMove == "R2") {
        tempMoves = {
            "L", "U", "D", "F", "B",
            "L'", "U'", "D'", "F'", "B'",
            "L2", "U2", "D2", "F2", "B2",
            "Lw", "Rw", "Uw", "Dw", "Fw", "Bw",
            "Lw'", "Rw'", "Uw'", "Dw'", "Fw'", "Bw'",
            "Lw2", "Rw2", "Uw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "U" || lastMove == "U'" || lastMove == "U2") {
        tempMoves = {
            "L", "R", "D", "F", "B",
            "L'", "R'", "D'", "F'", "B'",
            "L2", "R2", "D2", "F2", "B2",
            "Lw", "Rw", "Uw", "Dw", "Fw", "Bw",
            "Lw'", "Rw'", "Uw'", "Dw'", "Fw'", "Bw'",
            "Lw2", "Rw2", "Uw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "D" || lastMove == "D'" || lastMove == "D2") {
        tempMoves = {
            "L", "R", "U", "F", "B",
            "L'", "R'", "U'", "F'", "B'",
            "L2", "R2", "U2", "F2", "B2",
            "Lw", "Rw", "Uw", "Dw", "Fw", "Bw",
            "Lw'", "Rw'", "Uw'", "Dw'", "Fw'", "Bw'",
            "Lw2", "Rw2", "Uw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "F" || lastMove == "F'" || lastMove == "F2") {
        tempMoves = {
            "L", "R", "U", "D", "B",
            "L'", "R'", "U'", "D'", "B'",
            "L2", "R2", "U2", "D2", "B2",
            "Lw", "Rw", "Uw", "Dw", "Fw", "Bw",
            "Lw'", "Rw'", "Uw'", "Dw'", "Fw'", "Bw'",
            "Lw2", "Rw2", "Uw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "B" || lastMove == "B'" || lastMove == "B2") {
        tempMoves = {
            "L", "R", "U", "D", "F",
            "L'", "R'", "U'", "D'", "F'",
            "L2", "R2", "U2", "D2", "F2",
            "Lw", "Rw", "Uw", "Dw", "Fw", "Bw",
            "Lw'", "Rw'", "Uw'", "Dw'", "Fw'", "Bw'",
            "Lw2", "Rw2", "Uw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "Lw" || lastMove == "Lw'" || lastMove == "Lw2") {
        tempMoves = {
            "L", "R", "U", "D", "F", "B",
            "L'", "R'", "U'", "D'", "F'", "B'",
            "L2", "R2", "U2", "D2", "F2", "B2",
            "Rw", "Uw", "Dw", "Fw", "Bw",
            "Rw'", "Uw'", "Dw'", "Fw'", "Bw'",
            "Rw2", "Uw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "Rw" || lastMove == "Rw'" || lastMove == "Rw2") {
        tempMoves = {
            "L", "R", "U", "D", "F", "B",
            "L'", "R'", "U'", "D'", "F'", "B'",
            "L2", "R2", "U2", "D2", "F2", "B2",
            "Lw", "Uw", "Dw", "Fw", "Bw",
            "Lw'", "Uw'", "Dw'", "Fw'", "Bw'",
            "Lw2", "Uw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "Uw" || lastMove == "Uw'" || lastMove == "Uw2") {
        tempMoves = {
            "L", "R", "U", "D", "F", "B",
            "L'", "R'", "U'", "D'", "F'", "B'",
            "L2", "R2", "U2", "D2", "F2", "B2",
            "Lw", "Rw", "Dw", "Fw", "Bw",
            "Lw'", "Rw'", "Dw'", "Fw'", "Bw'",
            "Lw2", "Rw2", "Dw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "Dw" || lastMove == "Dw'" || lastMove == "Dw2") {
        tempMoves = {
            "L", "R", "U", "D", "F", "B",
            "L'", "R'", "U'", "D'", "F'", "B'",
            "L2", "R2", "U2", "D2", "F2", "B2",
            "Lw", "Rw", "Uw", "Fw", "Bw",
            "Lw'", "Rw'", "Uw'", "Fw'", "Bw'",
            "Lw2", "Rw2", "Uw2", "Fw2", "Bw2"
        };
        return;
    }

    if (lastMove == "Fw" || lastMove == "Fw'" || lastMove == "Fw2") {
        tempMoves = {
            "L", "R", "U", "D", "F", "B",
            "L'", "R'", "U'", "D'", "F'", "B'",
            "L2", "R2", "U2", "D2", "F2", "B2",
            "Lw", "Rw", "Uw", "Dw", "Bw",
            "Lw'", "Rw'", "Uw'", "Dw'", "Bw'",
            "Lw2", "Rw2", "Uw2", "Dw2", "Bw2"
        };
        return;
    }

    if (lastMove == "Bw" || lastMove == "Bw'" || lastMove == "Bw2") {
        tempMoves = {
            "L", "R", "U", "D", "F", "B",
            "L'", "R'", "U'", "D'", "F'", "B'",
            "L2", "R2", "U2", "D2", "F2", "B2",
            "Lw", "Rw", "Uw", "Dw", "Fw",
            "Lw'", "Rw'", "Uw'", "Dw'", "Fw'",
            "Lw2", "Rw2", "Uw2", "Dw2", "Fw2"
        };
        return;
    }
}

