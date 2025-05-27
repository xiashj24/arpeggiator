#pragma once
#include "PolyArp/Part.h"

// TODO: integrate with global swing and groove

// TODO: increase code reuse by [[fallthrough]];

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

  // arp will reset when step index reach length
  // make sure ARP_MAX_LENGTH * TICKS_PER_STEP do not overflow or weird shit
  // will happen
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
        last_note_number_(DUMMY_NOTE),
        rising_(true),
        current_octave_(0) {
    shuffled_note_list.reserve(128);

    euclid_fill_ = 3;  // pulses
    euclid_length_ = 11;
    euclid_rotate_ = 0;
    euclid_legato_ = false;
  }

  void setType(ArpType type) { type_ = type; }

  void setEuclidPattern(EuclidPattern pattern);

  void setOctave(int octave) { octave_ = octave; }

  void setGate(float gate) { gate_ = gate; }

  void handleNoteOn(juce::MidiMessage noteOn) {
    keyboard_.handleNoteOn(noteOn);
    shuffleNotesWithOctave();
  }

  void handleNoteOff(juce::MidiMessage noteOff) {
    if (!latch_) {
      keyboard_.handleNoteOff(noteOff);

      // automatic stop when all notes are off
      if (keyboard_.getNumNotesPressed() == 0) {
        stop();
      } else {
        shuffleNotesWithOctave();
      }
    }
  }

  int getNumNotesPressed() const { return keyboard_.getNumNotesPressed(); }

  void setLatchOn() { latch_ = true; }

  void setLatchOff() {
    latch_ = false;
    stop();
  }

  void setEuclidLegato(bool enabled) { euclid_legato_ = enabled; }

  void stop() {
    keyboard_.reset();
    Part::setEnabled(false);
    // Part::sendNoteOffNow();
  }

private:
  KeyboardState keyboard_;
  ArpType type_;
  float gate_;
  int octave_;
  int last_note_number_;

  bool rising_;
  bool latch_;

  // euclidean rhythm generator
  int euclid_fill_;  // pulses
  int euclid_length_;
  int euclid_rotate_;
  bool euclid_legato_;

  // implementation
  std::vector<int> shuffled_note_list;  // optimize: minimize about reallocation
  int current_octave_;

  void shuffleNotesWithOctave();

  Note getAdjacentArpNote(bool rise);

  int getStepRenderTick(int index) const override final {
    return index * getTicksPerStep();  // render on beat
  }

  void renderStep(int index) override final;
};
}  // namespace Sequencer
