/*
 * Ultrasonic.cpp
 *
 * Library for Ultrasonic Ranging Module in a minimalist way
 *
 * created 3 Apr 2014
 * by Erick Simões (github: @ErickSimoes | twitter: @AloErickSimoes)
 * modified 23 Jan 2017
 * by Erick Simões (github: @ErickSimoes | twitter: @AloErickSimoes)
 * modified 04 Mar 2017
 * by Erick Simões (github: @ErickSimoes | twitter: @AloErickSimoes)
 * modified 15 May 2017
 * by Eliot Lim    (github: @eliotlim)
 * modified 10 Jun 2018
 * by Erick Simões (github: @ErickSimoes | twitter: @AloErickSimoes)
 * modified 14 Jun 2018
 * by Otacilio Maia (github: @OtacilioN | linkedIn: in/otacilio)
 * modified 30 Nov 2021
 * by Israel Lins (github: @IsraeLins)
 *
 * Released into the MIT License.
 */

#if ARDUINO >= 100
  #include <Arduino.h>
#else
  #include <WProgram.h>
#endif
#include <YetAnotherPcInt.h>

#include "Ultrasonic.h"

#define SOUND_SPEED 346.3 // m/s at 25 °C (298,15 K)
#define US_TO_S 1e-6
#define M_TO_MM 1e3
#define MM_TO_IN 1.0 / 25.4

Ultrasonic::Ultrasonic(uint8_t trigPin, uint8_t echoPin, unsigned long timeOut) {
  trig = trigPin;
  echo = echoPin;
  timeout = timeOut;
  state = State::Sending;

  pinMode(trig, OUTPUT);
  digitalWrite(trig, LOW);

  if (trig != echo) {
    pinMode(echo, INPUT);
  }

  PcInt::attachInterrupt(echo, echoChanged, this, CHANGE);
}

Ultrasonic::~Ultrasonic() {
    PcInt::detachInterrupt(echo);
}

void Ultrasonic::echoChanged(Ultrasonic* self, bool pinstate) {
    if ((self->state == State::WaitingHigh) && pinstate) {
        self->receivedMicros = micros();
        self->state = State::WaitingLow;
    }
    // last state
    if ((self->state == State::WaitingLow) && !pinstate) {
        self->receivedMicros = micros();
        self->lastElapsed = (self->receivedMicros - self->sentMicros);
        self->state = State::Sending;
    }
}

void Ultrasonic::update() {
  if (state == State::Sending) {
    if (abs(micros() - max(receivedMicros, sentMicros)) <= interval) {
      // wait a little to trigger again
      return;
    }

    if (trig == echo)
      pinMode(trig, OUTPUT);

    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    if (trig == echo)
      pinMode(trig, INPUT);

    sentMicros = micros();
    receivedMicros = -1;
    state = WaitingHigh;
  } else {
    unsigned long l_elapsed = (micros() - sentMicros);
    if (l_elapsed >= timeout) {
      lastElapsed = l_elapsed;
      state = State::Sending;
    }
  }
}

bool Ultrasonic::measureIsAvaible() const {
    return (lastElapsed >= 0);
}

unsigned long long Ultrasonic::measureMM() {
  return ((lastElapsed * SOUND_SPEED) * M_TO_MM * US_TO_S) / 2;
}

float Ultrasonic::measureINCH() {
  return float(measureMM()) * MM_TO_IN;
}
