#pragma once
#include <string>
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
    int   root         = 9;     // 0 = C ... 11 = B
    int   scaleIndex   = 5;     // Aeolian
    int   rotation     = 0;     // mode de la gamme choisie
    int   octave       = 4;     // octave de la tonique (4 -> C4 = 60)
    int   bars         = 4;
    int   feel         = 0;     // 0 = triolets, 1 = normal (binaire), 2 = mix
    int   progression  = 0;     // 0 = auto, sinon index dans la liste
    int   chordMode    = 1;     // 0 = pas d'accords, 1 = tenus, 2 = rythmes
    int   motif        = 0;     // 0 = appris (tes MIDI), 1 = topline
    int   layer2       = 1;     // 0 = off, 1 = 2e couche
    int   style        = 0;     // 0 = perso, 1 = Ninho, 2 = Werenoi, 3 = Trap dark, 4 = Triolets
    int   bank         = 0;     // banque MIDI apprise : 0 = toutes, 1 = melodies, 2 = guitare, 3 = Nexus, 4 = Melodic (variations)
    float density      = 0.5f;  // 0..1  peu de notes -> beaucoup de notes
    float darkness     = 0.6f;  // 0..1  notes de passage sombres
    float rolls        = 0.3f;  // 0..1  runs / fioritures rapides
    float rests        = 0.2f;  // 0..1  silences dans le motif
    float octaveJumps  = 0.15f; // 0..1  sauts d'octave
    int   rangeOctaves = 2;     // 1..3
    float gate         = 0.95f; // 0.1..1 longueur des notes (legato)
    int   seed         = 1337;
};

struct GenResult
{
    std::vector<MelodyNote> melody;
    std::vector<MelodyNote> chords;      // accords + basse
    std::vector<MelodyNote> counter;     // 2e couche melodique
    std::vector<std::string> chordNames; // un nom par accord de la boucle
};

class MelodyGenerator
{
public:
    static GenResult generate (const GenParams& p);
    static std::vector<int> getScaleIntervals (int scaleIndex, int rotation);
    static std::vector<std::string> getProgressionNames(); // [0] = "Auto"
    static std::vector<std::string> getStyleNames();
    static std::vector<std::string> getBankNames();
};
