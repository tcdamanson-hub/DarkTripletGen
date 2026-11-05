#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "RantiProcessor.h"

namespace rtheme
{
const juce::Colour gold    { 0xffeecc80 };
const juce::Colour goldHi  { 0xfffff3d0 };
const juce::Colour goldDim { 0x59eecc80 };
const juce::Colour goldLow { 0x2eeecc80 };
const juce::Colour bg      { 0xff0b0a0d };
}

// curseur courbe dore (comme la maquette)
class RantiLook : public juce::LookAndFeel_V4
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

// une ligne de controle : TITRE   valeur%
//                         son choisi (clic = menu)
//                         ~~~~~~curseur~~~~~~
class ControlRow : public juce::Component
{
public:
    ControlRow (RantiProcessor& p, const juce::String& title, const juce::String& volId);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<juce::String()> subText;     // texte sous le titre
    std::function<void()> onSubClick;          // clic sur ce texte
    juce::Slider slider;
private:
    RantiProcessor& proc;
    juce::String title;
    std::unique_ptr<juce::SliderParameterAttachment> att;
    juce::Rectangle<int> subArea;
};

class RantiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RantiEditor (RantiProcessor&);
    ~RantiEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void drawPlanet (juce::Graphics&, juce::Point<float> c, float r);
    void drawKeyboard (juce::Graphics&, juce::Rectangle<float>);
    void drawChords (juce::Graphics&);
    void showInstMenu (const char* paramId, bool bass);
    void showGrooveMenu();
    void showKeyMenu();
    juce::Rectangle<float> modePills[16];
    int modePillValue[16] {}, numModePills = 0;
    int pillGenre = -9, pillMode = -9;
    float pillFs = 10.5f;
    juce::StringArray pillText;
    std::vector<int> pillVal;
    std::vector<float> pillW;
    static void glass (juce::Graphics&, juce::Rectangle<float>, float radius = 12.0f);
    static void spaced (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, float kerning,
                        juce::Justification, juce::Colour, bool bold = false);

    RantiProcessor& proc;
    RantiLook look;
    juce::Image logo, stars;
    ControlRow rowMel, rowChords, rowExtra, rowBass, rowVerb;
    juce::Slider master;
    std::unique_ptr<juce::SliderParameterAttachment> masterAtt;

    // zones cliquables
    juce::Rectangle<float> genreTabs[4], prevBtn, nextBtn, genBtn, planetArea, songBox, keyBox, previewBtn, chordBox, dragChips[5];
    int hoverChip = -1;
    bool dragging = false, genDown = false;
    float pulse = 0.0f;
    std::array<bool, 128> keys {};
    int lastVersion = -1;
};
