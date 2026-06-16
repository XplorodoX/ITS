#include "audio.h"
#include "globals.h"

void playMelody(const unsigned int* notes, const unsigned int* durs, int len) {
  for (int i = 0; i < len; i++) {
    aalec.play(notes[i], durs[i]);
    delay(durs[i] + 20);  // Note + kurze Pause zwischen Tönen
  }
  aalec.play(t_off);
}

void soundSubmit() {
  // Kurzer Bestätigungs-Beep beim Abschicken der Antwort
  aalec.play(t_c_2, 80);
  delay(100);
  aalec.play(t_off);
}

void soundCorrect() {
  // Aufsteigende Fanfare: C → E → G → C2
  static const unsigned int notes[] = { t_c_1, t_e_1, t_g_1, t_c_2 };
  static const unsigned int durs[]  = { 120,   120,   120,   250   };
  playMelody(notes, durs, 4);
}

void soundWrong() {
  // Absteigende Misserfolgs-Melodie: A → D
  static const unsigned int notes[] = { t_a_1, t_d_1 };
  static const unsigned int durs[]  = { 200,   350   };
  playMelody(notes, durs, 2);
}

void soundWinner() {
  // Winner-Fanfare: C C G E C2 — G C2
  static const unsigned int notes[] = { t_c_1, t_c_1, t_g_1, t_e_1, t_c_2, t_g_1, t_c_2 };
  static const unsigned int durs[]  = { 100,   100,   100,   100,   200,   100,   400   };
  playMelody(notes, durs, 7);
}
