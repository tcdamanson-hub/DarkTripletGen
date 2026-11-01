#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// =============================================================================
//  Historique des generations : chaque clic sur GENERER memorise les reglages
//  du generateur (seed compris). "<" / ">" permettent de revenir a une boucle
//  qu'on a aimee. Les volumes et les sons ne sont pas touches.
// =============================================================================
class GenHistory
{
public:
    GenHistory (juce::AudioProcessorValueTreeState& state, std::vector<juce::String> paramIds)
        : apvts (state), ids (std::move (paramIds)) {}

    // a appeler juste AVANT de changer la seed
    void beforeGenerate()
    {
        const auto cur = capture();
        if (pos < 0 || entries[(size_t) pos] != cur)
        {
            entries.resize ((size_t) (pos + 1));
            entries.push_back (cur);
            pos = (int) entries.size() - 1;
        }
    }

    // a appeler juste APRES avoir change la seed
    void afterGenerate()
    {
        entries.resize ((size_t) (pos + 1));
        entries.push_back (capture());
        if (entries.size() > 100) entries.erase (entries.begin());
        pos = (int) entries.size() - 1;
    }

    bool canBack() const    { return pos > 0; }
    bool canForward() const { return pos >= 0 && pos + 1 < (int) entries.size(); }
    juce::String text() const { return pos < 0 ? juce::String() : juce::String (pos + 1) + "/" + juce::String ((int) entries.size()); }

    void back()
    {
        if (! canBack()) return;
        // reglages modifies a la main depuis : on les garde en fin d'historique
        const auto cur = capture();
        if (cur != entries[(size_t) pos]) { entries.push_back (cur); }
        apply (entries[(size_t) --pos]);
    }

    void forward()
    {
        if (! canForward()) return;
        apply (entries[(size_t) ++pos]);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<juce::String> ids;
    std::vector<std::vector<float>> entries;
    int pos = -1;

    std::vector<float> capture() const
    {
        std::vector<float> v;
        for (auto& id : ids)
            if (auto* p = apvts.getParameter (id)) v.push_back (p->getValue());
        return v;
    }

    void apply (const std::vector<float>& v)
    {
        size_t i = 0;
        for (auto& id : ids)
            if (auto* p = apvts.getParameter (id))
            {
                if (i >= v.size()) break;
                if (p->getValue() != v[i])
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (v[i]);
                    p->endChangeGesture();
                }
                ++i;
            }
    }
};
