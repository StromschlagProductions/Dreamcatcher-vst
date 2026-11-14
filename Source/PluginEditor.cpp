#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
const char* kThemeProp = "theme";   // 0 = dunkel, 1 = hell (wird mit dem Projekt gespeichert)
const char* kUiWProp = "uiW";
const char* kUiHProp = "uiH";

juce::String modeDescription (int m)
{
    switch (m)
    {
        case dch::kInner:
            return u8 ("Die Stimme rückt ins Innere des Kopfes: schmal, nah, ein winziger, enger Raum und ein Hauch Echo, die Höhen weicher, die Tiefen weg. "
                       "Für Gedanken und die Stimme im Kopf einer Figur.");
        case dch::kMemory:
            return u8 ("Die Stimme zerfällt: Sie stottert, sinkt ab, klingt dünn und dumpf wie in einem alten Zimmer, leicht wie von einer leiernden Aufnahme. Nach einem Pre-Delay schwappen Echos "
                       "von links nach rechts heran, wop, wop. Für Rückblenden, zum Beispiel die alte Stimme, die aus einer Überblendung kommt.");
        case dch::kDream:
            return u8 ("Die Stimme ist körnig verstimmt und wabert langsam in der Tonhöhe, in einem riesigen, dunklen, schwebenden Hall mit einer Echo-Wolke, "
                       "deren Wiederholungen schief wegdriften. Für Träume, Fieber und Halbschlaf.");
        default:
            return u8 ("Reverse-Hall und Reverse-Echo: Vor jedem Wort schwillt ein Geisterhall an und rückwärts gespielte Stücke saugen ins Wort hinein. Das Plugin meldet dafür rund 0,6 s Latenz, die "
                       "die DAW ausgleicht. Für Geister und Erscheinungen. Am besten auf einer eigenen Spur.");
    }
}
juce::String modeShort (int m)
{
    return m == dch::kInner ? u8 ("INNERE STIMME") : m == dch::kMemory ? u8 ("ERINNERUNG") : m == dch::kDream ? u8 ("TRAUM") : u8 ("GEIST");
}
}

DreamcatcherEditor::Theme DreamcatcherEditor::darkTheme()
{
    return { juce::Colour (0xff182326), juce::Colour (0xff233539), juce::Colour (0xfff5781a),
             juce::Colour (0xffe8ecec), juce::Colour (0xff8fa1a5), juce::Colour (0xff35494e), true };
}
DreamcatcherEditor::Theme DreamcatcherEditor::lightTheme()
{
    return { juce::Colour (0xffd5dbda), juce::Colour (0xffe8eae9), juce::Colour (0xfff5781a),
             juce::Colour (0xff233539), juce::Colour (0xff5d6e72), juce::Colour (0xffb5bfbe), false };
}

