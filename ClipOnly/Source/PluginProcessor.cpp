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
    lastSampleL = 0.0f;
    wasPosClipL = false;
    wasNegClipL = false;

    lastSampleR = 0.0f;
    wasPosClipR = false;
    wasNegClipR = false;
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

    double hardness = 0.7390851332151606;  // x == cos(x)
    double softness = 1.0 - hardness;      // 0.260915
    double refclip = 0.9549925859;         // -0.2dB (is actually -0.4 dB!)

    /*
        How this works:

        The output is delayed by one sample time, so that we can look ahead to
        see if the next sample will clip or will stop clipping.

        (By design, the plug-in does not declare this one sample of latency to
        the host, as this makes it nicer to record through without having to deal
        with latency compensation.)

        The idea is to leave samples that are not clipping alone, and change only
        those samples that go from not-clipping to clipping, and from clipping to
        not-clipping, by "slowing down" the trajectory rather than doing a hard
        transition.

        The transition is rounded by blending between the last known non-clipping
        value and the max level of 0.955 using a linear interpolation, which you
        can also think of as a simple filter. Since we round off the hard corners,
        the result is that the brightness of the high end is reduced when clipping.
    */

    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float inputSampleL = inL[i] * inputLevel;
        float inputSampleR = inR[i] * inputLevel;

		if (inputSampleL >  4.0f) { inputSampleL =  4.0f; }
		if (inputSampleL < -4.0f) { inputSampleL = -4.0f; }
		if (inputSampleR >  4.0f) { inputSampleR =  4.0f; }
		if (inputSampleR < -4.0f) { inputSampleR = -4.0f; }

        // Are we currently clipping?
        if (wasPosClipL) {
            if (inputSampleL < lastSampleL) {
                // The new sample is not clipping, transition towards it.
                lastSampleL = inputSampleL * softness + refclip * hardness;
            } else {
                // Still clipping, keep moving towards to the max level.
                lastSampleL = lastSampleL * hardness + refclip * softness;
            }
            wasPosClipL = false;
        }

        // Look ahead: If the new sample will clip, ignore it and move
        // the current non-clipping value a bit towards the max level.
        if (inputSampleL > refclip) {
            wasPosClipL = true;
            inputSampleL = lastSampleL * softness + refclip * hardness;
        }

        // Are we clipping in the negative direction?
        if (wasNegClipL) {
            if (inputSampleL > lastSampleL) {  // new sample is not clipping
                lastSampleL = inputSampleL * softness - refclip * hardness;
            } else {  // still clipping, still chasing the target
                lastSampleL = lastSampleL * hardness - refclip * softness;
            }
            wasNegClipL = false;
        }
        if (inputSampleL < -refclip) {
            wasNegClipL = true;
            inputSampleL = lastSampleL * softness - refclip * hardness;
        }

        // Same logic for the right channel.
        if (wasPosClipR) {
            if (inputSampleR < lastSampleR) {
                lastSampleR = inputSampleR * softness + refclip * hardness;
            } else {
                lastSampleR = lastSampleR * hardness + refclip * softness;
            }
            wasPosClipR = false;
        }
        if (inputSampleR > refclip) {
            wasPosClipR = true;
            inputSampleR = lastSampleR * softness + refclip * hardness;
        }
        if (wasNegClipR) {
            if (inputSampleR > lastSampleR) {
                lastSampleR = inputSampleR * softness - refclip * hardness;
            } else {
                lastSampleR = lastSampleR * hardness - refclip * softness;
            }
            wasNegClipR = false;
        }
        if (inputSampleR < -refclip) {
            wasNegClipR = true;
            inputSampleR = lastSampleR * softness - refclip * hardness;
        }

        outL[i] = lastSampleL * outputLevel;
        outR[i] = lastSampleR * outputLevel;
        lastSampleL = inputSampleL;
        lastSampleR = inputSampleR;
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
