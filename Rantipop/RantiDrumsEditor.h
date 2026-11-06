#pragma once
#include "RantiSpace.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "RantiDrumsProcessor.h"

namespace dtheme
{
const juce::Colour text   { 0xffcfe3ff };
const juce::Colour cyan   { 0xff7fd4ff };
const juce::Colour violet { 0xffa98bff };
const juce::Colour hi     { 0xffeaf6ff };
const juce::Colour dim    { 0x59a0beff };
const juce::Colour low    { 0x33a0beff };
}

class DrumLook : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (15.0f)); }
};

class DrumRow : public juce::Component
{
public:
    DrumRow (RantiDrumsProcessor& p, int lane, const juce::String& title, const char* volId);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::Slider slider;
    int lane;
    juce::String title;
private:
    RantiDrumsProcessor& proc;
    std::unique_ptr<juce::SliderParameterAttachment> att;
    juce::Rectangle<int> subArea;
};

class RantiDrumsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RantiDrumsEditor (RantiDrumsProcessor&);
    ~RantiDrumsEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void drawPlanet (juce::Graphics&, juce::Point<float> c, float r);
    void showKitMenu();

    RantiDrumsProcessor& proc;
    DrumLook look;
    juce::Image logo, stars;
    float spin = 0.0f; // rotation de la planete
    bool planetDynamic = false;      // false : decor fixe ; true : ce qui tourne
    bool faceOnly = false;
    juce::Image faceImg;
    float faceScale = 2.0f;
    int animTick = 0;
    int planetVersion = -1;
    bool planetPlaying = false;
    ranti::PlanetLayer planetLayer;
    void refreshPlanetBase();
    std::vector<std::unique_ptr<DrumRow>> rows;
    juce::Slider master;
    std::unique_ptr<juce::SliderParameterAttachment> masterAtt;

    juce::Rectangle<float> genreTabs[4], prevBtn, nextBtn, genBtn, previewBtn, grooveBox, dragBox, infoBox, pads[10];
    bool dragging = false, genDown = false;
    std::array<juce::uint32, ranti::NumLanes> seenHit {};
    std::array<float, 10> padGlow {};
    int lastVersion = -1;
};
