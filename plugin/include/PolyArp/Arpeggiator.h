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

  enum Rhythm {
    // store rhythm patterns as 16 bit ints or use straightforward data
    // structure?
    Dummy = 0,
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
  }

  void setType(ArpType type) { type_ = type; }

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

  void stop() {
    keyboard_.reset();
    Part::setEnabled(false);
    // Part::sendNoteOffNow();
  }

private:
  KeyboardState keyboard_;
  ArpType type_;
  // Rhythm rhythm_;
  float gate_;
  int octave_;
  int last_note_number_;

  bool rising_;
  bool latch_;

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
