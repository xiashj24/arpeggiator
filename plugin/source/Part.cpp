#include "PolyArp/Part.h"

namespace Sequencer {

int Part::getCurrentStepIndex() const {
  return (tick_ + getTicksHalfStep()) / getTicksPerStep();
}

void Part::renderNote(int index, Note note) {
  if (note.number <= DISABLED_NOTE)
    return;

  DBG("rendered note number: " << note.number);

  int note_on_tick =
      static_cast<int>((index + note.offset) * getTicksPerStep());
  int note_off_tick =
      static_cast<int>((index + note.offset + note.length) * getTicksPerStep());

  // force note off before the next note on of the same note
  // search all midi messages after note_on_tick
  // if there is a note off with the same note number
  // delete that and insert a new note off at note_on_tick
  bool note_off_deleted = false;
  for (int i = midiQueue_.getNextIndexAtTime(tick_);
       i < midiQueue_.getNumEvents(); ++i) {
    auto message = midiQueue_.getEventPointer(i)->message;
    if (message.isNoteOff() && message.getNoteNumber() == note.number) {
      midiQueue_.deleteEvent(i, false);
      note_off_deleted = true;
    }
  }

  if (note_off_deleted) {
    juce::MidiMessage early_note_off_message = juce::MidiMessage::noteOff(
        getChannel(), note.number, static_cast<juce::uint8>(note.velocity));
    early_note_off_message.setTimeStamp(note_on_tick);  // -1?
    renderMidiMessage(early_note_off_message);
  }

  // note on
  juce::MidiMessage note_on_message = juce::MidiMessage::noteOn(
      getChannel(), note.number, static_cast<juce::uint8>(note.velocity));
  note_on_message.setTimeStamp(note_on_tick);
  renderMidiMessage(note_on_message);

  // note off
  juce::MidiMessage note_off_message = juce::MidiMessage::noteOff(
      getChannel(), note.number, static_cast<juce::uint8>(note.velocity));
  note_off_message.setTimeStamp(note_off_tick);
  renderMidiMessage(note_off_message);
}

// insert a future MIDI message into MIDI queue
void Part::renderMidiMessage(juce::MidiMessage message) {
  // int tick = static_cast<int>(message.getTimeStamp());
  // tick = ApplySwingToTick(tick);
  // message.setTimeStamp(static_cast<int>(tick));
  midiQueue_.addEvent(message);
}

void Part::reset(float start_index) {
  // sendNoteOffNow();
  midiQueue_.clear();
  tick_ = static_cast<int>(getTicksPerStep() * start_index);
  resolution_ = resolutionNew_;  // necessary?
}

void Part::sendNoteOffNow() {
  // delete unsent note on-offs pairs in the future first?
  // for (int i = midiQueue_.getNextIndexAtTime(tick_);
  //      i < midiQueue_.getNumEvents(); ++i) {
  //   auto& message = midiQueue_.getEventPointer(i)->message;
  //   if (message.isNoteOn())
  //   {
  //     message.setVelocity(0.f);
  //   }
  // }

  for (int i = midiQueue_.getNextIndexAtTime(tick_);
       i < midiQueue_.getNumEvents(); ++i) {
    auto message = midiQueue_.getEventPointer(i)->message;
    if (message.isNoteOff()) {
      message.setTimeStamp(tick_);  // this appears to have no effect
      sendMidiMessage(message);
    }
  }
}

void Part::tick() {
  // disabled part still ticks but does not render step
  int index = getCurrentStepIndex();

  if (!muted_) {
    // render the step just right before it's too late
    if (tick_ == getStepRenderTick(index)) {
      renderStep(index);
    }
  }

  // on step callback
  if (isOnGrid()) {
    if (onStep) {
      onStep(index);
    }
  }

  // send current tick's MIDI events
  for (int i = midiQueue_.getNextIndexAtTime(tick_);
       i < midiQueue_.getNextIndexAtTime(tick_ + 1); ++i) {
    auto message = midiQueue_.getEventPointer(i)->message;

    // Note: the following code is necessary for seq but do not make sense for
    // arp, which indicate that MIDI merging should be processed by a separate
    // module (probably VoiceAssigner)

    // do not note off if held by the keyboard
    // if (message.isNoteOff() &&
    // keyboardRef.isKeyDown(message.getNoteNumber())) {
    //   continue;
    // }

    sendMidiMessage(message);
  }

  tick_ += 1;

  // update track length on step boundaries
  if (tick_ % getTicksPerStep() == getTicksHalfStep()) {
    trackLength_ = trackLengthNew_;
  }

  // update resolution at 0 tick
  // worry: if (resolution becomes wider) and first step has -0.5 offset
  // the first step might be skipped
  // if (isOnGrid()) {
  //   resolution_ = resolutionNew_;
  // }

  // wrap from (length-0.5) to -0.5 step
  // worry: use == instead of >=?
  if (tick_ >= trackLength_ * getTicksPerStep() - getTicksHalfStep()) {
    midiQueue_.addTimeToMessages(
        -tick_);  // or use - (trackLength_ * getTicksPerStep() -
                  // getTicksHalfStep())?

    // only keep newer note events
    for (const auto& midiEvent : midiQueue_) {
      if (midiEvent->message.getTimeStamp() >= 0) {
        midiQueueNext_.addEvent(midiEvent->message);
      }
    }
    midiQueue_.swapWith(midiQueueNext_);
    midiQueueNext_.clear();

    // apply new resulution
    resolution_ = resolutionNew_;

    // tick_ -= trackLength_ * getTicksPerStep();
    tick_ = -getTicksHalfStep();
  }
}
}  // namespace Sequencer