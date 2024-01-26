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

bool AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AudioProcessor::resetState()
{
    gain = 1.0f;
}

void AudioProcessor::update()
{
    int bits = apvts.getRawParameterValue("BitShift")->load();
    switch (bits) {
        case -16: gain = 0.0000152587890625; break;
        case -15: gain = 0.000030517578125; break;
        case -14: gain = 0.00006103515625; break;
        case -13: gain = 0.0001220703125; break;
        case -12: gain = 0.000244140625; break;
        case -11: gain = 0.00048828125; break;
        case -10: gain = 0.0009765625; break;
        case -9: gain = 0.001953125; break;
        case -8: gain = 0.00390625; break;
        case -7: gain = 0.0078125; break;
        case -6: gain = 0.015625; break;
        case -5: gain = 0.03125; break;
        case -4: gain = 0.0625; break;
        case -3: gain = 0.125; break;
        case -2: gain = 0.25; break;
        case -1: gain = 0.5; break;
        case 0: gain = 1.0; break;
        case 1: gain = 2.0; break;
        case 2: gain = 4.0; break;
        case 3: gain = 8.0; break;
        case 4: gain = 16.0; break;
        case 5: gain = 32.0; break;
        case 6: gain = 64.0; break;
        case 7: gain = 128.0; break;
        case 8: gain = 256.0; break;
        case 9: gain = 512.0; break;
        case 10: gain = 1024.0; break;
        case 11: gain = 2048.0; break;
        case 12: gain = 4096.0; break;
        case 13: gain = 8192.0; break;
        case 14: gain = 16384.0; break;
        case 15: gain = 32768.0; break;
        case 16: gain = 65536.0; break;
    }
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

    const float* inL = buffer.getReadPointer(0);
    const float* inR = buffer.getReadPointer(1);
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        outL[i] = inL[i] * gain;
        outR[i] = inR[i] * gain;
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

    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("BitShift", 1),
        "BitShift",
        -16, 16, 0,
        juce::AudioParameterIntAttributes().withLabel("bits")));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioProcessor();
}
