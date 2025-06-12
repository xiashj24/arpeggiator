#include "PolyArp/Arpeggiator.h"

namespace Sequencer {

// reference: https://paulbatchelor.github.io/sndkit/euclid/
inline bool euclid_simple(int p, int n, int r, int i) {
  return (((p * (i + r)) % n) + p) >= n;
}

// int euclid_simple_2(int p, int n, int r, int i) {
//   return (p * (i + r)) % n < p;
// }

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

  // check euclid
  int euclid_index = index % euclid_length_;
  if (!euclid_simple(euclid_fill_, euclid_length_, euclid_rotate_,
                     euclid_index))
    return;

  float note_length = gate_;
  // euclid legato
  if (euclid_legato_) {
    euclid_index = (euclid_index + 1) % euclid_length_;
    while (!euclid_simple(euclid_fill_, euclid_length_, euclid_rotate_,
                          euclid_index)) {
      note_length += 1.f;
      euclid_index = (euclid_index + 1) % euclid_length_;
    }
  }

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
      DBG("index: " << index << " random 1 mode");
      break;

    case ArpType::Shuffle:
      // re-shuffle on every loop start
      if (index % (num_notes_pressed * octave_) == 0) {
        shuffleNotesWithOctave();
        // reshuffle if shuffled_note_list[0] == last_note_number?
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
          arp_note.length = note_length;
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
          arp_note.length = note_length;
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
        arp_note.length = note_length;
        arp_note.velocity = keyboard_.getAverageVelocity();

        renderNote(index, arp_note);
      }
      DBG("index: " << index << " chord");
      return;
      break;
  }

  last_note_number_ = arp_note.number;
  arp_note.number += 12 * current_octave_;
  arp_note.length = note_length;
  // wrap to the same note below 128
  while (arp_note.number >= 128) {
    arp_note.number -= 12;
  }

  renderNote(index, arp_note);
  DBG("index: " << index << " note: " << arp_note.number);
}

