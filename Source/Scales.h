#pragma once
#include <vector>

// Toutes les gammes / modes disponibles (intervalles en demi-tons depuis la tonique).
// Le parametre "Mode (rotation)" permet en plus d'obtenir n'importe quel mode de n'importe quelle gamme.
struct ScaleDef
{
    const char* name;
    std::vector<int> intervals;
};

inline const std::vector<ScaleDef>& getScales()
{
    static const std::vector<ScaleDef> scales = {
        // --- Modes de la gamme majeure ---
        { "Ionian (Major)",           { 0, 2, 4, 5, 7, 9, 11 } },
        { "Dorian",                   { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",                 { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",                   { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",               { 0, 2, 4, 5, 7, 9, 10 } },
        { "Aeolian (Natural Minor)",  { 0, 2, 3, 5, 7, 8, 10 } },
        { "Locrian",                  { 0, 1, 3, 5, 6, 8, 10 } },
        // --- Modes du mineur harmonique ---
        { "Harmonic Minor",           { 0, 2, 3, 5, 7, 8, 11 } },
        { "Locrian #6",               { 0, 1, 3, 5, 6, 9, 10 } },
        { "Ionian #5",                { 0, 2, 4, 5, 8, 9, 11 } },
        { "Dorian #4 (Ukrainian)",    { 0, 2, 3, 6, 7, 9, 10 } },
        { "Phrygian Dominant",        { 0, 1, 4, 5, 7, 8, 10 } },
        { "Lydian #2",                { 0, 3, 4, 6, 7, 9, 11 } },
        { "Ultralocrian",             { 0, 1, 3, 4, 6, 8, 9 } },
        // --- Modes du mineur melodique ---
        { "Melodic Minor",            { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorian b2",                { 0, 1, 3, 5, 7, 9, 10 } },
        { "Lydian Augmented",         { 0, 2, 4, 6, 8, 9, 11 } },
        { "Lydian Dominant",          { 0, 2, 4, 6, 7, 9, 10 } },
        { "Mixolydian b6",            { 0, 2, 4, 5, 7, 8, 10 } },
        { "Locrian #2",               { 0, 2, 3, 5, 6, 8, 10 } },
        { "Altered",                  { 0, 1, 3, 4, 6, 8, 10 } },
        // --- Modes du majeur harmonique ---
        { "Harmonic Major",           { 0, 2, 4, 5, 7, 8, 11 } },
        { "Dorian b5",                { 0, 2, 3, 5, 6, 9, 10 } },
        { "Phrygian b4",              { 0, 1, 3, 4, 7, 8, 10 } },
        { "Lydian b3",                { 0, 2, 3, 6, 7, 9, 11 } },
        { "Mixolydian b2",            { 0, 1, 4, 5, 7, 9, 10 } },
        { "Lydian Augmented #2",      { 0, 3, 4, 6, 8, 9, 11 } },
        { "Locrian bb7",              { 0, 1, 3, 5, 6, 8, 9 } },
        // --- Modes du double harmonique ---
        { "Double Harmonic Major",    { 0, 1, 4, 5, 7, 8, 11 } },
        { "Lydian #2 #6",             { 0, 3, 4, 6, 7, 10, 11 } },
        { "Ultraphrygian",            { 0, 1, 3, 4, 7, 8, 9 } },
        { "Hungarian Minor",          { 0, 2, 3, 6, 7, 8, 11 } },
        { "Oriental",                 { 0, 1, 4, 5, 6, 9, 10 } },
        { "Ionian #2 #5",             { 0, 3, 4, 5, 8, 9, 11 } },
        { "Locrian bb3 bb7",          { 0, 1, 2, 5, 6, 8, 9 } },
        // --- Exotiques / autres heptatoniques ---
        { "Neapolitan Minor",         { 0, 1, 3, 5, 7, 8, 11 } },
        { "Neapolitan Major",         { 0, 1, 3, 5, 7, 9, 11 } },
        { "Hungarian Major",          { 0, 3, 4, 6, 7, 9, 10 } },
        { "Persian",                  { 0, 1, 4, 5, 6, 8, 11 } },
        { "Enigmatic",                { 0, 1, 4, 6, 8, 10, 11 } },
        { "Spanish 8-Tone",           { 0, 1, 3, 4, 5, 6, 8, 10 } },
        // --- Pentatoniques & co ---
        { "Minor Pentatonic",         { 0, 3, 5, 7, 10 } },
        { "Major Pentatonic",         { 0, 2, 4, 7, 9 } },
        { "Blues",                    { 0, 3, 5, 6, 7, 10 } },
        { "Hirajoshi",                { 0, 2, 3, 7, 8 } },
        { "In Sen",                   { 0, 1, 5, 7, 10 } },
        { "Iwato",                    { 0, 1, 5, 6, 10 } },
        { "Kumoi",                    { 0, 2, 3, 7, 9 } },
        { "Pelog",                    { 0, 1, 3, 7, 8 } },
        { "Egyptian",                 { 0, 2, 5, 7, 10 } },
        // --- Symetriques ---
        { "Whole Tone",               { 0, 2, 4, 6, 8, 10 } },
        { "Diminished (H-W)",         { 0, 1, 3, 4, 6, 7, 9, 10 } },
        { "Diminished (W-H)",         { 0, 2, 3, 5, 6, 8, 9, 11 } },
        { "Augmented",                { 0, 3, 4, 7, 8, 11 } },
        { "Prometheus",               { 0, 2, 4, 6, 9, 10 } },
        { "Tritone",                  { 0, 1, 4, 6, 7, 10 } },
        { "Chromatic",                { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    return scales;
}

inline int findScaleIndex (const char* name)
{
    const auto& s = getScales();
    for (int i = 0; i < (int) s.size(); ++i)
    {
        const char* a = s[(size_t) i].name;
        const char* b = name;
        while (*a != 0 && *a == *b) { ++a; ++b; }
        if (*a == 0 && *b == 0)
            return i;
    }
    return 0;
}
