#pragma once

// TODO: make these constexpr
#define DEFAULT_NOTE 60  // C3
#define DUMMY_NOTE -1
#define DISABLED_NOTE 20  // note: -1?
#define MIN_NOTE (DISABLED_NOTE + 1)
#define MAX_NOTE 127
#define DEFAULT_VELOCITY 100  // 1..127 since 0 is the same as NoteOff
#define DEFAULT_LENGTH 0.75f  // gate

namespace Sequencer {

static int WrapNoteIntoValidRange(int note_number) {
  if (note_number < MIN_NOTE) {
    while (note_number < MIN_NOTE) {
      note_number += 12;
    }
  } else if (note_number > MAX_NOTE) {
    while (note_number > MAX_NOTE) {
      note_number -= 12;
    }
  }
  return note_number;
}

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

  Note transposed(int semitones) const {
    Note result = *this;
    if (result.number != DISABLED_NOTE) {
      result.number = WrapNoteIntoValidRange(result.number + semitones);
    }
    return result;
  }
};

}  // namespace Sequencer