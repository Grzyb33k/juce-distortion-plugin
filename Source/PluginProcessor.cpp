/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DistortionPluginAudioProcessor::DistortionPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

DistortionPluginAudioProcessor::~DistortionPluginAudioProcessor()
{
}

//==============================================================================
const juce::String DistortionPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DistortionPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DistortionPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DistortionPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DistortionPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DistortionPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DistortionPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DistortionPluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DistortionPluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void DistortionPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DistortionPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numChannels = getTotalNumInputChannels();
    const int oversampleStages = 3;

    oversampler.reset(new juce::dsp::Oversampling<float>(
        (size_t)numChannels,
        oversampleStages,
        juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR,
        true)
    );


    oversampler->initProcessing(samplesPerBlock);
    auto ovRate = oversampler->getOversamplingFactor();
    double ovSampleRate = sampleRate * ovRate;

    for (auto& engine : distortionEngine)
    {
        engine.prepare(ovSampleRate);
    }


    auto params = getDistortionParameters(apvts);

    for (auto& engine : distortionEngine)
        engine.updateParameters(params);
}

void DistortionPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DistortionPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DistortionPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto params = getDistortionParameters(apvts);

    juce::dsp::AudioBlock<float> block(buffer);

    auto oversampledBlock = oversampler->processSamplesUp(block);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        //auto* channelData = buffer.getWritePointer (channel);

        //auto& engine = distortionEngine[static_cast<size_t>(std::min(channel, 1))];
        auto& engine = distortionEngine[channel];

        engine.updateParameters(params);

       /* for (int sample = 0; sample < buffer.getNumSamples(); sample++)
        {
            channelData[sample] = engine.processSample(channelData[sample]);
        }
        */

        //juce::dsp::AudioBlock<float> channelBlock(oversampledBlock.getSingleChannelBlock(channel));
        auto channelBlock = oversampledBlock.getSingleChannelBlock(channel);

        engine.processBlock(channelBlock);
    }

    oversampler->processSamplesDown(block);
}

//==============================================================================
bool DistortionPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DistortionPluginAudioProcessor::createEditor()
{
    //return new DistortionPluginAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void DistortionPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void DistortionPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}


DistortionParameters getDistortionParameters(juce::AudioProcessorValueTreeState& apvts)
{
    DistortionParameters parameters;

    parameters.gain   = apvts.getRawParameterValue("Gain")->load();
    parameters.tone   = apvts.getRawParameterValue("Tone")->load();
    parameters.volume = apvts.getRawParameterValue("Volume")->load();

    return parameters;
}

juce::AudioProcessorValueTreeState::ParameterLayout DistortionPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(
        std::make_unique<juce::AudioParameterFloat>("Gain",
            "Gain",
            juce::NormalisableRange<float>(0.01f, 0.99f, 0.01f, 1.f),
            0.5f)
    );

    layout.add(
        std::make_unique<juce::AudioParameterFloat>("Tone",
            "Tone",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f, 1.f),
            0.5f)
    );

    layout.add(
        std::make_unique<juce::AudioParameterFloat>("Volume",
            "Volume",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f, 1.f),
            0.5f)
    );

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DistortionPluginAudioProcessor();
}
