#ifndef DISPLAY_H
#define DISPLAY_H

#include <AALeC-V3.h>

void displayShow();
void drawSpinner(int cx, int cy, int r, int frame);
void pulseLEDs(unsigned long t, RgbColor base);
void showConnectingFrame();
void showConnected();
void showNameSelect();
void showWaiting();
void showEstimate();
void showHigherLower();
void showPotiTarget();
void showTempTarget();
void showVoting();
void showVoted();
void showReveal();
void showEnded();

#endif // DISPLAY_H
