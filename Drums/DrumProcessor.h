#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "DrumGenerator.h"
#include "DrumKit.h"
#include "GenHistory.h"

class DarkTrapDrumsProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::Timer
{
public:
    enum Part { Kick = 0, Snare = 1, Hats = 2, Bass = 3, OpenHat = 4, Fills = 5, Perc = 6, Fx = 7, All = 8 };

    DarkTrapDrumsProcessor();
    ~DarkTrapDrumsProcessor() override;

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

    void regenerate();
    void randomizeSeed();
    GenHistory history;
    DrumResult getPatternCopy (double& loopBeats);
    int getPatternVersion() const { return patternVersion.load(); }
    double getPlayheadBeats() const { return playheadBeats.load(); }
    int getFollowedRoot() const { return followedRoot.load(); }
    juce::String getPatternName() const;
    bool writeMidiFile (const juce::File& file, Part part);
    juce::File writeTempMidiFile (Part part);

    std::atomic<bool> previewPlaying { false };

private:
    void parameterChanged (const juce::String&, float) override { needsRegen = true; }
    void timerCallback() override;
    DrumParams readParams() const;

    struct Ev { double ppq; bool on; int pitch; int vel; int ch; };

    juce::SpinLock lock;
    DrumResult pattern;
    std::vector<Ev> events;
    double loopBeats = 16.0;

    std::atomic<bool> needsRegen { false }, patternChanged { false };
    std::atomic<int> patternVersion { 0 }, followedRoot { -1 };
    std::atomic<double> playheadBeats { -1.0 };

    double sampleRate = 44100.0, internalPpq = 0.0, expectedPpq = 0.0;
    bool wasRunning = false;
    bool active[8][128] {};
    bool heldInput[128] {};
    int bassSounding = -1;

    DrumKit kit;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkTrapDrumsProcessor)
};
