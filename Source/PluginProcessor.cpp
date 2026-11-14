#include "PluginProcessor.h"
#include "PluginEditor.h"

// Reihenfolge muss zu enum Mode in DreamcatcherDSP.h passen
juce::StringArray DreamcatcherProcessor::modeNames() { return { "Innere Stimme", "Erinnerung", "Traum", "Geist" }; }

juce::AudioProcessorValueTreeState::ParameterLayout DreamcatcherProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters ("0123456789.,").replaceCharacter (',', '.').getFloatValue() / 100.0f; });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modus", modeNames(), 0));
    // 0 % = Stimme unbearbeitet, 100 % = voller Klang des Modus (Standard: 100 %)
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dream", 1 }, "Effekt",
                                                       NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent));
    // Hall und Echo: 100 % = so, wie der Modus abgestimmt ist; 0 % = weg, 200 % = doppelt (im Modus Geist: Reverse-Hall / Reverse-Echo)
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hall", 1 }, "Hall", NormalisableRange<float> (0.0f, 2.0f), 1.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "echo", 1 }, "Echo", NormalisableRange<float> (0.0f, 2.0f), 1.0f, percent));
    return layout;
}

DreamcatcherProcessor::DreamcatcherProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DREAMCATCHER", createLayout())
{
    pMode = apvts.getRawParameterValue ("mode");
    pDream = apvts.getRawParameterValue ("dream");
    pHall = apvts.getRawParameterValue ("hall");
    pEcho = apvts.getRawParameterValue ("echo");
}

void DreamcatcherProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setMode (juce::roundToInt (pMode->load()));   // vor prepare: Startzustand passt gleich zum Modus
    engine.prepare (sampleRate);
    reportedLatency = engine.latencySamples();
    setLatencySamples (reportedLatency.load());   // nur im Modus Geist: Vorlauf fuer den Reverse-Hall, gleicht der Host aus
}

bool DreamcatcherProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in == out) return true;
    return in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo();
}

void DreamcatcherProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numSamples == 0 || numCh == 0)
        return;

    // Mono hinein, Stereo heraus: Eingang auf beide Kanaele legen
    if (getTotalNumInputChannels() == 1 && numCh > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    engine.setMode (juce::roundToInt (pMode->load()));
    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (l, r, numSamples, (double) pDream->load(), (double) pHall->load(), (double) pEcho->load());

    if (engine.latencySamples() != reportedLatency.load())
    {
        reportedLatency = engine.latencySamples();   // Moduswechsel: Host ueber die neue Latenz informieren
        triggerAsyncUpdate();
    }
}

void DreamcatcherProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DreamcatcherProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DreamcatcherProcessor::createEditor() { return new DreamcatcherEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DreamcatcherProcessor(); }
