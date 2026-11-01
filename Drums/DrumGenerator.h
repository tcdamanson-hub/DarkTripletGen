#pragma once
#include <string>
#include <vector>

// Une frappe : temps en noires depuis le debut de la boucle.
// pitch : note MIDI (60 = C5, la note racine d'un canal FL ; les hats/snares "pitches" en sont decales)
struct DrumHit
{
    double start  = 0.0;
    double length = 0.1;
    int pitch     = 60;
    int velocity  = 100;
};

struct DrumParams
{
    int   feel        = 0;     // 0 = triolets, 1 = binaire, 2 = mix
    int   bars        = 4;     // 2, 4 ou 8
    int   root        = 9;     // tonique (0 = C ... 11 = B) pour la 808
    int   scaleIndex  = 5;     // gamme / mode de la 808 (meme liste que Dark Triplet Gen, 5 = Aeolian)
    int   rotation    = 0;     // rotation du mode
    float melo808     = 0.3f;  // 0..1 notes de passage de la 808 dans la gamme
    int   progression = 0;     // 0 = auto (appris), sinon index de la liste
    float hatDensity  = 0.55f; // 0..1
    float rolls       = 0.6f;  // 0..1 rolls de hats
    float hatPitch    = 0.6f;  // 0..1 hats pitches (descentes en tierces mineures)
    float kickDensity = 0.5f;  // 0..1
    float fills       = 0.4f;  // 0..1 relances de snare
    bool  snareOn3    = true;  // snare principal sur le 3e temps (half-time)
    float bounce808   = 0.5f;  // 0..1 envolees d'octave de la 808
    float openHat     = 0.35f; // 0..1 open hats en fin de mesure
    float perc        = 0.3f;  // 0..1 percussions
    float fx          = 0.5f;  // 0..1 impact au debut + riser en fin de boucle
    int   seed        = 1337;
};

struct DrumResult
{
    std::vector<DrumHit> kick, snare, fills, hats, openHats, bass808, perc, fx; // fx : 60 = impact, 62 = riser
    std::vector<int> chordRoots;            // demi-tons (depuis la tonique) de la fondamentale, par mesure
    std::vector<std::string> chordNames;
};

class DrumGenerator
{
public:
    static DrumResult generate (const DrumParams& p);
    static std::vector<std::string> getProgressionNames(); // [0] = "Auto"
};