static juce::AttributedString buildHelp (juce::Colour text, juce::Colour accent, juce::Colour dim)
{
    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.setJustification (juce::Justification::topLeft);

    auto head = [&] (const char* t) { s.append (u8 (t) + "\n", juce::Font (juce::FontOptions (14.0f, juce::Font::bold)), accent); };
    auto body = [&] (const char* t) { s.append (u8 (t) + "\n\n", juce::Font (juce::FontOptions (13.0f)), text); };
    auto note = [&] (const char* t) { s.append (u8 (t) + "\n\n", juce::Font (juce::FontOptions (12.0f)), dim); };

    s.append (u8 ("DREAMCATCHER – Hilfe") + u8 (" · v") + juce::String (PLUGIN_VERSION_TEXT) + "\n\n",
              juce::Font (juce::FontOptions (17.0f, juce::Font::bold)), accent);

    body ("Dreamcatcher ist ein Insert-Effekt für Stimmen, die nicht im Raum der Szene sprechen: innere Stimme, Erinnerung und Traum. "
          "Der Klang jedes Modus ist fest abgestimmt. Bedient wird es mit dem Modus, dem Effekt-Fader und drei Drehreglern, mit denen du Hall, Echo und Stereobreite des Modus anpasst.");

    head ("MODUS");
    body ("Innere Stimme: Die Stimme rückt ins Innere des Kopfes. Schmal und nah, ein winziger, enger Raum, ein Hauch Echo, Höhen weicher, Tiefen weg.");
    body ("Erinnerung: Die Stimme stottert, sinkt ab, klingt dünn und dumpf in einem kleinen, alten Zimmer, mit leichtem Bandleiern. Nach einem Pre-Delay schwappen Ping-Pong-Echos von links nach rechts "
          "heran und schwellen an (wop, wop). Gedacht für Rückblenden.");
    body ("Traum: Körnig verstimmt, die Tonhöhe wabert langsam, in einem riesigen, dunklen, schwebenden Hall und einer Echo-Wolke, deren Wiederholungen immer schiefer wegdriften.");
    body ("Geist: Echter Reverse-Hall. Vor jedem Wort schwillt ein Geisterhall an, dann erst kommt die Stimme, trocken und ohne Nachhall. Dazu kommt ein Reverse-Echo: Stücke der Stimme werden rückwärts gespielt und saugen ins Wort hinein. Dafür muss das Plugin die Stimme "
          "um rund 0,6 Sekunden verzögern und meldet diese Latenz dem Host; die DAW gleicht sie aus, sodass der Schwell hörbar VOR der Stimme liegt.");
    body ("Der Modus lässt sich automatisieren. Beim Wechsel blendet das Plugin in etwa einer zehntel Sekunde über, es knackt nicht.");

    head ("HALL und ECHO (Drehregler, 0–200 %)");
    body ("Mit den beiden Drehreglern passt du den Modus an. 100 % ist der Klang, wie der Modus abgestimmt ist, 0 % schaltet Hall bzw. Echo ab, "
          "200 % gibt doppelt so viel. So kannst du zum Beispiel die Innere Stimme mit mehr Raum spielen oder den Traum ohne Echos. "
          "Doppelklick setzt einen Regler auf 100 % zurück. Beide lassen sich automatisieren und gelten für den gerade gewählten Modus.");
    body ("Im Modus Geist heißen die Regler Reverse-Hall und Reverse-Echo: Damit wählst du, ob nur der rückwärts anschwellende Hall, nur die "
          "rückwärts gespielten Echos oder beides vor der Stimme liegen.");
    note ("Mehr Hall oder Echo macht den Klang lauter, weniger leiser; der Pegelausgleich gilt für die Einstellung 100 %.");

    head ("BREITE (Drehregler, 0–200 %)");
    body ("Die Stereobreite des Effekts: 0 % = mono, 100 % = so breit, wie der Modus abgestimmt ist, 200 % = deutlich breiter. Über 100 % wird "
          "das Seitensignal zusätzlich minimal verzögert (nach dem Prinzip von Airwindows Wider), dann wirkt der Klang breiter als die Lautsprecher. "
          "Die unbearbeitete Stimme (Effekt-Fader auf 0 %) bleibt davon unberührt.");
    note ("Der Regler ist monokompatibel: Er verändert nur das Seitensignal. In Mono klingt es bei 0 %, 100 % und 200 % exakt gleich, "
          "man hört dort nur die Breite nicht.");

    head ("EFFEKT (Fader, 0–100 %)");
    body ("0 % = die Stimme unbearbeitet, 100 % = der volle Klang des gewählten Modus. Dazwischen blendet der Fader fließend über. "
          "Meist steht er auf 100 %. Mit Automation holst du die Stimme zurück ins Hier und Jetzt, zum Beispiel beim Aufwachen, "
          "oder lässt die alte Stimme aus der Überblendung langsam in die Erinnerung hineinwachsen.");
    note ("Es gibt bewusst keinen Ausgangsregler. Der Pegel ist fest ausgeglichen, bei 100 % ist die Stimme etwa so laut wie bei 0 %.");

    head ("Allgemein");
    body ("Latenz: Nur im Modus Geist meldet das Plugin rund 0,6 s Latenz an den Host, in den anderen drei Modi ist es latenzfrei. Weil sich die gemeldete "
          "Latenz beim Moduswechsel ändert, nimm Geist am besten auf einer eigenen Spur oder Plugin-Instanz und automatisiere den Modus nicht quer durch die Szene. "
          "Nach dem Ende der Stimme klingen Hall und Echos nach, im Modus Traum bis zu etwa fünf Sekunden; im Modus Geist bleibt die Stimme trocken, es gibt keinen Nachhall.");
    body ("Doppelklick auf den Fader setzt ihn auf 100 % zurück. Look: Dunkel / Hell, das Fenster ist am rechten unteren Rand skalierbar (50–300 %), "
          "beides wird mit dem Projekt gespeichert.");
    body ("Auch auf einem Bus: Du kannst Dreamcatcher auf einen Gruppenkanal legen, damit die Stimme und die Geräusche einer Traum- oder "
          "Erinnerungsszene (Atmo, Schritte, Musik) gemeinsam dieselbe Behandlung bekommen und wie aus einer Welt klingen. Der Effekt arbeitet "
          "dann auf der Summe. Der Fader auf dem Bus blendet die ganze Szene zusammen in den Effekt hinein oder zurück.");
    body ("Der Eingang wird als Stereo verarbeitet. Eine Mono-Spur bekommt durch das Plugin eine Stereobreite.");

    head ("Idee und Konzept");
    body ("Lars Eickstädt, Stromschlag Audio");
    note ("Gebaut mit JUCE. Die Klangbausteine SweetWide, StereoEnsemble, GlitchShifter, PitchNasty, Flutter, GalacticVibe und der Hall Galactic sind Portierungen von Airwindows-Code "
          "(Chris Johnson, MIT-Lizenz).");
    return s;
}

