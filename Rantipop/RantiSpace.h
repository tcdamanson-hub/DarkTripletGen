#pragma once
#include <juce_graphics/juce_graphics.h>

// Fond "espace" (nebuleuses, poussiere, etoiles scintillantes, vignette), dessine une seule fois.
// warm = true : touches dorees (MELODIE) ; false : bleu / violet (DRUMS)
namespace ranti
{
inline juce::Image makeSpace (int W, int H, int seed, bool warm)
{
    juce::Image img (juce::Image::ARGB, W, H, true);
    juce::Graphics g (img);
    juce::Random r (seed);
    auto cloud = [&] (float cx, float cy, float rx, float ry, juce::Colour c)
    {
        juce::ColourGradient gr (c, cx, cy, c.withAlpha (0.0f), cx + rx, cy, true);
        gr.addColour (0.45, c.withMultipliedAlpha (0.45f));
        g.setGradientFill (gr);
        g.fillEllipse (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
    };
    const float w = (float) W, h = (float) H;
    // nebuleuses
    const juce::Colour deep (0xff1b2a6e), violet (0xff4a2a7a), teal (0xff1d5a7a);
    const juce::Colour accent = warm ? juce::Colour (0xffc9923a) : juce::Colour (0xff3a7bd5);
    cloud (w * 0.16f, h * 0.22f, w * 0.30f, h * 0.34f, deep.withAlpha (0.30f));
    cloud (w * 0.84f, h * 0.72f, w * 0.32f, h * 0.36f, violet.withAlpha (0.24f));
    cloud (w * 0.70f, h * 0.12f, w * 0.22f, h * 0.20f, teal.withAlpha (0.16f));
    cloud (w * 0.30f, h * 0.80f, w * 0.26f, h * 0.22f, violet.withAlpha (0.14f));
    cloud (w * 0.50f, h * 0.40f, w * 0.30f, h * 0.30f, accent.withAlpha (warm ? 0.10f : 0.12f));
    // grain / poussiere d'etoiles
    for (int i = 0; i < 2600; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (r.nextFloat() * 0.10f + 0.02f));
        g.fillRect (r.nextFloat() * w, r.nextFloat() * h, 1.0f, 1.0f);
    }
    // etoiles
    const juce::Colour starWarm = warm ? juce::Colour (0xffffe2a0) : juce::Colour (0xffa0c8ff);
    for (int i = 0; i < 520; ++i)
    {
        const float a = r.nextFloat() * 0.75f + 0.15f, s = r.nextFloat() * 1.8f + 0.5f;
        g.setColour ((r.nextFloat() < 0.3f ? starWarm : juce::Colour (0xfff4f8ff)).withAlpha (a));
        g.fillEllipse (r.nextFloat() * w, r.nextFloat() * h, s, s);
    }
    // etoiles brillantes avec halo
    for (int i = 0; i < 34; ++i)
    {
        const float x = r.nextFloat() * w, y = r.nextFloat() * h, rad = r.nextFloat() * 7.0f + 4.0f;
        const auto c = (r.nextFloat() < 0.5f ? juce::Colour (0xffcfe2ff) : starWarm);
        juce::ColourGradient gl (c.withAlpha (0.55f), x, y, c.withAlpha (0.0f), x + rad, y, true);
        g.setGradientFill (gl);
        g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.fillEllipse (x - 1.2f, y - 1.2f, 2.4f, 2.4f);
    }
    // quelques etoiles scintillantes (croix de lumiere)
    for (int i = 0; i < 9; ++i)
    {
        const float x = r.nextFloat() * w, y = r.nextFloat() * h, len = r.nextFloat() * 14.0f + 10.0f;
        const auto c = juce::Colour (0xffe6f0ff);
        for (int k = 0; k < 2; ++k)
        {
            juce::ColourGradient ray (c.withAlpha (0.7f), x, y, c.withAlpha (0.0f), k == 0 ? x + len : x, k == 0 ? y : y + len, true);
            g.setGradientFill (ray);
            if (k == 0) g.fillRect (x - len, y - 0.6f, len * 2.0f, 1.2f);
            else        g.fillRect (x - 0.6f, y - len, 1.2f, len * 2.0f);
        }
        juce::ColourGradient core (juce::Colours::white.withAlpha (0.9f), x, y, c.withAlpha (0.0f), x + 5.0f, y, true);
        g.setGradientFill (core);
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
    }
    // vignette
    {
        juce::ColourGradient v (juce::Colours::transparentBlack, w * 0.5f, h * 0.45f, juce::Colours::black.withAlpha (0.55f), w * 1.02f, h * 0.45f, true);
        v.addColour (0.6, juce::Colours::transparentBlack);
        g.setGradientFill (v);
        g.fillRect (0.0f, 0.0f, w, h);
    }
    return img;
}
} // namespace ranti