// MARK: euclid
void Arpeggiator::setEuclidPattern(EuclidPattern pattern) {
  switch (pattern) {
    case EuclidPattern::Off:
      euclid_fill_ = 1;
      euclid_length_ = 1;
      break;
    case EuclidPattern::_15_16:
      euclid_fill_ = 15;
      euclid_length_ = 16;
      break;
    case EuclidPattern::_13_14:
      euclid_fill_ = 13;
      euclid_length_ = 14;
      break;
    case EuclidPattern::_12_13:
      euclid_fill_ = 12;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_11_12:
      euclid_fill_ = 11;
      euclid_length_ = 12;
      break;
    case EuclidPattern::_10_11:
      euclid_fill_ = 10;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_9_10:
      euclid_fill_ = 9;
      euclid_length_ = 10;
      break;
    case EuclidPattern::_8_9:
      euclid_fill_ = 8;
      euclid_length_ = 9;
      break;
    case EuclidPattern::_7_8:
      euclid_fill_ = 7;
      euclid_length_ = 8;
      break;
    case EuclidPattern::_13_15:
      euclid_fill_ = 13;
      euclid_length_ = 15;
      break;
    case EuclidPattern::_6_7:
      euclid_fill_ = 6;
      euclid_length_ = 7;
      break;
    case EuclidPattern::_11_13:
      euclid_fill_ = 11;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_5_6:
      euclid_fill_ = 5;
      euclid_length_ = 6;
      break;
    case EuclidPattern::_9_11:
      euclid_fill_ = 9;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_13_16:
      euclid_fill_ = 13;
      euclid_length_ = 16;
      break;
    case EuclidPattern::_4_5:
      euclid_fill_ = 4;
      euclid_length_ = 5;
      break;
    case EuclidPattern::_11_14:
      euclid_fill_ = 11;
      euclid_length_ = 14;
      break;
    case EuclidPattern::_7_9:
      euclid_fill_ = 7;
      euclid_length_ = 9;
      break;
    case EuclidPattern::_10_13:
      euclid_fill_ = 10;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_3_4:
      euclid_fill_ = 3;
      euclid_length_ = 4;
      break;
    case EuclidPattern::_11_15:
      euclid_fill_ = 11;
      euclid_length_ = 15;
      break;
    case EuclidPattern::_8_11:
      euclid_fill_ = 8;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_5_7:
      euclid_fill_ = 5;
      euclid_length_ = 7;
      break;
    case EuclidPattern::_7_10:
      euclid_fill_ = 7;
      euclid_length_ = 10;
      break;
    case EuclidPattern::_9_13:
      euclid_fill_ = 9;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_11_16:
      euclid_fill_ = 11;
      euclid_length_ = 16;
      break;
    case EuclidPattern::_9_14:
      euclid_fill_ = 9;
      euclid_length_ = 14;
      break;
    case EuclidPattern::_7_11:
      euclid_fill_ = 7;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_5_8:
      euclid_fill_ = 5;
      euclid_length_ = 8;
      break;
    case EuclidPattern::_8_13:
      euclid_fill_ = 8;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_3_5:
      euclid_fill_ = 3;
      euclid_length_ = 5;
      break;
    case EuclidPattern::_7_12:
      euclid_fill_ = 7;
      euclid_length_ = 12;
      break;
    case EuclidPattern::_4_7:
      euclid_fill_ = 4;
      euclid_length_ = 7;
      break;
    case EuclidPattern::_9_16:
      euclid_fill_ = 9;
      euclid_length_ = 16;
      break;
    case EuclidPattern::_5_9:
      euclid_fill_ = 5;
      euclid_length_ = 9;
      break;
    case EuclidPattern::_6_11:
      euclid_fill_ = 6;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_7_13:
      euclid_fill_ = 7;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_8_15:
      euclid_fill_ = 8;
      euclid_length_ = 15;
      break;
    case EuclidPattern::_7_15:
      euclid_fill_ = 7;
      euclid_length_ = 15;
      break;
    case EuclidPattern::_6_13:
      euclid_fill_ = 6;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_5_11:
      euclid_fill_ = 5;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_4_9:
      euclid_fill_ = 4;
      euclid_length_ = 9;
      break;
    case EuclidPattern::_7_16:
      euclid_fill_ = 7;
      euclid_length_ = 16;
      break;
    case EuclidPattern::_3_7:
      euclid_fill_ = 3;
      euclid_length_ = 7;
      break;
    case EuclidPattern::_5_12:
      euclid_fill_ = 5;
      euclid_length_ = 12;
      break;
    case EuclidPattern::_2_5:
      euclid_fill_ = 2;
      euclid_length_ = 5;
      break;
    case EuclidPattern::_5_13:
      euclid_fill_ = 5;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_3_8:
      euclid_fill_ = 3;
      euclid_length_ = 8;
      break;
    case EuclidPattern::_4_11:
      euclid_fill_ = 4;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_5_14:
      euclid_fill_ = 5;
      euclid_length_ = 14;
      break;
    case EuclidPattern::_5_16:
      euclid_fill_ = 5;
      euclid_length_ = 16;
      break;
    case EuclidPattern::_4_13:
      euclid_fill_ = 4;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_3_10:
      euclid_fill_ = 3;
      euclid_length_ = 10;
      break;
    case EuclidPattern::_2_7:
      euclid_fill_ = 2;
      euclid_length_ = 7;
      break;
    case EuclidPattern::_3_11:
      euclid_fill_ = 3;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_4_15:
      euclid_fill_ = 4;
      euclid_length_ = 15;
      break;
    case EuclidPattern::_3_13:
      euclid_fill_ = 3;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_2_9:
      euclid_fill_ = 2;
      euclid_length_ = 9;
      break;
    case EuclidPattern::_3_14:
      euclid_fill_ = 3;
      euclid_length_ = 14;
      break;
    case EuclidPattern::_3_16:
      euclid_fill_ = 3;
      euclid_length_ = 16;
      break;
    case EuclidPattern::_2_11:
      euclid_fill_ = 2;
      euclid_length_ = 11;
      break;
    case EuclidPattern::_2_13:
      euclid_fill_ = 2;
      euclid_length_ = 13;
      break;
    case EuclidPattern::_2_15:
      euclid_fill_ = 2;
      euclid_length_ = 15;
      break;
  }
  // this line guarantees that first pulse is always on
  euclid_rotate_ = euclid_length_ / euclid_fill_;
}

}  // namespace Sequencer