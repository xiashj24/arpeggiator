#include "PolyArp/Arpeggiator.h"

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

void Arpeggiator::shuffleNotesWithOctave() {
  shuffled_note_list = keyboard_.getNoteStack();

  auto note_list_copy = shuffled_note_list;

  for (int i = 1; i < octave_; ++i) {
    for (auto& n : note_list_copy) {
      n += 12;
    }
    shuffled_note_list.insert(shuffled_note_list.end(), note_list_copy.begin(),
                              note_list_copy.end());
  }

  RemoveDuplicatesInVector(shuffled_note_list);
  FastShuffle(shuffled_note_list, juce::Random::getSystemRandom());
}

Note Arpeggiator::getAdjacentArpNote(bool repeat_boundary) {
  auto arp_note = (rising_) ? keyboard_.getHigherNote(last_note_number_)
                            : keyboard_.getLowerNote(last_note_number_);

  if (arp_note.isDummy()) {
    // reverse direction if hit boundary
    if (rising_ && current_octave_ == octave_ - 1) {
      rising_ = false;
      if (repeat_boundary) {
        return keyboard_.getHighestNote();
      }
    } else if (!rising_ && current_octave_ == 0) {
      rising_ = true;
      if (repeat_boundary) {
        return keyboard_.getLowestNote();
      }
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
  return arp_note;
}

void Arpeggiator::renderStep(int index) {
  int num_notes_pressed = keyboard_.getNumNotesPressed();
  jassert(num_notes_pressed >= 1);  // otherwise there is nothing to play

  Note arp_note;
  bool repeat_boundary = false;

  switch (type_) {
    // MARK: manual
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

    // MARK: classic
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

    case ArpType::RiseNFall:
      repeat_boundary = true;
      [[fallthrough]];

    case ArpType::RiseFall:
      if (index == 0) {
        arp_note = keyboard_.getLowestNote();
        rising_ = true;
        current_octave_ = 0;
      } else {
        arp_note = getAdjacentArpNote(repeat_boundary);
      }
      break;

    case ArpType::FallNRise:
      repeat_boundary = true;
      [[fallthrough]];

    case ArpType::FallRise:
      if (index == 0) {
        arp_note = keyboard_.getHighestNote();
        rising_ = false;
        current_octave_ = octave_ - 1;
      } else {
        arp_note = getAdjacentArpNote(repeat_boundary);
      }
      break;

    // MARK: random
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
        DBG("index: " << index << " random 2/3 mode");
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

  last_note_number_ = arp_note.number;
  arp_note.number += 12 * current_octave_;
  arp_note.length = gate_;
  // wrap to the same note below 128
  while (arp_note.number >= 128) {
    arp_note.number -= 12;
  }

  renderNote(index, arp_note);
  DBG("index: " << index << " note: " << arp_note.number);
}

}  // namespace Sequencer