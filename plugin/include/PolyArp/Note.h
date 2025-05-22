#pragma once

#define DEFAULT_NOTE 60  // C4
#define DUMMY_NOTE -1
#define DISABLED_NOTE 20      // note: better to use -1?
#define DEFAULT_VELOCITY 100  // 1..127 since 0 is the same as NoteOff
#define DEFAULT_LENGTH 0.75f  // gate

namespace Sequencer {

struct Note {
  int number = DEFAULT_NOTE;  // <= 20 indicates disabled
  int velocity = DEFAULT_VELOCITY;
  float offset = 0.f;             // in fractional steps
  float length = DEFAULT_LENGTH;  // in fractional steps

  void reset() {
    number = DISABLED_NOTE;
    velocity = DEFAULT_VELOCITY;
    offset = 0.f;  // relative the step index
    length = DEFAULT_LENGTH;
  }

  static Note dummy() { return {.number = DUMMY_NOTE}; }

  bool isDummy() const { return number == DUMMY_NOTE; }
};

}  // namespace Sequencer