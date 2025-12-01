/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>


struct DistortionParameters
{
    float gain{   0.5 };
    float tone{   0.5 };
    float volume{ 0.5 };
};

struct AnalogParameters
{
    double A{ 0.f }, B{ 0.f }, C{ 0.f };
    double D{ 0.f }, E{ 0.f }, F{ 0.f };
};

DistortionParameters getDistortionParameters(juce::AudioProcessorValueTreeState& apvts);

struct Biquad
{

    float processSample(float x)
    {
        float y  = x * b0 + x1 * b1 + x2 * b2;
              y -= y1 * a1 + y2 * a2;

        x2 = x1;
        x1 = x;

        y2 = y1;
        y1 = y;

        return y;
    }

    void setCoefficients(float B0, float B1, float B2, float A1, float A2)
    {
        b0 = B0; b1 = B1; b2 = B2;
                 a1 = A1; a2 = A2;
    }

    void reset()
    {
        x1 = 0.f; x2 = 0.f;
        y1 = 0.f; y2 = 0.f;
    }

private:

    float b0{ 0.f }, b1{ 0.f }, b2{ 0.f };
    float            a1{ 0.f }, a2{ 0.f };
    float x1{ 0.f }, x2{ 0.f };
    float y1{ 0.f }, y2{ 0.f };


};

void calculateCoefficients(Biquad& filter, AnalogParameters& p, float sampleRate)
{
    double T = 1.0 / sampleRate;

    double b0, b1, b2;
    double a0, a1, a2;

    double Tsq = T * T;

    b0 = 4 * p.A / Tsq + 2 * p.B / T + p.C;
    b1 = 2 * p.C - 8 * p.A / Tsq;
    b2 = p.C + 4 * p.A / Tsq - 2 * p.B / T;

    a0 = 4 * p.D / Tsq + 2 * p.E / T + p.F;
    a1 = 2 * p.F - 8 * p.D / Tsq;
    a2 = p.F + 4 * p.D / Tsq - 2 * p.E / T;

    b0 /= a0;
    b1 /= a0;
    b2 /= a0;

    a1 /= a0;
    a2 /= a0;

    filter.setCoefficients((float)b0, (float)b1, (float)b2, (float)a1, (float)a2);
}

struct DistortionProcessor
{
    DistortionProcessor() = default;

    void setParameters(const DistortionParameters& newParams)
    {
        params = newParams;
    }

    void updateParameters(const DistortionParameters& newParams)
    {
        if (!juce::approximatelyEqual(newParams.gain, params.gain))
        {
            params.gain = newParams.gain;
            updateOpAmpFilter();
        }

        params.tone = newParams.tone;
        params.volume = newParams.volume;
    }

    float processSample(float inputsSample)
    {
        float processedSample = processBJT(inputsSample);

        processedSample = processOpAmp(processedSample);

        processedSample = processClipper(processedSample);

        processedSample = processTone(processedSample);

        float outputSample = processedSample * params.volume;

        return outputSample;
    }

    void processBlock(juce::dsp::AudioBlock<float>& block)
    {
        const auto numCh = block.getNumChannels();
        const auto numS = block.getNumSamples();

        for (size_t ch = 0; ch < numCh; ++ch)
        {
            auto* data = block.getChannelPointer(ch);
            for (size_t n = 0; n < numS; ++n)
            {
                data[n] = processSample(data[n]);
            }
        }
    }


    void prepare(double sampleRate_)
    {
        sampleRate = sampleRate_;

        bjt.    reset();
        opamp.  reset();
        rc.     reset();
        toneLP. reset();
        toneHP. reset();

        updateConstFilters();
        updateOpAmpFilter();
    }
    

private:
    DistortionParameters params;
    Biquad bjt, opamp, rc, toneLP, toneHP;
    AnalogParameters bjtParams, opampParams, rcParams, toneLpParams, toneHpParams;

    const float bjtGain = std::pow(10, 36.f/20.f);
    const float aDiode = 0.405;
    const float bDiode = 3.178;
    const float pi = 3.14159265359f;

    float sampleRate;

    float processBJT(float x)
    {
        float y = bjt.processSample(x);
        return bjtGain * y;
    }

    float processOpAmp(float x)
    {
        float y = opamp.processSample(x);

        y = y > 0 ? 4.55 * std::tanh(y / 4.55) : 4.4 * std::tanh(y / 4.4);

        return y;
    }

    float processClipper(float x)
    {
        float xClipped = aDiode * std::atan(x * bDiode);

        float y = rc.processSample(xClipped);

        return y;
    }

    float processTone(float x)
    {
        float xLP = toneLP.processSample(x);
        float xHP = toneHP.processSample(x);
        float y = (1 - params.tone) * xLP + params.tone * xHP;

        return y;
    }

    void updateConstFilters()
    {   
        // BJT stage
        double w1 = 2 * pi * 3.f;
        double w2 = 2 * pi * 600.f;
        bjtParams.A = 1.f;
        bjtParams.B = 0.f;
        bjtParams.C = 0.f;
        bjtParams.D = 1.f;
        bjtParams.E = w1 + w2;
        bjtParams.F = w1 * w2;

        calculateCoefficients(bjt, bjtParams, sampleRate);

        // RC stage

        double R = 2.2e3;
        double C = 0.01e-6;

        rcParams.C = 1.f;
        rcParams.E = R * C;
        rcParams.F = 1.f;

        calculateCoefficients(rc, rcParams, sampleRate);

        // Tone stage

        double LpR    = 6.8e3;
        double LpC    = 0.1e-6;
        double hpR1   = 2.2e3;
        double hpR2   = 6.8e3;
        double hpC    = 0.022e-6;
        double lpF    = 320.f;
        double hpF    = 1.16e3;
        double hpGain = hpR2 / (hpR1 + hpR2);

        toneLpParams.C = 1.f;
        toneLpParams.E = 1.f / (2.f * pi * lpF);
        toneLpParams.F = 1.f;

        toneHpParams.B = hpGain;
        toneHpParams.E = 1.f;
        toneHpParams.F = 2.f * pi * hpF;

        calculateCoefficients(toneLP, toneLpParams, sampleRate);
        calculateCoefficients(toneHP, toneHpParams, sampleRate);

    }

    void updateOpAmpFilter()
    {
        float dist = params.gain;

        double Rt = (double)dist * 100e3;
        double Rb = (1.f - (double)dist) * 100e3 + 4.7e3;
        double Cz = 1e-6;
        double Cc = 250e-12;
        double a = 1 / (Rt * Cc);
        double b = 1 / (Rb * Cz);
        double c = 1 / (Rb * Cc);

        opampParams.A = 1.f;
        opampParams.B = a + b + c;
        opampParams.C = a * b;
        opampParams.D = 1.f;
        opampParams.E = a + b;
        opampParams.F = a * b;

        calculateCoefficients(opamp, opampParams, sampleRate);
    }
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
    std::array<DistortionProcessor, 2> distortionEngine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionPluginAudioProcessor)
};
