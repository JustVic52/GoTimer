#include "ClockScrambler.h"

using namespace std;

#define SCRAMBLE_MOVES 80

static string lastMove;

static void getPossibleMoves();

string ClockScrambler::scramble() {
    string scramble = "";
    int number = (rand() % 13) - 6;
    bool isWritten = (number != 0);
    if (isWritten) {
        scramble += "UR" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "DR" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "DL" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "UL" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "U" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "R" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "D" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "L" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "ALL" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    scramble += "y2 ";

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "U" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "R" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "D" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "L" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    number = (rand() % 13) - 6;
    isWritten = (number != 0);
    if (isWritten) {
        scramble += "ALL" + std::to_string(abs(number));
        scramble += (number >= 0) ? "+ " : "- ";
    }

    if (scramble.length() <= 10) {
        return ClockScrambler::scramble();
    }

    return scramble;
}
