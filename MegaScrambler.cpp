#include "MegaScrambler.h"

using namespace std;

#define SCRAMBLE_MOVES 80

static string lastMove;

static void getPossibleMoves();

string MegaScrambler::scramble() {
    string scramble = "";
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            scramble += j%2==0 ? "R" : "D";
            scramble += rand()%2==0 ? "++ " : "-- ";
        }
        scramble += rand()%2==0 ? "U   " : "U'  ";
    }

    return scramble;
}
