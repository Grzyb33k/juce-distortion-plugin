/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class DistortionPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    DistortionPluginAudioProcessorEditor (DistortionPluginAudioProcessor&);
    ~DistortionPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    juce::Slider distortionSlider, toneSlider, volumeSlider;
	juce::Label distortionLabel, toneLabel, volumeLabel, infoLabel;

	using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
	std::unique_ptr<Attachment> distortionAttachment;
	std::unique_ptr<Attachment> toneAttachment;
	std::unique_ptr<Attachment> volumeAttachment;

    DistortionPluginAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionPluginAudioProcessorEditor)
};
