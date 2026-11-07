#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "RantiEngine.h"
#include "RantiClock.h"

class RantiProcessor : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    RantiProcessor();
    ~RantiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // --- interface (message thread) ---
    void generate();                 // GENERER
    bool canGoBack() const    { return histPos > 0; }
    bool canGoForward() const { return histPos + 1 < (int) history.size(); }
    void goBack();
    void goForward();
    void setGenre (int g);
    void applyGenreSounds (int genre);
    void setGroove (int grooveIndex);
    void stepSong (int dir);         // MIDI precedent / suivant du genre
    void setKey (int keyChoice);     // 0 = auto
    int  getKeyChoice() const { return keyChoice; }
    void setMode (int modeChoice);   // -1 toutes, -2 majeures, -3 mineures, 0..18 une gamme
    int  getModeChoice() const { return modeChoice; }
    ranti::GenState getState() const;
    ranti::Pattern getPattern();
    int getPatternVersion() const { return patternVersion.load(); }
    double getPlayhead() const { return playhead.load(); }
    float getMeter() const { return meter.load(); }
    juce::File writeTempMidi (int part); // 0..4, 5 = tout
    std::array<bool, 128> getActiveKeys();

    std::atomic<bool> previewPlaying { false };
    void togglePreview()
    {
        ranti::SharedClock::setRunning (! previewPlaying.load());
        clock.poll();
        previewPlaying = clock.running.load();
    }

private:
    void timerCallback() override { clock.poll(); previewPlaying = clock.running.load(); }
    ranti::SharedClock clock;
    void applyState (const ranti::GenState& g, bool pushHistory);

    struct Ev { double beat; bool on; int part, pitch, vel; };

    juce::SpinLock lock;
    ranti::GenState state;
    ranti::Pattern pattern;
    std::vector<Ev> events;
    std::vector<ranti::GenState> history;
    int histPos = -1;
    int keyChoice = 0;
    int modeChoice = -1;
    juce::Random rng;

    std::atomic<bool> patternChanged { false };
    std::atomic<int> patternVersion { 0 };
    std::atomic<double> playhead { -1.0 };
    std::atomic<float> meter { 0.0f };

    double sampleRate = 44100.0, internalPpq = 0.0, expectedPpq = 0.0;
    bool wasRunning = false, previewWasOn = false;
    bool active[ranti::NumParts][128] {};
    std::array<std::atomic<int>, 128> keyCount {};

    ranti::Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RantiProcessor)
};
