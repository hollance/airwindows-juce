#include "PluginProcessor.h"

AudioProcessor::AudioProcessor() :
    juce::AudioProcessor(BusesProperties().withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

AudioProcessor::~AudioProcessor()
{
}

void AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resetState();
}

void AudioProcessor::releaseResources()
{
}

void AudioProcessor::reset()
{
    resetState();
}

juce::AudioProcessorParameter* AudioProcessor::getBypassParameter() const
{
    return apvts.getParameter("Bypass");
}

bool AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AudioProcessor::resetState()
{
    lastSampleL = 0.0;
    lastSampleR = 0.0;

    for (int x = 0; x < 16; x++) {
        intermediateL[x] = 0.0;
        intermediateR[x] = 0.0;
    }

    // Used by Airwindows dithering, which I disabled for the JUCE version.
    //fpdL = 1.0; while (fpdL < 16386) fpdL = rand()*UINT32_MAX;
    //fpdR = 1.0; while (fpdR < 16386) fpdR = rand()*UINT32_MAX;
}

void AudioProcessor::update()
{
    // These parameters are not in the original plug-in but are useful for testing.
    bypassed = apvts.getRawParameterValue("Bypass")->load();
    inputLevel = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("Input")->load());
    outputLevel = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("Output")->load());
}

void AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    update();

    if (bypassed) { return; }

    const float* inL = buffer.getReadPointer(0);
    const float* inR = buffer.getReadPointer(1);
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    // Calculate the length of the delay line. Could go into prepareToPlay().
    // At 44.1 and 48 kHz, the delay is only one sample. At higher sampling rates,
    // the delay is longer and so the smoothing takes place over a longer time.
    double overallscale = getSampleRate() / 44100.0;
    int spacing = std::floor(overallscale);
    if (spacing < 1) { spacing = 1; }
    if (spacing > 16) { spacing = 16; }

    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        double inputSampleL = inL[i] * inputLevel;
        double inputSampleR = inR[i] * inputLevel;

        // Used by Airwindows dithering, which I disabled for the JUCE version.
        //if (std::abs(inputSampleL) < 1.18e-23) { inputSampleL = fpdL * 1.18e-17; }
        //if (std::abs(inputSampleR) < 1.18e-23) { inputSampleR = fpdR * 1.18e-17; }

        // Calculate the linear interpolation coefficient that's used to mix
        // inputSample with lastSample. If we're not clipping, this is 1.0 and
        // lastSample is ignored. However, the heavier inputSample clips, the
        // more lastSample is blended in, which smoothens the transition.
        // Think of softSpeed as the look-ahead value for how much to correct
        // the next sample.
        double softSpeed = std::abs(inputSampleL);
        if (softSpeed < 1.0) { softSpeed = 1.0; } else { softSpeed = 1.0 / softSpeed; }

        // Hard clip to -pi/2 and +pi/2 for the sin() waveshaper.
        if (inputSampleL > 1.57079633) { inputSampleL = 1.57079633; }
        if (inputSampleL < -1.57079633) { inputSampleL = -1.57079633; }

        // Apply the waveshaper and scale to the clipping level of -0.4 dB.
        inputSampleL = std::sin(inputSampleL) * 0.9549925859;

        // Blend between the waveshaped input sample and the running value.
        // This only uses lastSample when the input is too loud / clipping.
        inputSampleL = inputSampleL * softSpeed + lastSampleL * (1.0 - softSpeed);

        // As in ClipOnly2, this shifts the incoming sample into the delay line
        // and puts the value that got shifted out into lastSample, so that on
        // the next timestep we'll use that for smoothing. This delay exists so
        // that on higher sampling rates, the high end is not overly bright.
        intermediateL[spacing] = inputSampleL;
        inputSampleL = lastSampleL;
        for (int x = spacing; x > 0; x--) {
            intermediateL[x - 1] = intermediateL[x];
        }
        lastSampleL = intermediateL[0];

        // Same for right channel.
        softSpeed = std::abs(inputSampleR);
        if (softSpeed < 1.0) { softSpeed = 1.0; } else { softSpeed = 1.0 / softSpeed; }
        if (inputSampleR > 1.57079633) { inputSampleR = 1.57079633; }
        if (inputSampleR < -1.57079633) { inputSampleR = -1.57079633; }
        inputSampleR = std::sin(inputSampleR) * 0.9549925859;
        inputSampleR = inputSampleR * softSpeed + lastSampleR * (1.0 - softSpeed);
        intermediateR[spacing] = inputSampleR;
        inputSampleR = lastSampleR;
        for (int x = spacing; x > 0; x--) {
            intermediateR[x - 1] = intermediateR[x];
        }
        lastSampleR = intermediateR[0];

        /*
        // 32 bit stereo floating point dither. Disabled this for the JUCE
        // version, since it's unrelated to the logic of the plug-in itself.
        int expon; frexpf((float)inputSampleL, &expon);
        fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
        inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
        frexpf((float)inputSampleR, &expon);
        fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
        inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
        */

        // At this point, inputSample holds the value that was shifted out
        // of the delay line, so this has been delayed by `spacing` samples.
        outL[i] = inputSampleL * outputLevel;
        outR[i] = inputSampleR * outputLevel;
    }
}

juce::AudioProcessorEditor* AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("Bypass", 1),
        "Bypass",
        false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Input", 1),
        "Input",
        juce::NormalisableRange<float>(-12.0f, 36.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Output", 1),
        "Output",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioProcessor();
}
