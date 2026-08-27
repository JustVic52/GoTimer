#include "PyraScrambler.h"
#include <vector>

using namespace std;

#define SCRAMBLE_MOVES 11
#define MAX_DIFF_MOVES 8
#define PERMITED_MOVES 6
#define MAX_CORNERS 4

static const string moves[MAX_DIFF_MOVES] = {
    "R", "U", "L", "B",
    "R'", "U'", "L'", "B'"
};
static std::vector<std::string> tempMoves;
static int selection, numMoves;
static string lastMove;

static void getPossibleMoves();

string PyraScrambler::scramble() {
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

    numMoves = rand() % MAX_CORNERS + 1;

    switch (numMoves) {
        case 0:
            break;
        case 1:
            selection = rand() % 2;
            if (selection == 0) scramble += " u";
            else scramble += " u'";
            break;
        case 2:
            selection = rand() % 2;
            if (selection == 0) scramble += " u";
            else scramble += " u'";
            selection = rand() % 2;
            if (selection == 0) scramble += " l";
            else scramble += " l'";
            break;
        case 3:
            selection = rand() % 2;
            if (selection == 0) scramble += " u";
            else scramble += " u'";
            selection = rand() % 2;
            if (selection == 0) scramble += " l";
            else scramble += " l'";
            selection = rand() % 2;
            if (selection == 0) scramble += " r";
            else scramble += " r'";
            break;
        case 4:
            selection = rand() % 2;
            if (selection == 0) scramble += " u";
            else scramble += " u'";
            selection = rand() % 2;
            if (selection == 0) scramble += " l";
            else scramble += " l'";
            selection = rand() % 2;
            if (selection == 0) scramble += " r";
            else scramble += " r'";
            selection = rand() % 2;
            if (selection == 0) scramble += " b";
            else scramble += " b'";
            break;
    }

    return scramble;
}

static void getPossibleMoves() {
    if (lastMove == "R" || lastMove == "R'") {
        tempMoves = {
            "U", "L", "B",
            "U'", "L'", "B'"
        };
        return;
    }

    if (lastMove == "U" || lastMove == "U'") {
        tempMoves = {
            "R", "L", "B",
            "R'", "L'", "B'"
        };
        return;
    }

    if (lastMove == "L" || lastMove == "L'") {
        tempMoves = {
            "R", "U", "B",
            "R'", "U'", "B'"
        };
        return;
    }

    if (lastMove == "B" || lastMove == "B'") {
        tempMoves = {
            "R", "U", "L",
            "R'", "U'", "L'"
        };
        return;
    }
}

