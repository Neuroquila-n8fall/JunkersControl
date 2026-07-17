#ifndef FAILSAFE_H
#define FAILSAFE_H

void SetupFailSafe();
void UpdateFailSafe();
void NotifyValidHeatingCommand();
bool IsFailSafeActive();
double ApplyFailSafeFeedLimits(double temperature);

#endif