DreamcatcherEditor::DreamcatcherEditor (DreamcatcherProcessor& p) : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (kBaseW / 2, kBaseH / 2, kBaseW * 3, kBaseH * 3);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
    theme = (int) processor.apvts.state.getProperty (kThemeProp, 0) == 1 ? lightTheme() : darkTheme();
    setLookAndFeel (&look);

    const auto modes = DreamcatcherProcessor::modeNames();
    for (int i = 0; i < 4; ++i)
    {
        modeButtons[i].setButtonText (modes[i].toUpperCase());
        modeButtons[i].onClick = [this, i] { setMode (i); };
        content.addAndMakeVisible (modeButtons[i]);
    }
    modeButtons[0].setTooltip (u8 ("Die Stimme im Kopf einer Figur: nah, leicht verbreitert, weich"));
    modeButtons[1].setTooltip (u8 ("Rückblende: stotternd, abgesunken, dünn und dumpf, mit heranschwappenden Echos"));
    modeButtons[2].setTooltip (u8 ("Traum: körnig verstimmt, in einer weiten, anschwellenden Echo-Wolke"));
    modeButtons[3].setTooltip (u8 ("Geist: Reverse-Hall und Reverse-Echo vor der Stimme (meldet ca. 0,6 s Latenz)"));

    fader.setSliderStyle (juce::Slider::LinearHorizontal);
    fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    fader.setDoubleClickReturnValue (true, 1.0);
    fader.setTooltip (u8 ("0 % = die Stimme unbearbeitet, 100 % = voller Klang des Modus. Per Automation einblenden oder zurückholen."));
    content.addAndMakeVisible (fader);
    faderAtt = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "dream", fader);

    setupKnob (hallKnob, "hall");
    setupKnob (echoKnob, "echo");
    setupKnob (widthKnob, "width");
    widthKnob.label.setText (u8 ("BREITE"), juce::dontSendNotification);
    widthKnob.slider.setTooltip (u8 ("Stereobreite: 0 % = mono, 100 % = wie der Modus abgestimmt ist, 200 % = breiter als die Lautsprecher. Doppelklick: 100 %"));

    themeButton.onClick = [this]
    {
        theme = theme.dark ? lightTheme() : darkTheme();
        processor.apvts.state.setProperty (kThemeProp, theme.dark ? 0 : 1, nullptr);
        applyTheme();
    };
    content.addAndMakeVisible (themeButton);

    helpButton.setButtonText ("?");
    helpButton.setTooltip (u8 ("Hilfe: was machen die Regler?"));
    helpButton.onClick = [this] { helpScroll.setVisible (! helpScroll.isVisible()); };
    content.addAndMakeVisible (helpButton);

    helpScroll.setViewedComponent (&helpText, false);
    helpScroll.setScrollBarsShown (true, false);
    helpScroll.setVisible (false);
    content.addChildComponent (helpScroll);

    applyTheme();
    updateModeButtons();

    const int savedW = (int) processor.apvts.state.getProperty (kUiWProp, kBaseW);
    const int savedH = (int) processor.apvts.state.getProperty (kUiHProp, kBaseH);
    setSize (juce::jlimit (kBaseW / 2, kBaseW * 3, savedW), juce::jlimit (kBaseH / 2, kBaseH * 3, savedH));
    sizeReady = true;
    startTimerHz (15);
}

