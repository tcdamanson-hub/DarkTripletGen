#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class DarkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DarkLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class PianoRoll : public juce::Component
{
public:
    void setNotes (std::vector<MelodyNote> mel, std::vector<MelodyNote> chd, double loop)
    {
        melody = std::move (mel); chords = std::move (chd); loopBeats = loop; repaint();
    }
    void setPlayhead (double b) { if (b != playhead) { playhead = b; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    std::vector<MelodyNote> melody, chords;
    double loopBeats = 16.0, playhead = -1.0;
};

// Zone a faire glisser directement dans la DAW (cree un .mid temporaire)
class MidiDragZone : public juce::Component
{
public:
    MidiDragZone (DarkTripletAudioProcessor& p, DarkTripletAudioProcessor::Layer l, juce::String t)
        : proc (p), layer (l), text (std::move (t)) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
private:
    DarkTripletAudioProcessor& proc;
    DarkTripletAudioProcessor::Layer layer;
    juce::String text;
    bool dragging = false, hover = false;
};

class DarkTripletAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DarkTripletAudioProcessorEditor (DarkTripletAudioProcessor&);
    ~DarkTripletAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void addKnob (const juce::String& id, const juce::String& label);
    void addCombo (const juce::String& id, const juce::String& label, int width);
    void exportMidi();

    DarkLookAndFeel lnf;
    DarkTripletAudioProcessor& proc;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        int width = 100;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Combo>> combos;

    juce::TextButton generateBtn { "GENERER" }, previewBtn { "PREVIEW" }, exportBtn { "EXPORT .MID" };
    juce::ToggleButton synthToggle { "Synth" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> synthAtt;
    MidiDragZone dragMelo, dragChords;
    PianoRoll roll;
    juce::Label chordLabel, info;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkTripletAudioProcessorEditor)
};
