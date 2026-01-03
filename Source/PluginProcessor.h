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

struct DS1Parameters
{
    // Parametry BJT
    static constexpr double bjtLowPassFreq  =   591.5;
    static constexpr double bjtHighPassFreq =   111.1e3;
    static constexpr double bjtGain         =   1.0;
    static constexpr double bjtFitA         =   27.075;
    static constexpr double bjtFitB         =   4.083;
    static constexpr double bjtFitC         =   8.820;
    static constexpr double bjtInputBias    = - 0.0;
    static constexpr double bjtEdge         =   0.02845;

    // Parametry OPAMP
    static constexpr double opampMaxAmplitude = 3.52;
    static constexpr double slewRate          = 0.325e6;
    static constexpr double opampRD           = 100e3;
    static constexpr double opampCS           = 1e-6;
    static constexpr double opampRS           = 4.7e3;
    static constexpr double opampCF           = 250e-12;

    // Parametry DIODE CLIPPING
    static constexpr double diodeFitA = 0.57836409;
    static constexpr double diodeFitB = 2.81174573;
    static constexpr double diodeFitC = 0.01271795;
    static constexpr double diodeR    = 2.2e3;
    static constexpr double diodeC    = 0.01e-6;

    // Parametr filtru HP miêdzy stopniami
    static constexpr double interstageHighPassFreq = 3.39;

    static AnalogParameters calculateToneCoefficients(float tone)
    {
        AnalogParameters p;

        p.A = 0.00203456 * (double)tone;
        p.B = 5.3064 - 0.968 * (double)tone;
        p.C = 26800.0 - 20000.0 * (double)tone;
        p.D = 0.0029166016;
        p.E = 23.85952;
        p.F = 33600.0;

        return p;
    }

};

DistortionParameters getDistortionParameters(juce::AudioProcessorValueTreeState& apvts);

struct Biquad
{

    double processSample(double x)
    {
        double y  = x * b0 + x1 * b1 + x2 * b2;
               y -= y1 * a1 + y2 * a2;

        x2 = x1;
        x1 = x;

        y2 = y1;
        y1 = y;

        return y;
    }

    void setCoefficients(double B0, double B1, double B2, double A1, double A2)
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

    double b0{ 0.f }, b1{ 0.f }, b2{ 0.f };
    double            a1{ 0.f }, a2{ 0.f };
    double x1{ 0.f }, x2{ 0.f };
    double y1{ 0.f }, y2{ 0.f };


};

inline void calculateCoefficients(Biquad& filter, AnalogParameters& p, float sampleRate)
{
    double T = 1.0 / (double)sampleRate;

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

    filter.setCoefficients(b0, b1, b2, a1, a2);
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

        if (!juce::approximatelyEqual(newParams.tone, params.tone))
        {
            params.tone = newParams.tone;
            updateToneFilter();
		}

        params.volume = newParams.volume;
    }

    float processSample(float inputsSample)
    {

        double processedSample = processBJT((double)inputsSample);


        processedSample = processIHPF(processedSample);


        processedSample = processOpAmp(processedSample);


        processedSample = processClipper(processedSample);


        processedSample = processTone(processedSample);


        float outputSample = processedSample * (double)params.volume;


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

        bjt.reset();
        opamp.reset();
        rc.reset();
        tone.reset();

        updateConstFilters();
        updateOpAmpFilter();
        updateToneFilter();

        opampMaxDelta = DS1Parameters::slewRate / sampleRate;
    }


