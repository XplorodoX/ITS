#ifndef AUDIO_H
#define AUDIO_H

void playMelody(const unsigned int* notes, const unsigned int* durs, int len);
void soundSubmit();
void soundCorrect();
void soundWrong();
void soundWinner();

#endif // AUDIO_H
