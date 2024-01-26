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
    wasPosClipL = false;
    wasNegClipL = false;

    lastSampleR = 0.0;
    wasPosClipR = false;
    wasNegClipR = false;

    for (int x = 0; x < 16; x++) {
        intermediateL[x] = 0.0;
        intermediateR[x] = 0.0;
    }
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

    /*
        This works very much like ClipOnly, where samples that don't clip are
        not changed, while edges between non-clipping and clipping are softened
        using a simple interpolation filter.

        The difference is that at higher sampling rates ClipOnly2 uses a longer
        window for softening such transitions.

        The latency at 44.1 or 48 kHz is 1 sample. At higher sampling rates the
        latency is however many samples equals one 44.1k sample, rounded down to
        an integer multiple.
    */

    // Calculate the length of the delay line. Could move this into prepareToPlay().
    double overallscale = getSampleRate() / 44100.0;
    int spacing = std::floor(overallscale);
    if (spacing < 1) { spacing = 1; }
    if (spacing > 16) { spacing = 16; }

    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        double inputSampleL = inL[i] * inputLevel;
        double inputSampleR = inR[i] * inputLevel;

        if (inputSampleL > 4.0) { inputSampleL = 4.0; }
        if (inputSampleL < -4.0) { inputSampleL = -4.0; }

        // The following is identical to ClipOnly. The constants are hardcoded
        // but are the same as before, e.g. 0.9549925859 is the reference level
        // of -0.4 dB and 0.7058208 is `refclip * hardness` from ClipOnly.
        if (wasPosClipL) {
            if (inputSampleL < lastSampleL) {
                lastSampleL = 0.7058208 + inputSampleL * 0.2609148;
            } else {
                lastSampleL = 0.2491717 + lastSampleL * 0.7390851;
            }
            wasPosClipL = false;
        }
        if (inputSampleL > 0.9549925859) {
            wasPosClipL = true;
            inputSampleL = 0.7058208 + lastSampleL * 0.2609148;
        }
        if (wasNegClipL) {
            if (inputSampleL > lastSampleL) {
                lastSampleL = -0.7058208 + inputSampleL * 0.2609148;
            } else {
                lastSampleL = -0.2491717 + lastSampleL * 0.7390851;
            }
            wasNegClipL = false;
        }
        if (inputSampleL < -0.9549925859) {
            wasNegClipL = true;
            inputSampleL = -0.7058208 + lastSampleL * 0.2609148;
        }

        // Shift the incoming sample into the delay line, and put the value that
        // got shifted out into lastSample, so that on the next timestep we'll
        // use that for smoothing. At 44.1 and 48 kHz, ClipOnly2 should give the
        // same output as ClipOnly, since that also uses a delay of one sample.
        // At higher sampling rates, the delay is longer and so the smoothing
        // takes place over a longer time.
        intermediateL[spacing] = inputSampleL;
        inputSampleL = lastSampleL;
        for (int x = spacing; x > 0; x--) {
            intermediateL[x - 1] = intermediateL[x];
        }
        lastSampleL = intermediateL[0];

        // Same logic for the right channel.
        if (inputSampleR > 4.0) { inputSampleR = 4.0; }
        if (inputSampleR < -4.0) { inputSampleR = -4.0; }
        if (wasPosClipR) {
            if (inputSampleR < lastSampleR) {
                lastSampleR = 0.7058208 + inputSampleR * 0.2609148;
            } else {
                lastSampleR = 0.2491717 + lastSampleR * 0.7390851;
            }
            wasPosClipR = false;
        }
        if (inputSampleR > 0.9549925859) {
            wasPosClipR = true;
            inputSampleR = 0.7058208 + lastSampleR * 0.2609148;
        }
        if (wasNegClipR) {
            if (inputSampleR > lastSampleR) {
                lastSampleR = -0.7058208 + inputSampleR * 0.2609148;
            } else {
                lastSampleR = -0.2491717 + lastSampleR * 0.7390851;
            }
            wasNegClipR = false;
        }
        if (inputSampleR < -0.9549925859) {
            wasNegClipR = true;
            inputSampleR = -0.7058208 + lastSampleR * 0.2609148;
        }
        intermediateR[spacing] = inputSampleR;
        inputSampleR = lastSampleR;
        for (int x = spacing; x > 0; x--) {
            intermediateR[x-1] = intermediateR[x];
        }
        lastSampleR = intermediateR[0];

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