DreamcatcherEditor::~DreamcatcherEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int DreamcatcherEditor::currentMode() const
{
    return juce::jlimit (0, 3, juce::roundToInt (processor.apvts.getRawParameterValue ("mode")->load()));
}

void DreamcatcherEditor::setMode (int m)
{
    if (auto* prm = processor.apvts.getParameter ("mode"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) m));
        prm->endChangeGesture();
    }
    timerCallback();
}

void DreamcatcherEditor::timerCallback()
{
    const int m = currentMode();
    if (m != shownMode)
    {
        shownMode = m;
        updateModeButtons();
        repaint();
    }
}

void DreamcatcherEditor::updateModeButtons()
{
    const int m = currentMode();
    for (int i = 0; i < 4; ++i)
        modeButtons[i].setToggleState (i == m, juce::dontSendNotification);
    updateKnobLabels();
}

void DreamcatcherEditor::setupKnob (Knob& k, const juce::String& paramId)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setDoubleClickReturnValue (true, 1.0);
    content.addAndMakeVisible (k.slider);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    content.addAndMakeVisible (k.label);
    k.attachment = std::make_unique<APVTS::SliderAttachment> (processor.apvts, paramId, k.slider);
}

void DreamcatcherEditor::updateKnobLabels()
{
    const bool ghost = currentMode() == dch::kGhost;
    hallKnob.label.setText (ghost ? u8 ("REVERSE-HALL") : u8 ("HALL"), juce::dontSendNotification);
    echoKnob.label.setText (ghost ? u8 ("REVERSE-ECHO") : u8 ("ECHO"), juce::dontSendNotification);
    hallKnob.slider.setTooltip (ghost ? u8 ("Reverse-Hall vor der Stimme: 100 % = wie abgestimmt, 0 % = aus, 200 % = doppelt. Doppelklick: 100 %")
                                      : u8 ("Hall: 100 % = wie der Modus abgestimmt ist, 0 % = kein Hall, 200 % = doppelt so viel. Doppelklick: 100 %"));
    echoKnob.slider.setTooltip (ghost ? u8 ("Reverse-Echo: rückwärts gespielte Stücke, die ins Wort hineinsaugen. 100 % = wie abgestimmt, 0 % = aus. Doppelklick: 100 %")
                                      : u8 ("Echo: 100 % = wie der Modus abgestimmt ist, 0 % = keine Echos, 200 % = doppelt so viel. Doppelklick: 100 %"));
}

