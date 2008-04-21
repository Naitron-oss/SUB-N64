/***************************************************************************
                          plugin.h  -  description
                             -------------------
    begin                : Fri Oct 18 2002
    copyright            : (C) 2002 by blight
    email                : blight@fuckmicrosoft.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#define PACKAGE "blight's SDL input plugin"
#define VERSION "0.0.10"

#define PLUGIN_NAME     PACKAGE
#define PLUGIN_VERSION  VERSION

#ifndef PATH_MAX
#  define PATH_MAX 1024
#endif

#include "../main/winlnxdefs.h"
#include "Controller_1.1.h"

#include "SDL.h"

#define DEVICE_KEYBOARD     (-1)
#define DEVICE_NONE         (-2)

// Some stuff from n-rage plugin
#define RD_GETSTATUS        0x00        // get status
#define RD_READKEYS         0x01        // read button values
#define RD_READPAK          0x02        // read from controllerpack
#define RD_WRITEPAK         0x03        // write to controllerpack
#define RD_RESETCONTROLLER  0xff        // reset controller
#define RD_READEEPROM       0x04        // read eeprom
#define RD_WRITEEPROM       0x05        // write eeprom

#define PAK_IO_RUMBLE       0xC000      // the address where rumble-commands are sent to

enum EButton
{
    R_DPAD          = 0,
    L_DPAD,
    D_DPAD,
    U_DPAD,
    START_BUTTON,
    Z_TRIG,
    B_BUTTON,
    A_BUTTON,
    R_CBUTTON,
    L_CBUTTON,
    D_CBUTTON,
    U_CBUTTON,
    R_TRIG,
    L_TRIG,
    MEMPAK,
    RUMBLEPAK,
    Y_AXIS,
    X_AXIS,
    NUM_BUTTONS
};

typedef struct
{
    int button;         // button index; -1 if notassigned
    SDLKey key;         // sdl keysym; SDLK_UNKNOWN if not assigned
    int axis, axis_dir; // aixs + direction (i.e. 0, 1 = X Axis +; 0, -1 = X Axis -); -1 if notassigned
    int hat, hat_pos;   // hat + hat position; -1 if not assigned
    int mouse;          // mouse button
} SButtonMap;

typedef struct
{
    int button_a, button_b;         // up/down or left/right; -1 if not assigned
    SDLKey key_a, key_b;            // up/down or left/right; SDLK_UNKNOWN if not assigned
    int axis;                       // axis index; -1 if not assigned
    int hat, hat_pos_a, hat_pos_b;  // hat + hat position up/down and left/right; -1 if not assigned
} SAxisMap;

typedef struct
{
    CONTROL control;
    BUTTONS buttons;

    // mappings
    SButtonMap    button[16];   // 14 buttons; in the order of EButton
    SAxisMap      axis[2];      //  2 axis
    int           device;       // joystick device; -1 = keyboard; -2 = none
    int           mouse;        // mouse enabled: 0 = no; 1 = yes
    SDL_Joystick *joystick;     // SDL joystick device
    int           event_joystick;   // the /dev/input/eventX device for force feeback
    
} SController;

#endif // __PLUGIN_H__
