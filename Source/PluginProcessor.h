/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


struct DistortionParameters
{
    float gain{ 1.f };
    float tone{ 1.f };
};

DistortionParameters getDistortionParameters(juce::AudioProcessorValueTreeState& apvts);


struct DistortionProcessor
{
    DistortionProcessor() = default;

    void setParameters(const DistortionParameters& newParams)
    {
        params = newParams;
    }

    float processSample(float inputSample)
    {
        float x = inputSample;
        x *= params.gain;
        return std::tanh(x + 0.3f * x * x * x);
    }

private:
    DistortionParameters params;
};



//==============================================================================
/**
*/
class DistortionPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    DistortionPluginAudioProcessor();
    ~DistortionPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts{*this, nullptr, "Parameters", createParameterLayout()};

private:
    DistortionProcessor distortionProcessor;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionPluginAudioProcessor)
};
