#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DrumProcessor.h"

class DrumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DrumLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override;
};

class DrumGrid : public juce::Component
{
public:
    void setPattern (DrumResult r, double loop) { res = std::move (r); loopBeats = loop; repaint(); }
    void setPlayhead (double b) { if (b != playhead) { playhead = b; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    DrumResult res;
    double loopBeats = 16.0, playhead = -1.0;
};

class DrumDragZone : public juce::Component
{
public:
    DrumDragZone (DarkTrapDrumsProcessor& p, DarkTrapDrumsProcessor::Part pt, juce::String t, juce::Colour c)
        : proc (p), part (pt), text (std::move (t)), colour (c) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
private:
    DarkTrapDrumsProcessor& proc;
    DarkTrapDrumsProcessor::Part part;
    juce::String text;
    juce::Colour colour;
    bool dragging = false, hover = false;
};

class DarkTrapDrumsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DarkTrapDrumsEditor (DarkTrapDrumsProcessor&);
    ~DarkTrapDrumsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void addKnob (const juce::String& id, const juce::String& label, bool mix = false);
    void addCombo (const juce::String& id, const juce::String& label, int width, bool sound = false);
    void addToggle (const juce::String& id, const juce::String& label);
    void exportMidi();

    DrumLookAndFeel lnf;
    DarkTrapDrumsProcessor& proc;

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
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
    std::vector<std::unique_ptr<Knob>> knobs, mixKnobs;
    juce::Label mixLabel;
    std::vector<std::unique_ptr<Combo>> combos, soundCombos;
    std::vector<std::unique_ptr<Toggle>> toggles;

    juce::TextButton generateBtn { "GENERER" }, previewBtn { "PREVIEW" }, exportBtn { "EXPORT .MID" };
    DrumDragZone dKick, dSnare, dFill, dHats, dOpen, d808, dPerc, dFx, dAll;
    DrumGrid grid;
    juce::Label info;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkTrapDrumsEditor)
};
