#ifndef __F_SETUP_H_
#define __F_SETUP_H_

void setupRAMGuards (void);
void disarmRAMGuards (void);
void unlockFlashForRW (void);
void unlockInfoForRO (void);
void checkBuildVersion (void);
void reboot (void);
void _purgatory (void);

#endif  // __F_SETUP_H_
