#pragma once
#include "PluginProcessor.h"

// Onboard-Hilfe: formatierter Text, scrollbar in einem Viewport (wie bei Locations und Radiophone)
class HelpView : public juce::Component
{
public:
    void setAttributed (const juce::AttributedString& s) { text = s; relayout(); }
    void setWidth (int w) { if (w != laidOutW) { laidOutW = w; relayout(); } }
    void paint (juce::Graphics& g) override { tl.draw (g, getLocalBounds().toFloat().reduced (14.0f, 10.0f)); }
private:
    void relayout()
    {
        const int w = juce::jmax (120, laidOutW);
        tl.createLayout (text, (float) (w - 28));
        setSize (w, (int) std::ceil (tl.getHeight()) + 24);
        repaint();
    }
    juce::AttributedString text;
    juce::TextLayout tl;
    int laidOutW = 0;
};

class HelpPanel : public juce::Viewport
{
public:
    juce::Colour fill, edge;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (fill);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    }
};

class DreamcatcherEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DreamcatcherEditor (DreamcatcherProcessor&);
    ~DreamcatcherEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Theme { juce::Colour bg, panel, accent, text, dim, line; bool dark; };

    void timerCallback() override;
    void applyTheme();
    void updateModeButtons();
    int currentMode() const;
    void setMode (int m);
    static Theme darkTheme();
    static Theme lightTheme();

    static constexpr int kBaseW = 680, kBaseH = 420;

    DreamcatcherProcessor& processor;
    juce::LookAndFeel_V4 look;
    juce::Component content;
    float uiScale = 1.0f;
    Theme theme;
    bool sizeReady = false;
    juce::Image logo;

    juce::TextButton modeButtons[4];
    int shownMode = -1;

    juce::Slider fader;
    std::unique_ptr<APVTS::SliderAttachment> faderAtt;

    juce::TextButton themeButton, helpButton;
    HelpPanel helpScroll;
    HelpView helpText;
    juce::TooltipWindow tooltipWindow { this, 400 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreamcatcherEditor)
};
