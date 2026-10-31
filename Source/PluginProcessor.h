#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "MelodyGenerator.h"
#include "DarkSynth.h"

class DarkTripletAudioProcessor : public juce::AudioProcessor,
                                  private juce::AudioProcessorValueTreeState::Listener,
                                  private juce::Timer
{
public:
    enum Layer { Melody = 0, Chords = 1, Both = 2 };

    DarkTripletAudioProcessor();
    ~DarkTripletAudioProcessor() override;

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

    // --- appele depuis l'interface (message thread) ---
    void regenerate();
    void randomizeSeed();
    std::vector<MelodyNote> getMelodyCopy (double& loopBeatsOut);
    std::vector<MelodyNote> getChordsCopy();
    juce::String getChordText();
    int getPatternVersion() const { return patternVersion.load(); }
    double getPlayheadBeats() const { return playheadBeats.load(); }
    juce::String getPatternName() const;
    bool writeMidiFile (const juce::File& file, Layer layer);
    juce::File writeTempMidiFile (Layer layer);

    std::atomic<bool> previewPlaying { false };

private:
    void parameterChanged (const juce::String&, float) override { needsRegen = true; }
    void timerCallback() override;
    GenParams readParams() const;

    struct Ev { double ppq; bool on; int pitch; int vel; int ch; };

    juce::SpinLock patternLock;
    std::vector<MelodyNote> melody, chords;
    juce::String chordText;
    std::vector<Ev> events;
    double loopBeats = 16.0;

    std::atomic<bool> needsRegen { false };
    std::atomic<bool> patternChanged { false };
    std::atomic<int> patternVersion { 0 };
    std::atomic<double> playheadBeats { -1.0 };

    double sampleRate = 44100.0;
    double internalPpq = 0.0;
    double expectedPpq = 0.0;
    bool wasRunning = false;
    bool active[2][128] {};

    DarkSynth synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkTripletAudioProcessor)
};
