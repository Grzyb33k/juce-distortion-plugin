/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DistortionPluginAudioProcessorEditor::DistortionPluginAudioProcessorEditor (DistortionPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    distortionSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
	distortionSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF45404b));
    distortionSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFFE37830));
    distortionSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE6DDE0));

    toneSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    toneSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF45404b));
    toneSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFFE37830));
    toneSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE6DDE0));

    volumeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
	volumeSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF45404b));
	volumeSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFFE37830));
	volumeSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE6DDE0));

    toneSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    distortionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);


	addAndMakeVisible(distortionSlider);
    addAndMakeVisible(toneSlider);
    addAndMakeVisible(volumeSlider);

    distortionLabel.setText("DIST", juce::dontSendNotification);
    distortionLabel.setJustificationType(juce::Justification::centred);
	distortionLabel.setFont(juce::Font(30.0f, juce::Font::bold));
	distortionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF281E24));

    toneLabel.setText("TONE", juce::dontSendNotification);
    toneLabel.setJustificationType(juce::Justification::centred);
	toneLabel.setFont(juce::Font(30.0f, juce::Font::bold));
	toneLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF281E24));

    volumeLabel.setText("LEVEL", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);
	volumeLabel.setFont(juce::Font(26.0f, juce::Font::bold));
	volumeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF281E24));

	infoLabel.setText("Efekt distortion modelowany na podstawie DS-1", juce::dontSendNotification);
	infoLabel.setJustificationType(juce::Justification::centred);
	infoLabel.setFont(juce::Font(15.0f, juce::Font::italic));
	infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF281E24));

    addAndMakeVisible(distortionLabel);
    addAndMakeVisible(toneLabel);
    addAndMakeVisible(volumeLabel);
	addAndMakeVisible(infoLabel);

    distortionAttachment = std::make_unique<Attachment>(
        p.apvts,
        "Gain",
        distortionSlider
    );

    toneAttachment = std::make_unique<Attachment>(
        p.apvts,
        "Tone",
        toneSlider
    );

    volumeAttachment = std::make_unique<Attachment>(
        p.apvts,
        "Volume",
        volumeSlider
    );


    setSize (450, 300);
}

DistortionPluginAudioProcessorEditor::~DistortionPluginAudioProcessorEditor()
{
}

//==============================================================================
void DistortionPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
	g.fillAll(juce::Colour(0xFFE37830));

    auto drawKnobValue = [&](juce::Slider& slider)
        {
            auto r = slider.getBounds().toFloat();
            juce::String text = juce::String(slider.getValue(), 2);
            g.setColour(juce::Colour(0xFFE6DDE0));
            g.setFont(20.0f);
            g.drawText(text, r.toNearestInt(), juce::Justification::centred);
        };

    drawKnobValue(toneSlider);
    drawKnobValue(distortionSlider);
    drawKnobValue(volumeSlider);
}

void DistortionPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto knobsArea = area.removeFromTop(300);

    const int knobSizeLarge = 150;
    const int knobSizeSmall = 100;
    const int labelHeight = 20;
    const int spacing = 20;

    const int totalWidth = knobSizeLarge + spacing + knobSizeSmall + spacing + knobSizeLarge;
    int startX = (knobsArea.getWidth() - totalWidth) / 2;

    int yKnobTop = 40;
    int yKnobMiddle = yKnobTop + 80;

    toneSlider.setBounds(startX, yKnobTop, knobSizeLarge, knobSizeLarge);
    volumeSlider.setBounds(startX + knobSizeLarge + spacing, yKnobMiddle, knobSizeSmall, knobSizeSmall);
    distortionSlider.setBounds(startX + knobSizeLarge + spacing + knobSizeSmall + spacing, yKnobTop, knobSizeLarge, knobSizeLarge);

    toneLabel.setBounds(toneSlider.getX(), toneSlider.getBottom() + 5, knobSizeLarge, labelHeight);
    distortionLabel.setBounds(distortionSlider.getX(), distortionSlider.getBottom() + 5, knobSizeLarge, labelHeight);

    volumeLabel.setBounds(volumeSlider.getX(), volumeSlider.getY() - labelHeight - 5, knobSizeSmall, labelHeight);

	infoLabel.setBounds(0, getHeight() - 40, getWidth(), 20);
}