void DreamcatcherEditor::applyTheme()
{
    look.setColour (juce::Slider::thumbColourId, theme.accent);
    look.setColour (juce::Slider::trackColourId, theme.accent);
    look.setColour (juce::Slider::rotarySliderFillColourId, theme.accent);
    look.setColour (juce::Slider::rotarySliderOutlineColourId, theme.line);
    look.setColour (juce::Slider::backgroundColourId, theme.line);
    look.setColour (juce::Slider::textBoxTextColourId, theme.text);
    look.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    look.setColour (juce::PopupMenu::backgroundColourId, theme.panel);
    look.setColour (juce::PopupMenu::textColourId, theme.text);
    look.setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent);
    look.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    look.setColour (juce::Label::textColourId, theme.text);
    for (auto* k : { &hallKnob, &echoKnob, &widthKnob }) k->label.setColour (juce::Label::textColourId, theme.dim);
    look.setColour (juce::TextButton::buttonColourId, theme.dark ? theme.panel : theme.bg);
    look.setColour (juce::TextButton::buttonOnColourId, theme.accent);
    look.setColour (juce::TextButton::textColourOffId, theme.text);
    look.setColour (juce::TextButton::textColourOnId, juce::Colours::black);

    themeButton.setButtonText (theme.dark ? "Look: Dunkel" : "Look: Hell");
    helpText.setAttributed (buildHelp (theme.text, theme.accent, theme.dim));
    helpScroll.fill = theme.panel;
    helpScroll.edge = theme.line;
    sendLookAndFeelChange();
    repaint();
}

void DreamcatcherEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme.bg);
    g.addTransform (juce::AffineTransform::scale (uiScale));
    const int W = kBaseW;
    const int m = currentMode();

    // Kopfleiste wie bei LOCATIONS und RADIOPHONE
    g.setColour (theme.panel);
    g.fillRect (0, 0, W, 64);
    g.setColour (theme.line);
    g.fillRect (0, 63, W, 1);
    g.setColour (theme.accent);
    g.fillRect (0, 0, 5, 64);
    if (logo.isValid())
    {
        g.setColour (theme.text);
        g.drawImage (logo, juce::Rectangle<float> (14.0f, 4.0f, 56.0f, 56.0f), juce::RectanglePlacement::centred, true);
    }

    const juce::Font titleFont (juce::FontOptions (22.0f, juce::Font::bold));
    const juce::Font subFont (juce::FontOptions (11.5f, juce::Font::bold));
    auto textWidth = [] (const juce::Font& f, const juce::String& t)
    {
        juce::GlyphArrangement ga; ga.addLineOfText (f, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    };
    const juce::String sub = "AUDIO DRAMA MINDSCAPE SIMULATOR";
    const float subX = 80.0f + textWidth (titleFont, "DREAMCATCHER") + 16.0f;
    g.setColour (theme.text);
    g.setFont (titleFont);
    g.drawText ("DREAMCATCHER", 80, 9, 220, 28, juce::Justification::centredLeft);
    g.setColour (theme.accent);
    g.setFont (subFont);
    g.drawText (sub, (int) subX, 14, 340, 22, juce::Justification::centredLeft);
    g.setColour (theme.dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (u8 ("STROMSCHLAG AUDIO  ·  v" PLUGIN_VERSION_TEXT), 80, 36, 300, 16, juce::Justification::centredLeft);

    // Kleines Symbol: Traumfänger (einfarbig, folgt dem Look)
    {
        juce::Path hoop, web, strings, feathers;
        const float cx = 13.0f, cy = 13.0f, R = 11.0f;
        hoop.addEllipse (cx - R, cy - R, 2.0f * R, 2.0f * R);
        const float pi2 = juce::MathConstants<float>::twoPi;
        for (float r : { 7.5f, 4.0f })
        {
            for (int i = 0; i <= 8; ++i)
            {
                const float a = pi2 * (float) i / 8.0f + 0.3927f;
                const float x = cx + r * std::cos (a), y = cy + r * std::sin (a);
                if (i == 0) web.startNewSubPath (x, y); else web.lineTo (x, y);
            }
        }
        for (int i = 0; i < 8; ++i)
        {
            const float a = pi2 * (float) i / 8.0f + 0.3927f;
            web.startNewSubPath (cx + 1.2f * std::cos (a), cy + 1.2f * std::sin (a));
            web.lineTo (cx + R * std::cos (a), cy + R * std::sin (a));
        }
        struct Str { float x0, y0, x1, y1; };
        for (const Str& s : { Str { 13.0f, 24.0f, 13.0f, 33.0f }, Str { 7.0f, 22.0f, 6.0f, 29.0f }, Str { 19.0f, 22.0f, 20.0f, 29.0f } })
        {
            strings.startNewSubPath (s.x0, s.y0);
            strings.lineTo (s.x1, s.y1);
            feathers.addEllipse (s.x1 - 1.3f, s.y1 - 0.5f, 2.6f, 5.5f);
        }
        const float subEnd = subX + textWidth (subFont, sub);
        const float helpX = (float) (kBaseW - 160);   // linke Kante von "Look"
        const float mid = 0.5f * (subEnd + helpX);
        const auto place = juce::AffineTransform::scale (1.1f).translated (mid - 14.3f, 12.0f);
        for (auto* p : { &hoop, &web, &strings, &feathers }) p->applyTransform (place);
        const auto ink = theme.panel.interpolatedWith (theme.text, 0.30f);
        const auto fill = theme.panel.interpolatedWith (theme.text, 0.10f);
        g.setColour (fill);
        g.fillPath (feathers);
        g.setColour (ink);
        g.strokePath (hoop, juce::PathStrokeType (1.8f));
        g.strokePath (web, juce::PathStrokeType (0.9f));
        g.strokePath (strings, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (feathers, juce::PathStrokeType (1.0f));
    }

    auto card = [&] (float x, float y, float w, float h, const juce::String& title)
    {
        g.setColour (theme.panel);
        g.fillRoundedRectangle (x, y, w, h, 8.0f);
        g.setColour (theme.line);
        g.drawRoundedRectangle (x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 8.0f, 1.0f);
        g.setColour (theme.accent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, (int) x + 12, (int) y + 4, (int) w - 24, 18, juce::Justification::centredLeft);
    };

    // Karte Modus
    card (12.0f, 76.0f, (float) W - 24.0f, 84.0f, "MODUS");
    // Karte Effekt-Fader
    card (12.0f, 170.0f, 320.0f, 150.0f, "EFFEKT");
    g.setColour (theme.dim);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (u8 ("0 %  ·  Stimme unbearbeitet"), 24, 206, 160, 16, juce::Justification::centredLeft);
    g.drawText (u8 ("100 %  ·  voller Klang"), 320 - 130, 206, 130, 16, juce::Justification::centredRight);
    // Karte Hall / Echo
    card (342.0f, 170.0f, (float) W - 354.0f, 150.0f, m == dch::kGhost ? u8 ("REVERSE & BREITE") : u8 ("HALL · ECHO · BREITE"));
    // Karte Beschreibung
    card (12.0f, 330.0f, (float) W - 24.0f, 78.0f, modeShort (m));
    g.setColour (theme.text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (modeDescription (m), 24, 352, W - 48, 54, juce::Justification::topLeft, 3, 1.0f);
}

void DreamcatcherEditor::resized()
{
    if (sizeReady)
    {
        processor.apvts.state.setProperty (kUiWProp, getWidth(), nullptr);
        processor.apvts.state.setProperty (kUiHProp, getHeight(), nullptr);
    }
    uiScale = std::min ((float) getWidth() / kBaseW, (float) getHeight() / kBaseH);
    content.setTransform (juce::AffineTransform::scale (uiScale));
    content.setBounds (0, 0, kBaseW, kBaseH);
    repaint();

    const int w = kBaseW;
    themeButton.setBounds (w - 160, 18, 104, 28);
    helpButton.setBounds (w - 48, 18, 32, 28);
    helpScroll.setBounds (12, 76, w - 24, 332);
    helpText.setWidth (helpScroll.getMaximumVisibleWidth());

    const int bx = 24, bw = (w - 48 - 30) / 4;
    for (int i = 0; i < 4; ++i)
        modeButtons[i].setBounds (bx + i * (bw + 10), 106, bw, 44);

    fader.setBounds (24, 228, 296, 72);
    {
        Knob* ks[3] = { &hallKnob, &echoKnob, &widthKnob };
        for (int i = 0; i < 3; ++i)
        {
            const int x = 350 + i * 106;
            ks[i]->label.setBounds (x, 192, 104, 16);
            ks[i]->slider.setBounds (x + 2, 208, 100, 106);
        }
    }

    helpScroll.toFront (false);
}
