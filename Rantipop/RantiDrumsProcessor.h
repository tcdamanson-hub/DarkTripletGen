#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "RantiEngine.h"

// RANTIPOP DRUMS : tes grooves MIDI joues tels quels avec tes kits (R&B / Afro / Synthwave)
class RantiDrumsProcessor : public juce::AudioProcessor
{
public:
    RantiDrumsProcessor();
    ~RantiDrumsProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // --- interface ---
    void generate();                         // groove + kit au hasard
    void newKit();                           // kit au hasard, meme groove
    bool canGoBack() const    { return histPos > 0; }
    bool canGoForward() const { return histPos + 1 < (int) history.size(); }
    void goBack();
    void goForward();
    void setGenre (int g);
    void stepGroove (int dir);
    void setGroove (int g);
    void setSample (int lane, int index);
    ranti::GenState getState() const;
    std::vector<ranti::PNote> getHits();
    double getLength() const { return length.load(); }
    int getVersion() const { return version.load(); }
    double getPlayhead() const { return playhead.load(); }
    float getMeter() const { return meter.load(); }
    juce::uint32 getLaneHit (int lane) const { return laneHit[(size_t) lane].load(); }
    void audition (int lane) { pendingPad = lane; }
    juce::File writeTempMidi();

    std::atomic<bool> previewPlaying { false };

    static const char* laneParam (int lane);

private:
    void applyState (const ranti::GenState& g, bool pushHistory);

    juce::SpinLock lock;
    ranti::GenState state;
    std::vector<ranti::PNote> hits;
    std::vector<ranti::GenState> history;
    int histPos = -1;
    juce::Random rng;

    std::atomic<double> length { 16.0 };
    std::atomic<int> version { 0 }, pendingPad { -1 };
    std::atomic<double> playhead { -1.0 };
    std::atomic<float> meter { 0.0f };
    std::array<std::atomic<juce::uint32>, ranti::NumLanes> laneHit {};
    juce::uint32 hitCounter = 0;

    double sampleRate = 44100.0, internalPpq = 0.0, expectedPpq = 0.0;
    bool wasRunning = false;
    ranti::Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RantiDrumsProcessor)
};
