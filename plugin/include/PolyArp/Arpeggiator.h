#pragma once
#include "PolyArp/Part.h"
#include "PolyArp/KeyboardState.h"

namespace Sequencer {

class Arpeggiator : public Part {
public:
  enum class ArpType {
    Manual,
    // classic
    Rise,
    Fall,
    RiseFall,
    RiseNFall,
    FallRise,
    FallNRise,
    // random
    Shuffle,
    Walk,
    Random,
    RandomTwo,
    RandomThree,
    Chord,
    Gacha,
  };

  enum class VelocityMode {
    Manual,
    Average,
    Last,
    Fixed,
  };

  enum class EuclidPattern {
    Off,
    _15_16,
    _13_14,
    _12_13,
    _11_12,
    _10_11,
    _9_10,
    _8_9,
    _7_8,
    _13_15,
    _6_7,
    _11_13,
    _5_6,
    _9_11,
    _13_16,
    _4_5,
    _11_14,
    _7_9,
    _10_13,
    _3_4,
    _11_15,
    _8_11,
    _5_7,
    _7_10,
    _9_13,
    _11_16,
    _9_14,
    _7_11,
    _5_8,
    _8_13,
    _3_5,
    _7_12,
    _4_7,
    _9_16,
    _5_9,
    _6_11,
    _7_13,
    _8_15,
    _7_15,
    _6_13,
    _5_11,
    _4_9,
    _7_16,
    _3_7,
    _5_12,
    _2_5,
    _5_13,
    _3_8,
    _4_11,
    _5_14,
    _5_16,
    _4_13,
    _3_10,
    _2_7,
    _3_11,
    _4_15,
    _3_13,
    _2_9,
    _3_14,
    _3_16,
    _2_11,
    _2_13,
    _2_15
  };

  // step index will wrap to 0 after reaching ARP_MAX_LENGTH
  static constexpr int ARP_MAX_LENGTH = 65536;

  Arpeggiator(int channel,
              int length = ARP_MAX_LENGTH,
              Resolution resolution = _8th,
              ArpType type = ArpType::Rise,
              float gate = DEFAULT_LENGTH,
              int octave = 1)
      : Part(channel, length, resolution),
        type_(type),
        gate_(gate),
        octave_(octave),
        velocityMode_(VelocityMode::Manual),
        fixedVelocity_(DEFAULT_VELOCITY),
        euclidLegato_(false),
        patternLength_(16),
        currentOctave_(0),
        interval_(0),
        lastNote_(DUMMY_NOTE),
        rising_(true) {
    stop();
    shuffledNoteList_.reserve(128);
    setFixedVelocity(100);
  }

  void setType(ArpType type) { type_ = type; }

  void setEuclidPattern(EuclidPattern pattern);

  void setOctave(int octave) {
    if (octave_ != octave) {
      octave_ = octave;
      shuffleNotesWithOctave();
      // generateRandomPatternWithOctave();
    }
  }

  void setGate(float gate) { gate_ = gate; }

  void setVelocityMode(VelocityMode mode) { velocityMode_ = mode; }
  void setFixedVelocity(int velocity) { fixedVelocity_ = velocity; }

  void handleNoteOn(juce::MidiMessage noteOn) {
    keyboard_.handleNoteOn(noteOn);
    shuffleNotesWithOctave();
    generateRandomPatternWithOctave();
  }

  void handleNoteOff(juce::MidiMessage noteOff) {
    keyboard_.handleNoteOff(noteOff);
    // automatically stop when all notes are off
    if (keyboard_.getNumNotesPressed() == 0) {
      stop();
    } else {
      shuffleNotesWithOctave();
      // removeNoteFromPattern(noteOff.getNoteNumber());
      generateRandomPatternWithOctave();
    }
  }

  // mute but keep ticking to recall note off
  // note: calling this function has no effect if arp is not running
  void stop(bool immediateNoteOff = false) {
    keyboard_.reset();

    // if (!isMuted()) {
    setMuted(true);
    if (immediateNoteOff) {
      sendNoteOffNow();
    }
    // }
  }

  void setEuclidLegato(bool enabled) { euclidLegato_ = enabled; }

  // will restart from 0 if already running
  // no effect when no notes are pressed
  void start() {
    if (keyboard_.getNumNotesPressed() > 0) {
      reset();
      setMuted(false);
    }
  }

  void setTransposeInterval(int semitones) { interval_ = semitones; }

  void setPatternLength(int length) {
    length = std::clamp(length, 1, 16);

    patternLength_ = length;
  }

  // bool isRunning() const { return isMuted(); }

private:
  KeyboardState keyboard_;
  ArpType type_;
  float gate_;
  int octave_;
  VelocityMode velocityMode_;
  int fixedVelocity_;

  int getArpVelocity(int note) const {
    switch (velocityMode_) {
      case VelocityMode::Manual:
        return keyboard_.getVelocityForNote(note);
        break;
      case VelocityMode::Average:
        return keyboard_.getAverageVelocity();
        break;
      case VelocityMode::Last:
        return keyboard_.getLatestVelocity();
        break;
      case VelocityMode::Fixed:
        return fixedVelocity_;
        break;
      default:
        return DEFAULT_VELOCITY;  // should not happen
        break;
    }
  }

  // euclidean rhythm generator
  int euclidFill_;  // pulses
  int euclidLength_;
  int euclidRotate_;
  bool euclidLegato_;

  // implementation
  std::vector<int> shuffledNoteList_;  // optimize: minimize reallocation
  std::array<int, 16> notePattern_;
  std::array<int, 16> octavePattern_;
  int patternLength_;
  int currentOctave_;
  int interval_;
  int lastNote_;
  bool rising_;

  void shuffleNotesWithOctave();

  void generateRandomPatternWithOctave();

  // void removeNoteFromPattern(int note) {
  //   for (auto& n : notePattern_) {
  //     if (n == note) {
  //       n = DUMMY_NOTE;
  //     }
  //   }
  // }

  int getAdjacentArpNote(bool repeat_boundary);

  int getStepRenderTick(int index) const override final {
    return index * getTicksPerStep();  // render on beat
  }

  void renderArpNote(int index, int note_number);

  void renderStep(int index) override final;
};
}  // namespace Sequencer
