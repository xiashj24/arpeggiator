#include "PolyArp/PluginProcessor.h"
#include "PolyArp/PluginEditor.h"

#define HIRES_TIMER_INTERVAL_MS 1

namespace audio_plugin {
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      polyarp(arpMidiCollector),
      parameters(*this, &undoManager, "PolyArp", createParameterLayout()),
      lastCallbackTime(0.0),
      bypassed(false) {

  arpTypeParam = parameters.getRawParameterValue("ARP_TYPE");
  arpOctaveParam = parameters.getRawParameterValue("ARP_OCTAVE");
  arpGateParam = parameters.getRawParameterValue("ARP_GATE");
  arpResolutionParam = parameters.getRawParameterValue("ARP_RESOLUTION");
  arpBypassParam = parameters.getRawParameterValue("ARP_BYPASS");

  HighResolutionTimer::startTimer(HIRES_TIMER_INTERVAL_MS);
}

// MARK: parameter layout
juce::AudioProcessorValueTreeState::ParameterLayout
AudioPluginAudioProcessor::createParameterLayout() {
  using namespace juce;
  AudioProcessorValueTreeState::ParameterLayout layout;

  // Arpeggiator Type
  StringArray arpTypeChoices{
      "Manual",    "Rise",         "Fall",    "Rise Fall", "Rise N' Fall",
      "Fall Rise", "Fall N' Rise", "Shuffle", "Walk",      "Random 1",
      "Random 2",  "Random 3",     "Chord"};
  layout.add(std::make_unique<AudioParameterChoice>("ARP_TYPE", "Arp Type",
                                                    arpTypeChoices, 0));

  // Octave (1-4)
  layout.add(
      std::make_unique<AudioParameterInt>("ARP_OCTAVE", "Octave", 1, 4, 1));

  // Gate (0.0 to 2.0)
  layout.add(std::make_unique<AudioParameterFloat>(
      "ARP_GATE", "Gate", NormalisableRange<float>(0.0f, 2.0f, 0.01f),
      DEFAULT_LENGTH));

  // Resolution
  StringArray resolutionChoices{"1/32", "1/16", "1/8",  "1/4",
                                "1/2T", "1/4T", "1/8T", "1/16T"};

  layout.add(std::make_unique<AudioParameterChoice>(
      "ARP_RESOLUTION", "Resolution", resolutionChoices, 2));

  layout.add(
      std::make_unique<juce::AudioParameterBool>("ARP_LATCH", "Latch", false));
  layout.add(std::make_unique<juce::AudioParameterBool>("ARP_BYPASS", "Bypass",
                                                        false));

  return layout;
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {
  HighResolutionTimer::stopTimer();
}

const juce::String AudioPluginAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const {
  return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms() {
  return 1;  // NB: some hosts don't cope very well if you tell them there are 0
             // programs, so this should be at least 1, even if you're not
             // really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() {
  return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index,
                                                  const juce::String& newName) {
  juce::ignoreUnused(index, newName);
}

void AudioPluginAudioProcessor::prepareToPlay(double sampleRate,
                                              int samplesPerBlock) {
  // Use this method as the place to do any pre-playback
  // initialisation that you need..
  juce::ignoreUnused(sampleRate, samplesPerBlock);

  arpMidiCollector.reset(sampleRate);
}

void AudioPluginAudioProcessor::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}

void AudioPluginAudioProcessor::hiResTimerCallback() {
  // MARK: arp logic
  constexpr double deltaTime = HIRES_TIMER_INTERVAL_MS / 1000.0;

  auto arp_type =
      static_cast<Sequencer::Arpeggiator::ArpType>(arpTypeParam->load());
  int octave = static_cast<int>(arpOctaveParam->load());
  float gate = arpGateParam->load();
  auto resolution =
      static_cast<Sequencer::Track::Resolution>(arpResolutionParam->load());
  bool bypass = static_cast<bool>(arpBypassParam->load());

  polyarp.getArp().setType(arp_type);
  polyarp.getArp().setOctave(octave);
  polyarp.getArp().setGate(gate);
  polyarp.getArp().setResolution(resolution);
  this->setBypassed(bypass);

  polyarp.process(deltaTime);
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages) {
  juce::ignoreUnused(midiMessages);

  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  // In case we have more outputs than inputs, this code clears any output
  // channels that didn't contain input data, (because these aren't
  // guaranteed to be empty - they may contain garbage).
  // This is here to avoid people getting screaming feedback
  // when they first compile a plugin, but obviously you don't need to keep
  // this code if your algorithm always overwrites all the output channels.
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  // This is the place where you'd normally do the guts of your plugin's
  // audio processing...
  // Make sure to reset the state if your inner loop is processing
  // the samples and the outer loop is handling the channels.
  // Alternatively, you can process the samples with the channels
  // interleaved by keeping the same state.
  for (int channel = 0; channel < totalNumInputChannels; ++channel) {
    auto* channelData = buffer.getWritePointer(channel);
    juce::ignoreUnused(channelData);
    // ..do something to the data...
  }

  // MARK: process MIDI

  if (!bypassed) {
    for (const auto metadata : midiMessages) {
      auto message = metadata.getMessage();
      auto time_stamp_in_seconds =
          message.getTimeStamp() / getSampleRate() + lastCallbackTime;

      if (message.isNoteOn()) {
        message.setTimeStamp(time_stamp_in_seconds);
        polyarp.handleNoteOn(message);
      } else if (message.isNoteOff()) {
        message.setTimeStamp(time_stamp_in_seconds);
        polyarp.handleNoteOff(message);
      }
    }
  }

  lastCallbackTime = juce::Time::getMillisecondCounterHiRes() * 0.001;

  // generate MIDI start/stop/continue messages by querying DAW transport
  // also set bpm
  if (this->wrapperType ==
      juce::AudioProcessor::WrapperType::wrapperType_VST3) {
    if (auto dawPlayHead = getPlayHead()) {
      if (auto positionInfo = dawPlayHead->getPosition()) {
        polyarp.setBpm(positionInfo->getBpm().orFallback(120.0));

        // TODO: start arp here, and make sure arp sync'd to DAW
        // sequencer grid if (positionInfo->getIsPlaying()) {
        //   if (!sequencer.isRunning())
        //     sequencer.start(juce::Time::getMillisecondCounterHiRes() *
        //     0.001);
        // } else {
        //   if (sequencer.isRunning())
        //     sequencer.stop();
        // }
      }
    }
  }

  // discard input MIDI messages if arp is enabled
  if (polyarp.isRunning()) {
    midiMessages.clear();
  }

  // TODO: merge MIDI from seq and keyboard using a separate VoiceAssigner
  // module
  // overwrite MIDI buffer
  arpMidiCollector.removeNextBlockOfMessages(midiMessages, getBlockSize());
  // guiMidiCollector.removeNextBlockOfMessages(midiMessages, getBlockSize());
  // visualize MIDI in all channels and manual trigger
  keyboardState.processNextMidiBuffer(midiMessages, 0, getBlockSize(), true);
}

bool AudioPluginAudioProcessor::hasEditor() const {
  return true;  // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor() {
  return new AudioPluginAudioProcessorEditor(*this);
}

void AudioPluginAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData) {
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
  if (this->wrapperType ==
      juce::AudioProcessor::WrapperType::wrapperType_Standalone)
    return;  // only recall parameters if run inside a DAW
  auto state = parameters.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void* data,
                                                    int sizeInBytes) {
  // You should use this method to restore your parameters from this memory
  // block, whose contents will have been created by the getStateInformation()
  // call.
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get() != nullptr) {
    if (xmlState->hasTagName(parameters.state.getType())) {
      parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
  }
}
}  // namespace audio_plugin

// This creates new instances of the plugin.
// This function definition must be in the global namespace.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new audio_plugin::AudioPluginAudioProcessor();
}
