#pragma once
#include <juce_core/juce_core.h>
#include <atomic>

// Horloge partagee entre RANTIPOP MELODIE et RANTIPOP DRUMS pour la previsualisation :
// PREVISUALISER dans l'un lance / arrete les deux, et ils jouent sur le meme temps.
// (Quand FL joue, les deux suivent deja la position de FL.)
namespace ranti
{
class SharedClock
{
public:
    static juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Rantipop").getChildFile ("preview_clock.txt");
    }

    // appele depuis l'interface
    static void setRunning (bool run)
    {
        auto f = file();
        f.getParentDirectory().createDirectory();
        f.replaceWithText (run ? "1 " + juce::String (juce::Time::currentTimeMillis()) : "0 0");
    }

    // appele par un timer (thread des messages) : lit l'etat partage
    void poll()
    {
        // on ne relit le fichier que s'il a change (evite des acces disque continus)
        const auto f = file();
        const auto mod = f.getLastModificationTime().toMilliseconds();
        if (mod == lastMod && ! running.load()) return;
        if (mod == lastMod && juce::Time::currentTimeMillis() - startMs.load() < 3 * 3600 * 1000) return;
        lastMod = mod;
        const auto t = f.loadFileAsString();
        const auto a = juce::StringArray::fromTokens (t, " ", "");
        if (a.size() >= 2)
        {
            startMs = a[1].getLargeIntValue();
            // un etat "en lecture" de plus de 3 h est un reste d'une ancienne session
            running = a[0] == "1" && juce::Time::currentTimeMillis() - startMs.load() < 3 * 3600 * 1000;
        }
        else running = false;
    }

    // position en temps depuis le depart commun (thread audio)
    double beatsNow (double bpm) const
    {
        const double ms = (double) (juce::Time::currentTimeMillis() - startMs.load());
        return juce::jmax (0.0, ms / 60000.0 * bpm);
    }

    std::atomic<bool> running { false };
    std::atomic<juce::int64> startMs { 0 };
    juce::int64 lastMod = -1;
};
} // namespace ranti
