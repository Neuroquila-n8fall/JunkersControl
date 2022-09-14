#ifndef NVM_H
#define NVM_H

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

extern Preferences preferences;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

/**
 * @brief Initializes the preferences storage
 **/
extern void initSettings();
/**
 * @brief Loads stored settings. Uses default values if this is the first time the ESP starts
 **/
extern void loadSettings();

#endif