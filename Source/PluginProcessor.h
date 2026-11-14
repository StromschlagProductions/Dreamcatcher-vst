#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DreamcatcherDSP.h"

class DreamcatcherProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    DreamcatcherProcessor();
    ~DreamcatcherProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override { engine.reset(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dreamcatcher"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }   // Schwapp-Echos klingen bis zu ca. 3 s nach

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::StringArray modeNames();
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override { setLatencySamples (reportedLatency.load()); }
    std::atomic<int> reportedLatency { 0 };
    dch::Engine engine;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pDream = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreamcatcherProcessor)
};
