#ifndef CONFIG_H
#define CONFIG_H

#include "MAX7456.h"

// vTx config
//=============================
//#define IMPULSERC_VTX

#ifdef IMPULSERC_VTX
# define MAX7456RESET  9         // RESET
#endif

//#define NEW_SEND_FC_SETTINGS
//#define MUSTACHE

#endif
