/*
 * Ultrasonic.h
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
 * modified 30 Nov 2021
 * by Israel Lins (github: @IsraeLins)
 *
 * Released into the MIT License.
 */

#ifndef Ultrasonic_h
#define Ultrasonic_h

class Ultrasonic {
  public:
    Ultrasonic(uint8_t sigPin) : Ultrasonic(sigPin, sigPin) {};
    Ultrasonic(uint8_t trigPin, uint8_t echoPin, unsigned long timeOut = 20000UL);
    ~Ultrasonic();

    bool measureIsAvaible() const;
    unsigned long long measureMM();
    float measureINCH();
    void setTimeout(unsigned long timeOut) { timeout = timeOut; }
    void setInterval(unsigned long interval) { interval = interval; }
    void update();

  private:
    static void echoChanged(Ultrasonic* self, bool pinstate);

  private:
    enum State {
      Sending,
      WaitingHigh,
      WaitingLow,
    };

    uint8_t trig;
    uint8_t echo;
    State state = State::Sending;

    long lastElapsed = -1;

    unsigned long sentMicros = 0;
    unsigned long receivedMicros = -1;

    unsigned long interval = 1000000; // in us
    unsigned long timeout = 10000; // in us
};

#endif // Ultrasonic_h
