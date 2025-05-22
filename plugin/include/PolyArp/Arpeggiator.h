// poly track reuse most of the implementations for time-keeping from mono
// track, but renders the step differently

// note: maybe it makes sense to inherit from Track to create mono track and
// make track Abstract
#pragma once
#include "PolyArp/Track.h"

/*
  gate time -> note.length

  -> inside renderStep
  play mode
  octave range
  rhythm pattern (Euclidean like)

  *arp rate -> sequencer step rate
  *resync to master length: same as nono sequencer

  snap to grid?
  retrigger by new note?

*/

// TODO: make this a constexpr
#define ARP_MAX_LENGTH \
  65535  // arp will reset when all keys are off or step index reaches this
         // number, which is still possible for a very long arp session so maybe
         // use max<int32_t> (if somebody is bothered)

// TODO: difference velocity modes: as played(default), average, latest, fixed
// integrate with global swing and groove

// TODO: increase code reuse by [[fallthrough]];

// TODO: reshuffle when octave changes

// TODO: sync to master length

namespace Sequencer {

inline int positive_modulo(int i, int n) {
  return (i % n + n) % n;
}

template <class T>
void RemoveDuplicatesInVector(std::vector<T>& vec) {
  std::sort(vec.begin(), vec.end());
  vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

template <typename T>
void FastShuffle(std::vector<T>& array, juce::Random& rng) {
  for (int i = static_cast<int>(array.size()) - 1; i > 0; --i) {
    int j = rng.nextInt(i + 1);  // 0 ≤ j ≤ i
    std::swap(array[static_cast<size_t>(i)], array[static_cast<size_t>(j)]);
  }
}

// TODO: make this private inheritance
class Arpeggiator : public Track {
public:
  enum class ArpType {
    Manual,

    Rise,
    Fall,
    RiseFall,
    RiseNFall,
    FallRise,
    FallNRise,

    // Interleave, // too cumbersome for multi octave
    Shuffle,
    Walk,
    Random,
    RandomTwo,
    RandomThree,
    Chord,
    // Gacha,    // Pattern in Arturia (low priority)

  };

  enum Rhythm {
    // store rhythm patterns as 16 bit ints or use straightforward data
    // structure?
    Dummy = 0,
  };

  Arpeggiator(int channel,
              int length = ARP_MAX_LENGTH,
              Resolution resolution = _8th,
              ArpType type = ArpType::Rise,
              float gate = DEFAULT_LENGTH,
              int octave = 1)
      : Track(channel, length, resolution),
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

  void setResolutionDeferred(Resolution resolution) {
    arp_resolution_ = resolution;
  }

  void handleNoteOn(juce::MidiMessage noteOn) {
    keyboard_.handleNoteOn(noteOn);
    // re-shuffle when keyboard state changes
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

  void setLatchOn() { latch_ = true; }

  void setLatchOff() {
    latch_ = false;
    stop();
  }

  void stop() {
    keyboard_.reset();
    Track::setEnabled(false);
    Track::sendNoteOffNow();
    // shuffled_note_list.clear();

    Track::setResolution(arp_resolution_); // write thru on stop
  }

private:
  KeyboardState keyboard_;
  ArpType type_;
  // Rhythm rhythm_;
  float gate_;  // 0..200%
  int octave_;  // 0..4.0 (fractional?)
  int last_note_number_;

  bool rising_;  // flag for bouncing arp types (RiseNFall, etc.)
  bool latch_;

  std::vector<int> shuffled_note_list;

  int current_octave_;

  Resolution arp_resolution_;

  // reset by new note?
  // reset when mode changes?

  void shuffleNotesWithOctave() {
    shuffled_note_list = keyboard_.getNoteStack();

    auto note_list_copy = shuffled_note_list;

    for (int i = 1; i < octave_; ++i) {
      for (auto& n : note_list_copy) {
        n += 12;
      }
      shuffled_note_list.insert(shuffled_note_list.end(),
                                note_list_copy.begin(), note_list_copy.end());
    }

    RemoveDuplicatesInVector(shuffled_note_list);
    FastShuffle(shuffled_note_list, juce::Random::getSystemRandom());
  }

  // only render on beat
  int getStepRenderTick(int index) const override final {
    return index * getTicksPerStep();
  }

  // int getArpStepIndex() const; // for GUI

  void renderStep(int index) override final {
    int num_notes_pressed = keyboard_.getNumNotesPressed();
    jassert(num_notes_pressed >= 1);  // otherwise there is nothing to play

    Note arp_note;

    switch (type_) {
      case ArpType::Manual:
        if (index == 0) {
          arp_note = keyboard_.getEarliestNote();
          current_octave_ = 0;
        } else {
          arp_note = keyboard_.getNextNote(last_note_number_);

          if (arp_note.isDummy()) {
            arp_note = keyboard_.getEarliestNote();
            current_octave_ = (current_octave_ + 1) % octave_;
          }
        }
        break;

      case ArpType::Rise:
        if (index == 0) {
          arp_note = keyboard_.getLowestNote();
          current_octave_ = 0;
        } else {
          arp_note = keyboard_.getHigherNote(last_note_number_);
          if (arp_note.isDummy()) {
            arp_note = keyboard_.getLowestNote();
            current_octave_ = (current_octave_ + 1) % octave_;
          }
        }
        break;

      case ArpType::Fall:
        if (index == 0) {
          arp_note = keyboard_.getHighestNote();
          current_octave_ = octave_ - 1;
        } else {
          arp_note = keyboard_.getLowerNote(last_note_number_);

          if (arp_note.isDummy()) {
            arp_note = keyboard_.getHighestNote();
            current_octave_ = positive_modulo(current_octave_ - 1, octave_);
          }
        }
        break;

      case ArpType::RiseFall:
        if (index == 0) {
          arp_note = keyboard_.getLowestNote();
          rising_ = true;
          current_octave_ = 0;
        } else {
          arp_note = (rising_) ? keyboard_.getHigherNote(last_note_number_)
                               : keyboard_.getLowerNote(last_note_number_);
        }

        if (arp_note.isDummy()) {
          // reverse direction if hit boundary
          if (rising_ && current_octave_ == octave_ - 1) {
            rising_ = false;
          } else if (!rising_ && current_octave_ == 0) {
            rising_ = true;
          }

          // try again
          if (rising_) {
            arp_note = keyboard_.getHigherNote(last_note_number_);

            if (arp_note.isDummy()) {
              arp_note = keyboard_.getLowestNote();
              current_octave_ = (current_octave_ + 1) % octave_;
            }
          } else {
            arp_note = keyboard_.getLowerNote(last_note_number_);
            if (arp_note.isDummy()) {
              arp_note = keyboard_.getHighestNote();
              current_octave_ = positive_modulo(current_octave_ - 1, octave_);
            }
          }
        }

        break;

      case ArpType::RiseNFall:

        if (index == 0) {
          arp_note = keyboard_.getLowestNote();
          rising_ = true;
          current_octave_ = 0;
        } else {
          arp_note = (rising_) ? keyboard_.getHigherNote(last_note_number_)
                               : keyboard_.getLowerNote(last_note_number_);
        }

        if (arp_note.isDummy()) {
          // reverse direction if hit boundary
          if (rising_ && current_octave_ == octave_ - 1) {
            rising_ = false;
            arp_note = keyboard_.getHighestNote();
            break;
          } else if (!rising_ && current_octave_ == 0) {
            rising_ = true;
            arp_note = keyboard_.getLowestNote();
            break;
          }

          // try again
          if (rising_) {
            arp_note = keyboard_.getHigherNote(last_note_number_);

            if (arp_note.isDummy()) {
              arp_note = keyboard_.getLowestNote();
              current_octave_ = (current_octave_ + 1) % octave_;
            }
          } else {
            arp_note = keyboard_.getLowerNote(last_note_number_);
            if (arp_note.isDummy()) {
              arp_note = keyboard_.getHighestNote();
              current_octave_ = positive_modulo(current_octave_ - 1, octave_);
            }
          }
        }

        break;

      case ArpType::FallRise:
        if (index == 0) {
          arp_note = keyboard_.getHighestNote();
          rising_ = false;
          current_octave_ = octave_ - 1;
        } else {
          arp_note = (rising_) ? keyboard_.getHigherNote(last_note_number_)
                               : keyboard_.getLowerNote(last_note_number_);
        }

        if (arp_note.isDummy()) {
          // reverse direction if hit boundary
          if (rising_ && current_octave_ == octave_ - 1) {
            rising_ = false;
          } else if (!rising_ && current_octave_ == 0) {
            rising_ = true;
          }

          // try again
          if (rising_) {
            arp_note = keyboard_.getHigherNote(last_note_number_);

            if (arp_note.isDummy()) {
              arp_note = keyboard_.getLowestNote();
              current_octave_ = (current_octave_ + 1) % octave_;
            }
          } else {
            arp_note = keyboard_.getLowerNote(last_note_number_);
            if (arp_note.isDummy()) {
              arp_note = keyboard_.getHighestNote();
              current_octave_ = positive_modulo(current_octave_ - 1, octave_);
            }
          }
        }
        break;

      case ArpType::FallNRise:
        if (index == 0) {
          arp_note = keyboard_.getHighestNote();
          rising_ = false;
          current_octave_ = octave_ - 1;
        } else {
          arp_note = (rising_) ? keyboard_.getHigherNote(last_note_number_)
                               : keyboard_.getLowerNote(last_note_number_);
        }

        if (arp_note.isDummy()) {
          // reverse direction if hit boundary
          if (rising_ && current_octave_ == octave_ - 1) {
            rising_ = false;
            arp_note = keyboard_.getHighestNote();
            break;
          } else if (!rising_ && current_octave_ == 0) {
            rising_ = true;
            arp_note = keyboard_.getLowestNote();
            break;
          }

          // try again
          if (rising_) {
            arp_note = keyboard_.getHigherNote(last_note_number_);

            if (arp_note.isDummy()) {
              arp_note = keyboard_.getLowestNote();
              current_octave_ = (current_octave_ + 1) % octave_;
            }
          } else {
            arp_note = keyboard_.getLowerNote(last_note_number_);
            if (arp_note.isDummy()) {
              arp_note = keyboard_.getHighestNote();
              current_octave_ = positive_modulo(current_octave_ - 1, octave_);
            }
          }
        }
        break;

        // case ArpType::Interleave:

        //   if (index == 0) {
        //     arp_note = keyboard_.getLowestNote();
        //     odd_ = true;
        //   } else {
        //     arp_note = keyboard_.getHigherNote(last_note_number_);

        //     if (arp_note.isDummy()) {
        //       odd_ = !odd_;
        //       arp_note = keyboard_.getLowestNote();
        //       if (!odd_) {
        //         arp_note = keyboard_.getHigherNoteWrap(arp_note.number);
        //       }

        //     } else {
        //       arp_note = keyboard_.getHigherNote(arp_note.number);
        //       if (arp_note.isDummy()) {
        //         odd_ = !odd_;
        //         arp_note = keyboard_.getLowestNote();
        //         if (!odd_) {
        //           arp_note = keyboard_.getHigherNoteWrap(arp_note.number);
        //         }
        //       }
        //     }
        //   }

        //   break;

      case ArpType::Random:
        arp_note = keyboard_.getRandomNote();
        current_octave_ = juce::Random::getSystemRandom().nextInt(octave_);

        break;

      case ArpType::Shuffle:

        // re-shuffle on every loop start
        if (index % (num_notes_pressed * octave_) == 0) {
          shuffleNotesWithOctave();
        }

        arp_note.number = shuffled_note_list[static_cast<size_t>(
            index % (num_notes_pressed * octave_))];
        arp_note.velocity = keyboard_.getAverageVelocity();

        break;

      case ArpType::Walk:
        if (index == 0) {
          arp_note = keyboard_.getRandomNote();
          current_octave_ = juce::Random::getSystemRandom().nextInt(octave_);
        } else if (juce::Random::getSystemRandom().nextBool()) {
          arp_note = keyboard_.getHigherNote(last_note_number_);
          if (arp_note.isDummy()) {
            arp_note = keyboard_.getLowestNote();
            current_octave_ = (current_octave_ + 1) % octave_;
          }
        } else {
          arp_note = keyboard_.getLowerNote(last_note_number_);
          if (arp_note.isDummy()) {
            arp_note = keyboard_.getHighestNote();
            current_octave_ = positive_modulo(current_octave_ - 1, octave_);
          }
        }
        break;

      case ArpType::RandomThree:
        if (num_notes_pressed > 2 || octave_ > 2 ||
            (num_notes_pressed == 2 && octave_ == 2)) {
          // re-shuffle on every step
          shuffleNotesWithOctave();

          for (size_t i = 0; i < 3; ++i) {
            arp_note.number = shuffled_note_list[i];
            arp_note.length = gate_;
            arp_note.velocity = keyboard_.getAverageVelocity();

            renderNote(index, arp_note);
          }
          DBG("index: " << index << " random three");
          return;
        } else
          [[fallthrough]];

      case ArpType::RandomTwo:
        jassert(octave_ >= 1);
        if (num_notes_pressed == 1 && octave_ == 1) {
          arp_note = keyboard_.getLowestNote();
        } else {
          // re-shuffle on every step
          shuffleNotesWithOctave();

          for (size_t i = 0; i < 2; ++i) {
            arp_note.number = shuffled_note_list[i];
            arp_note.length = gate_;
            arp_note.velocity = keyboard_.getAverageVelocity();

            renderNote(index, arp_note);
          }
          DBG("index: " << index << " random two");
          return;
        }
        break;

      case ArpType::Chord:
        for (int n : shuffled_note_list) {
          arp_note.number = n;
          arp_note.length = gate_;
          arp_note.velocity = keyboard_.getAverageVelocity();

          renderNote(index, arp_note);
        }
        DBG("index: " << index << " chord");
        return;
        break;
    }

    DBG("Current octave: " << current_octave_);

    last_note_number_ = arp_note.number;
    arp_note.number += 12 * current_octave_;
    arp_note.length = gate_;
    if (arp_note.number < 128) {
      renderNote(index, arp_note);
      DBG("index: " << index << " note: " << arp_note.number);
    } else {
      DBG("index: " << index << " note number larger than 127!");
    }
  }
};
}  // namespace Sequencer
