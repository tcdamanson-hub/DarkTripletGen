#pragma once
#include <vector>

// Une note generee. Temps en noires (beats), boucle commencant a 0.
struct MelodyNote
{
    double start  = 0.0;
    double length = 0.25;
    int pitch     = 60;
    int velocity  = 100;
};

struct GenParams
{
    int   root         = 1;     // 0 = C ... 11 = B
    int   scaleIndex   = 7;
    int   rotation     = 0;     // mode de la gamme choisie
    int   octave       = 4;     // octave de la tonique (4 -> C4 = 60)
    int   bars         = 4;
    int   feel         = 0;     // 0 = triolets, 1 = normal (binaire), 2 = mix
    float density      = 0.6f;  // 0..1  peu de notes -> beaucoup de notes
    float darkness     = 0.8f;  // 0..1  poids sur b2, b3, b6, triton, tonique, mouvement descendant
    float rolls        = 0.3f;  // 0..1  rolls en triolets de doubles croches
    float rests        = 0.25f; // 0..1  silences
    float octaveJumps  = 0.15f; // 0..1  sauts d'octave
    int   rangeOctaves = 2;     // 1..3
    float gate         = 0.85f; // 0.1..1 longueur des notes
    int   seed         = 1337;
};

class MelodyGenerator
{
public:
    static std::vector<MelodyNote> generate (const GenParams& p);
    static std::vector<int> getScaleIntervals (int scaleIndex, int rotation);
};