private:
    DistortionParameters params;
    Biquad bjt, opamp, rc, tone, interstage;
    AnalogParameters bjtParams, opampParams, rcParams, toneParams, interstageParams;

    const float bjtGain = std::pow(10, 36.f / 20.f);
    //const float aDiode = 0.405;
    //const float bDiode = 3.178;
    const double pi = 3.14159265359;

    double sampleRate;

    double opampMaxDelta;
    double opampLastOutput = 0.0;
	const double bjtConst = DS1Parameters::bjtFitB * std::exp(DS1Parameters::bjtFitA * DS1Parameters::bjtInputBias);

    static inline double sign(double x)
    {
		return (double)((x > 0.0) - (x < 0.0));
    }

    double processBJT(double x)
    {
        double y = bjt.processSample(x);

        double a = DS1Parameters::bjtFitA;
        double b = DS1Parameters::bjtFitB;
        double c = DS1Parameters::bjtFitC;
        double edge = DS1Parameters::bjtEdge;
        double ib = DS1Parameters::bjtInputBias;

        y = y < edge - ib ?
            (bjtConst * (1 - std::exp(a * y))) :
            (bjtConst - c);

        return y;
    }

    double processIHPF(double x)
    {
        double y = interstage.processSample(x);
        return y;
    }

    double processOpAmp(double x)
    {
        double y = opamp.processSample(x);

        double delta = y - opampLastOutput;
        if(std::abs(delta) > opampMaxDelta)
        {
            if(delta > 0)
                y = opampLastOutput + opampMaxDelta;
            else
                y = opampLastOutput - opampMaxDelta;
		}

        if(std::abs(y) > DS1Parameters::opampMaxAmplitude)
        {
            y = y > 0 ? DS1Parameters::opampMaxAmplitude : -DS1Parameters::opampMaxAmplitude;
        }

		opampLastOutput = y;

        return y;
    }

    double processClipper(double x)
    {
		double a = DS1Parameters::diodeFitA;
		double b = DS1Parameters::diodeFitB;
		double c = DS1Parameters::diodeFitC;

        double y = a * sign(x) * (1 - std::exp(-std::abs(b * x)) + c * std::abs(x));

        y = rc.processSample(y);

        return y;
    }

    double processTone(double x)
    {
        double y = tone.processSample(x);

        return y;
    }

    void updateConstFilters()
    {   
        // BJT stage
        double w1 = 2 * pi * DS1Parameters::bjtLowPassFreq;
        double w2 = 2 * pi * DS1Parameters::bjtHighPassFreq;

        bjtParams.A = 0.f;
        bjtParams.B = DS1Parameters::bjtGain * w2;
        bjtParams.C = 0.f;

        bjtParams.D = 1.f;
        bjtParams.E = w1 + w2;
        bjtParams.F = w1 * w2;

        calculateCoefficients(bjt, bjtParams, sampleRate);

		// High pass between BJT and OpAmp
        double w = 2 * pi * DS1Parameters::interstageHighPassFreq;
        interstageParams.B = 1.f;
		interstageParams.E = 1.f;
		interstageParams.F = w;

		calculateCoefficients(interstage, interstageParams, sampleRate);

        // RC stage
        rcParams.C = 1.f;
        rcParams.E = DS1Parameters::diodeR * DS1Parameters::diodeC;
        rcParams.F = 1.f;

        calculateCoefficients(rc, rcParams, sampleRate);
    }

    void updateOpAmpFilter()
    {
        double d = (double)params.gain;
        double RD = DS1Parameters::opampRD;
		double RS = DS1Parameters::opampRS;
		double CS = DS1Parameters::opampCS;
		double CF = DS1Parameters::opampCF;

        opampParams.A = CF * d * RD * ((1 - d) * RD + RS);
        opampParams.B = RD + RS + CF / CS * d * RD;
        opampParams.C = 1.f / CS;

        opampParams.D = CF * d * RD * ((1 - d) * RD + RS);
        opampParams.E = (1 - d) * RD + RS + CF / CS * d * RD;
		opampParams.F = 1.f / CS;
        
        calculateCoefficients(opamp, opampParams, sampleRate);
    }

    void updateToneFilter()
    {
		toneParams = DS1Parameters::calculateToneCoefficients(params.tone);
		calculateCoefficients(tone, toneParams, sampleRate);
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
