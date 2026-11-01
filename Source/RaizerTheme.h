#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// =============================================================================
//  RAIZER DA VST - theme commun aux deux plugins
//  espace etoile, nebuleuse violette, sphere lumineuse, panneaux en verre
// =============================================================================
namespace Raizer
{
const juce::Colour space   { 0xff04050d };
const juce::Colour deep    { 0xff0a0d24 };
const juce::Colour nebula  { 0xff5b2dff };
const juce::Colour cyan    { 0xff3fd6ff };
const juce::Colour magenta { 0xffff3fa4 };
const juce::Colour text    { 0xffe9eaff };
const juce::Colour dim     { 0xff8b90bd };
const juce::Colour glass   { 0x14ffffff };
const juce::Colour edge    { 0x26ffffff };

// fond : degrade spatial + nebuleuses + etoiles (dessine une fois dans une image)
inline juce::Image makeBackground (int w, int h)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
    juce::Graphics g (img);
    g.setGradientFill (juce::ColourGradient (deep, 0.0f, 0.0f, space, 0.0f, (float) h, false));
    g.fillAll();
    auto blob = [&] (float cx, float cy, float r, juce::Colour c)
    {
        juce::ColourGradient gr (c, cx, cy, c.withAlpha (0.0f), cx + r, cy, true);
        g.setGradientFill (gr);
        g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    };
    blob (w * 0.18f, h * 0.10f, w * 0.35f, nebula.withAlpha (0.30f));
    blob (w * 0.85f, h * 0.25f, w * 0.30f, magenta.withAlpha (0.12f));
    blob (w * 0.60f, h * 0.95f, w * 0.40f, cyan.withAlpha (0.08f));
    juce::Random rnd (4242);
    for (int i = 0; i < (w * h) / 900; ++i)
    {
        const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h;
        const float s = rnd.nextFloat() < 0.92f ? 1.0f : 2.0f;
        g.setColour (juce::Colours::white.withAlpha (0.15f + 0.6f * rnd.nextFloat() * rnd.nextFloat()));
        g.fillEllipse (x, y, s, s);
    }
    return img;
}

// sphere lumineuse (pulse = 0..1, suit le tempo)
inline void drawOrb (juce::Graphics& g, juce::Point<float> c, float r, float pulse)
{
    const float glow = r * (1.9f + 0.35f * pulse);
    juce::ColourGradient halo (nebula.withAlpha (0.55f + 0.3f * pulse), c.x, c.y, nebula.withAlpha (0.0f), c.x + glow, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (c.x - glow, c.y - glow, glow * 2, glow * 2);
    juce::ColourGradient body (cyan.brighter (0.4f), c.x - r * 0.35f, c.y - r * 0.4f, nebula.darker (0.6f), c.x + r, c.y + r, true);
    body.addColour (0.45, nebula.brighter (0.2f));
    g.setGradientFill (body);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.6f, r * 0.45f, r * 0.3f);
    g.setColour (cyan.withAlpha (0.5f + 0.4f * pulse));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.2f);
}

// panneau en verre
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 10.0f)
{
    g.setColour (juce::Colour (0x66060816));
    g.fillRoundedRectangle (r, corner);
    g.setGradientFill (juce::ColourGradient (glass.withMultipliedAlpha (1.6f), r.getX(), r.getY(), glass.withAlpha (0.0f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

inline void drawTitle (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, const juce::String& sub)
{
    g.setGradientFill (juce::ColourGradient (text, r.getX(), r.getY(), cyan, r.getX() + 260.0f, r.getY(), false));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (title, r.withHeight (26.0f), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (sub, r.withTrimmedTop (24.0f).withHeight (14.0f), juce::Justification::centredLeft);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0x660a0d24));
        setColour (juce::ComboBox::outlineColourId, edge);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, cyan);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf20a0d24));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, cyan);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, nebula.withAlpha (0.6f));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0x660a0d24));
        setColour (juce::TextButton::buttonOnColourId, nebula);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, cyan);
        setColour (juce::ToggleButton::tickDisabledColourId, dim);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override
    {
        auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (5.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        // corps
        juce::ColourGradient body (juce::Colour (0xff1c2150), c.x - r * 0.4f, c.y - r * 0.5f, juce::Colour (0xff070818), c.x + r, c.y + r, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f);
        g.setColour (edge);
        g.drawEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f, 1.0f);
        // anneau
        juce::Path track, val;
        track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
        val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, ang, true);
        g.setColour (juce::Colour (0x22ffffff));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cyan.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (juce::ColourGradient (nebula, c.x - r, c.y + r, cyan, c.x + r, c.y - r, false));
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // point lumineux
        const juce::Point<float> d (c.x + std::sin (ang) * r * 0.48f, c.y - std::cos (ang) * r * 0.48f);
        g.setColour (cyan.withAlpha (0.35f));
        g.fillEllipse (d.x - 4.0f, d.y - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (d.x - 2.0f, d.y - 2.0f, 4.0f, 4.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colour (0x880a0d24));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.hasKeyboardFocus (true) || box.isMouseOver (true) ? cyan.withAlpha (0.6f) : edge);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (cyan);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (juce::FontOptions (13.0f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        if (on || over)
        {
            g.setColour ((on ? nebula : cyan).withAlpha (0.25f));
            g.fillRoundedRectangle (r.expanded (1.0f), 9.0f);
        }
        juce::ColourGradient gr (on ? nebula.brighter (0.2f) : juce::Colour (0xaa161a3d), r.getX(), r.getY(),
                                 on ? nebula.darker (0.4f) : juce::Colour (0xaa080a1c), r.getX(), r.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (down ? cyan : (over ? cyan.withAlpha (0.7f) : edge));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto sw = r.removeFromLeft (30.0f).withSizeKeepingCentre (26.0f, 14.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? nebula.withAlpha (0.9f) : juce::Colour (0x55ffffff));
        g.fillRoundedRectangle (sw, 7.0f);
        if (on) { g.setColour (cyan.withAlpha (0.3f)); g.drawRoundedRectangle (sw.expanded (1.5f), 8.0f, 2.0f); }
        const float kx = on ? sw.getRight() - 12.0f : sw.getX() + 2.0f;
        g.setColour (juce::Colours::white);
        g.fillEllipse (kx, sw.getY() + 2.0f, 10.0f, 10.0f);
        g.setColour (over ? text : text.withAlpha (0.85f));
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
    }
};
} // namespace Raizer
