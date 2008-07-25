/**
 * Mupen64 - main.c
 * Copyright (C) 2002 Hacktarux
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 * 
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

/* This is MUPEN64's main entry point. It contains code that is common
 * to both the gui and non-gui versions of mupen64. See
 * gui subdirectories for the gui-specific code.
 * if you want to implement an interface, you should look here
 */

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h> // POSIX Thread library
#include <signal.h> // signals
#include <getopt.h> // getopt_long
#include <libgen.h> // basename, dirname
#include <png.h>    // for writing screenshot PNG files
#include <SDL.h>

#include "main.h"
#include "version.h"
#include "winlnxdefs.h"
#include "config.h"
#include "plugin.h"
#include "rom.h"
#include "romcache.h"
#include "../r4300/r4300.h"
#include "../r4300/recomph.h"
#include "../r4300/interupt.h"
#include "../memory/memory.h"
#include "savestates.h"
#include "util.h"
#include "translate.h"
#include "cheat.h"
#include "../opengl/osd.h"
#include "../opengl/screenshot.h"
#include "../network/network.h"


#ifdef DBG
#include <glib.h>
#include "../debugger/debugger.h"
#endif

#ifdef WITH_LIRC
#include "lirc.h"
#endif //WITH_LIRC

#ifdef VCR_SUPPORT
#include "vcr.h"
#include "vcr_compress.h"
#endif

/** function prototypes **/
static void parseCommandLine(int argc, char **argv);
static int  SaveRGBBufferToFile(char *filename, unsigned char *buf, int width, int height, int pitch);
static void *emulationThread( void *_arg );
static void sighandler( int signal, siginfo_t *info, void *context ); // signal handler
extern void *rom_cache_system(void *_arg);

/** globals **/
int         g_Noask = 0;                // don't ask to force load on bad dumps
int         g_NoaskParam = 0;           // was --noask passed at the commandline?
int         g_MemHasBeenBSwapped = 0;   // store byte-swapped flag so we don't swap twice when re-playing game
pthread_t   g_EmulationThread = 0;      // core thread handle
pthread_t   g_RomCacheThread = 0;       // rom cache thread handle
int         g_EmulatorRunning = 0;      // need separate boolean to tell if emulator is running, since --nogui doesn't use a thread
int         g_OsdEnabled = 1;           // On Screen Display enabled?
int         g_Fullscreen = 0;           // fullscreen enabled?
int         g_TakeScreenshot = 0;       // Tell OSD Rendering callback to take a screenshot just before drawing the OSD
char        *g_GfxPlugin = NULL;        // pointer to graphics plugin specified at commandline (if any)
char        *g_AudioPlugin = NULL;      // pointer to audio plugin specified at commandline (if any)
char        *g_InputPlugin = NULL;      // pointer to input plugin specified at commandline (if any)
char        *g_RspPlugin = NULL;        // pointer to rsp plugin specified at commandline (if any)
MupenClient  g_NetplayClient;

/** static (local) variables **/
#ifdef NO_GUI
static int  l_GuiEnabled = 0;           // GUI enabled?
#else
static int  l_GuiEnabled = 1;           // GUI enabled?
#endif

static char l_ConfigDir[PATH_MAX] = {0};
static char l_InstallDir[PATH_MAX] = {0};

static int   l_EmuMode = 0;              // emumode specified at commandline?
static int   l_CurrentFrame = 0;         // frame counter
static int  *l_TestShotList = NULL;      // list of screenshots to take for regression test support
static int   l_TestShotIdx = 0;          // index of next screenshot frame in list
static char *l_Filename = NULL;          // filename to load & run at startup (if given at command line)
static int   l_SpeedFactor = 100;        // percentage of nominal game speed at which emulator is running
static int   l_FrameAdvance = 0;         // variable to check if we pause on next frame

static osd_message_t *l_volMsg = NULL;

static NetPlaySettings  l_NetSettings;
static int              SyncStatus;

MupenClient *getNetplayClient();
MupenClient *getNetplayClient() {return &g_NetplayClient;}


/*********************************************************************************************************
* exported gui funcs
*/
char *get_configpath()
{
    return l_ConfigDir;
}

char *get_installpath()
{
    return l_InstallDir;
}

char *get_savespath()
{
    static char path[PATH_MAX];
    strncpy(path, get_configpath(), PATH_MAX-5);
    strcat(path, "save/");
    return path;
}

char *get_iconspath()
{
    static char path[PATH_MAX];
    strncpy(path, get_installpath(), PATH_MAX-6);
    strcat(path, "icons/");
    return path;
}

char *get_iconpath(char *iconfile)
{
    static char path[PATH_MAX];
    strncpy(path, get_iconspath(), PATH_MAX-strlen(iconfile));
    strcat(path, iconfile);
    return path;
}

int gui_enabled(void)
{
    return l_GuiEnabled;
}

void main_message(unsigned int console, unsigned int statusbar, unsigned int osd, unsigned int osd_corner, const char *format, ...)
{
    va_list ap;
    char buffer[2049];
    va_start(ap, format);
    vsnprintf(buffer, 2047, format, ap);
    buffer[2048]='\0';
    va_end(ap);

    if (g_OsdEnabled && osd)
        osd_new_message(osd_corner, buffer);
#ifndef NO_GUI
    if (l_GuiEnabled && statusbar)
        gui_message(0, buffer);
#endif
    if (console)
        printf("%s\n", buffer);
}

void error_message(const char *format, ...)
{
    va_list ap;
    char buffer[2049];
    va_start(ap, format);
    vsnprintf(buffer, 2047, format, ap);
    buffer[2048]='\0';
    va_end(ap);

#ifndef NO_GUI
    if (l_GuiEnabled)
        gui_message(1, buffer);
#endif
    printf("%s: %s\n", tr("Error"), buffer);
}


void setSpeed(unsigned int speed)
{
    l_SpeedFactor = speed;
    setSpeedFactor(l_SpeedFactor);  // call to audio plugin
}


/*********************************************************************************************************
* timer functions
*/
static float VILimit = 60.0;
static double VILimitMilliseconds = 1000.0/60.0;

static int GetVILimit(void)
{
    switch (ROM_HEADER->Country_code&0xFF)
    {
        // PAL codes
        case 0x44:
        case 0x46:
        case 0x49:
        case 0x50:
        case 0x53:
        case 0x55:
        case 0x58:
        case 0x59:
            return 50;
            break;

        // NTSC codes
        case 0x37:
        case 0x41:
        case 0x45:
        case 0x4a:
            return 60;
            break;

        // Fallback for unknown codes
        default:
            return 60;
    }
}

static unsigned int gettimeofday_msec(void)
{
    struct timeval tv;
    unsigned int foo;

    gettimeofday(&tv, NULL);
    foo = ((tv.tv_sec % 1000000) * 1000) + (tv.tv_usec / 1000);
    return foo;
}

/*********************************************************************************************************
* global functions, for adjusting the core emulator behavior
*/

void main_speeddown(int percent)
{
    if (l_SpeedFactor - percent > 10)  /* 10% minimum speed */
    {
        l_SpeedFactor -= percent;
        main_message(0, 1, 1, OSD_BOTTOM_LEFT, "%s %d%%", tr("Playback speed:"), l_SpeedFactor);
        setSpeedFactor(l_SpeedFactor);  // call to audio plugin
    }
}

void main_speedup(int percent)
{
    if (l_SpeedFactor + percent < 300) /* 300% maximum speed */
    {
        l_SpeedFactor += percent;
        main_message(0, 1, 1, OSD_BOTTOM_LEFT, "%s %d%%", tr("Playback speed:"), l_SpeedFactor);
        setSpeedFactor(l_SpeedFactor);  // call to audio plugin
    }
}

void main_pause(void)
{
    pauseContinueEmulation();
    l_FrameAdvance = 0;
}

void main_advance_one(void)
{
    l_FrameAdvance = 1;
    rompause = 0;
}
/*
=======
    int Dif;
    unsigned int CurrentFPSTime;
    static unsigned int LastFPSTime = 0;
    static unsigned int CounterTime = 0;
    static unsigned int CalculatedTime ;
    static int VI_Counter = 0;

#ifdef DBG
    if(debugger_mode) debugger_frontend_vi();
#endif

    // if paused, poll for input events
    if(rompause) {
        osd_render();  // draw Paused message in case updateScreen didn't do it
        SDL_GL_SwapBuffers();
    }

    do  {
        SyncStatus = netMain(&g_NetplayClient); //may adjust l_SpeedFactor

        double AdjustedLimit = VILimitMilliseconds * 100.0 / l_SpeedFactor;  // adjust for selected emulator speed
        int time;

        SDL_PumpEvents();
#ifdef WITH_LIRC
        lircCheckInput();
#endif //WITH_LIRC


        start_section(IDLE_SECTION);
        VI_Counter++;

        if(LastFPSTime == 0)
        {
            LastFPSTime = gettimeofday_msec();
            CounterTime = gettimeofday_msec();
            return;
        }
        CurrentFPSTime = gettimeofday_msec();

        Dif = CurrentFPSTime - LastFPSTime;

        if (Dif < AdjustedLimit) 
        {
            CalculatedTime = CounterTime + AdjustedLimit * VI_Counter;
            time = (int)(CalculatedTime - CurrentFPSTime);
            if (time > 0)
            {
                usleep(time * 1000);
            }
        CurrentFPSTime = CurrentFPSTime + time;
        }

        if (CurrentFPSTime - CounterTime >= 1000.0 ) 
        {
            CounterTime = gettimeofday_msec();
            VI_Counter = 0 ;
        }

        LastFPSTime = CurrentFPSTime ;
        end_section(IDLE_SECTION);
    } while (rompause);

    if (l_FrameAdvance) {
        rompause = 1;
        l_FrameAdvance = 0;
    }
>>>>>>> .merge-right.r807
}
*/
void main_draw_volume_osd(void)
{
    char msgString[32];
    const char *volString;

    // if we had a volume message, make sure that it's still in the OSD list, or set it to NULL
    if (l_volMsg != NULL && !osd_message_valid(l_volMsg))
        l_volMsg = NULL;

    // this calls into the audio plugin
    volString = volumeGetString();
    if (volString == NULL)
    {
        strcpy(msgString, tr("Volume Not Supported."));
    }
    else
    {
        sprintf(msgString, "%s: %s", tr("Volume"), volString);
        if (msgString[strlen(msgString) - 1] == '%')
            strcat(msgString, "%");
    }

    // create a new message or update an existing one
    if (l_volMsg != NULL)
        osd_update_message(l_volMsg, msgString);
    else
        l_volMsg = osd_new_message(OSD_MIDDLE_CENTER, msgString);
}

/* this function could be called as a result of a keypress, joystick/button movement,
   LIRC command, or 'testshots' command-line option timer */
void take_next_screenshot(void)
{
    g_TakeScreenshot = l_CurrentFrame + 1;
}

void startEmulation(void)
{
    VILimit = GetVILimit();
    VILimitMilliseconds = (double) 1000.0/VILimit; 
    printf("init timer!\n");

    const char *gfx_plugin = NULL,
               *audio_plugin = NULL,
               *input_plugin = NULL,
               *RSP_plugin = NULL;

    // make sure rom is loaded before running
    if(!rom)
    {
        error_message(tr("There is no Rom loaded."));
        return;
    }

    // make sure all plugins are specified before running
    if(g_GfxPlugin)
        gfx_plugin = plugin_name_by_filename(g_GfxPlugin);
    else
        gfx_plugin = plugin_name_by_filename(config_get_string("Gfx Plugin", ""));

    if(!gfx_plugin)
    {
        error_message(tr("No graphics plugin specified."));
        return;
    }

    if(g_AudioPlugin)
        audio_plugin = plugin_name_by_filename(g_AudioPlugin);
    else
        audio_plugin = plugin_name_by_filename(config_get_string("Audio Plugin", ""));

    if(!audio_plugin)
    {
        error_message(tr("No audio plugin specified."));
        return;
    }

    if(g_InputPlugin)
        input_plugin = plugin_name_by_filename(g_InputPlugin);
    else
        input_plugin = plugin_name_by_filename(config_get_string("Input Plugin", ""));

    if(!input_plugin)
    {
        error_message(tr("No input plugin specified."));
        return;
    }

    if(g_RspPlugin)
        RSP_plugin = plugin_name_by_filename(g_RspPlugin);
    else
        RSP_plugin = plugin_name_by_filename(config_get_string("RSP Plugin", ""));

    if(!RSP_plugin)
    {
        error_message(tr("No RSP plugin specified."));
        return;
    }
    
    // in nogui mode, just start the emulator in the main thread
    if(!l_GuiEnabled)
    {
        emulationThread(NULL);
    }
    else if(!g_EmulationThread)
    {
        // spawn emulation thread
        if(pthread_create(&g_EmulationThread, NULL, emulationThread, NULL) != 0)
        {
            g_EmulationThread = 0;
            error_message(tr("Couldn't spawn core thread!"));
            return;
        }
        pthread_detach(g_EmulationThread);
        main_message(0, 1, 0, OSD_BOTTOM_LEFT,  tr("Emulation started (PID: %d)"), g_EmulationThread);
    }
    // if emulation is already running, but it's paused, unpause it
    else if(rompause)
    {
        main_pause();
    }

#ifndef NO_GUI
    g_romcache.rcspause = 1;
#endif
}

void stopEmulation(void)
{
    if(g_EmulationThread || g_EmulatorRunning)
    {
#ifndef NO_GUI
        g_romcache.rcspause = 0;
#endif
        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Stopping emulation.\n"));
        rompause = 0;
        stop_it();

        // wait until emulation thread is done before continuing
        if(g_EmulationThread)
            pthread_join(g_EmulationThread, NULL);

        g_EmulatorRunning = 0;

        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Emulation stopped.\n"));
    }
}

int pauseContinueEmulation(void)
{
    static osd_message_t *msg = NULL;

    if (!g_EmulatorRunning)
        return 1;

    if (rompause)
    {
#ifndef NO_GUI
        g_romcache.rcspause = 1;
#endif
        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Emulation continued.\n"));
        if(msg)
        {
            osd_delete_message(msg);
            msg = NULL;
        }
    }
    else
    {
#ifndef NO_GUI
        g_romcache.rcspause = 0;
#endif
        if(msg)
            osd_delete_message(msg);

        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Paused\n"));
        msg = osd_new_message(OSD_MIDDLE_CENTER, tr("Paused\n"));
        osd_message_set_static(msg);
    }

    rompause = !rompause;
    return rompause;
}


void netplayReady(void)
{
    //NetMessage netMsg;
    g_NetplayClient.startEvt=1;
    //if (l_NetplayEnabled && g_NetplayClient.isConnected) {
    //    netMsg.type = NETMSG_READY;
    //    clientSendMessage(&g_NetplayClient, &netMsg);
    //}
}


/*********************************************************************************************************
* global functions, callbacks from the r4300 core or from other plugins
*/

void video_plugin_render_callback(void)
{
    // if the flag is set to take a screenshot, then grab it now
    if (g_TakeScreenshot != 0)
    {
        TakeScreenshot(g_TakeScreenshot - 1);  // current frame number +1 is in g_TakeScreenshot
        g_TakeScreenshot = 0; // reset flag
    }

    // if the OSD is enabled, then draw it now
    if (g_OsdEnabled)
    {
        osd_render();
    }
}

void new_frame(void)
{
    // take a screenshot if we need to
    if (l_TestShotList != NULL)
    {
        int nextshot = l_TestShotList[l_TestShotIdx];
        if (nextshot == l_CurrentFrame)
        {
            // set global variable so screenshot will be taken just before OSD is drawn at the end of frame rendering
            take_next_screenshot();
            // advance list index to next screenshot frame number.  If it's 0, then quit
            l_TestShotIdx++;
        }
        else if (nextshot == 0)
        {
            stopEmulation();
            free(l_TestShotList);
            l_TestShotList = NULL;
        }
    }

    // advance the current frame
    l_CurrentFrame++;
}

void new_vi(void)
{
    int Dif;
    unsigned int CurrentFPSTime;
    static unsigned int LastFPSTime = 0;
    static unsigned int CounterTime = 0;
    static unsigned int CalculatedTime ;
    static int VI_Counter = 0;

    double AdjustedLimit = VILimitMilliseconds * 100.0 / l_SpeedFactor;  // adjust for selected emulator speed
    int time;

    start_section(IDLE_SECTION);
    VI_Counter++;
    
#ifdef DBG
    if(debugger_mode) debugger_frontend_vi();
#endif

    if(LastFPSTime == 0)
    {
        LastFPSTime = gettimeofday_msec();
        CounterTime = gettimeofday_msec();
        return;
    }
    CurrentFPSTime = gettimeofday_msec();
    
    Dif = CurrentFPSTime - LastFPSTime;
    
    if (Dif < AdjustedLimit) 
    {
        CalculatedTime = CounterTime + AdjustedLimit * VI_Counter;
        time = (int)(CalculatedTime - CurrentFPSTime);
        if (time > 0)
        {
            usleep(time * 1000);
        }
        CurrentFPSTime = CurrentFPSTime + time;
    }

    if (CurrentFPSTime - CounterTime >= 1000.0 ) 
    {
        CounterTime = gettimeofday_msec();
        VI_Counter = 0 ;
    }
    
    LastFPSTime = CurrentFPSTime ;
    end_section(IDLE_SECTION);
    if (l_FrameAdvance) {
        rompause = 1;
        l_FrameAdvance = 0;
    }
}

/*********************************************************************************************************
* sdl event filter
*/
static int sdl_event_filter( const SDL_Event *event )
{
    static osd_message_t *msgFF = NULL;
    static int SavedSpeedFactor = 100;
    char *event_str = NULL;

    switch( event->type )
    {
        // user clicked on window close button
        case SDL_QUIT:
            stopEmulation();
            break;
        case SDL_KEYDOWN:
            switch( event->key.keysym.sym )
            {
                case SDLK_F8:
                    netplayReady();
                    break;

                case SDLK_ESCAPE:
                    stopEmulation();
                    break;
                case SDLK_RETURN:
                    // Alt+Enter toggles fullscreen
                    if(event->key.keysym.mod & (KMOD_LALT | KMOD_RALT))
                        changeWindow();
                    break;
                case SDLK_F5:
                    savestates_job |= SAVESTATE;
                    break;
                case SDLK_F7:
                    savestates_job |= LOADSTATE;
                    break;
                case SDLK_F9:
                    add_interupt_event(HW2_INT, 0);  /* Hardware 2 Interrupt immediately */
                    add_interupt_event(NMI_INT, 50000000);  /* Non maskable Interrupt after 1/2 second */
                    break;
                case SDLK_F10:
                    main_speeddown(5);
                    break;
                case SDLK_F11:
                    main_speedup(5);
                    break;
                case SDLK_F12:
                    // set flag so that screenshot will be taken at the end of frame rendering
                    take_next_screenshot();
                    break;

                // Pause
                case SDLK_PAUSE:
                    main_pause();
                    break;

                default:
                    switch (event->key.keysym.unicode)
                    {
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                            savestates_select_slot( event->key.keysym.unicode - '0' );
                            break;
                        // volume mute/unmute
                        case 'm':
                        case 'M':
                            volumeMute();
                            main_draw_volume_osd();
                            break;
                        // increase volume
                        case ']':
                            volumeUp();
                            main_draw_volume_osd();
                            break;
                        // decrease volume
                        case '[':
                            volumeDown();
                            main_draw_volume_osd();
                            break;
                        // fast-forward
                        case 'f':
                        case 'F':
                            SavedSpeedFactor = l_SpeedFactor;
                            l_SpeedFactor = 250;
                            setSpeedFactor(l_SpeedFactor);  // call to audio plugin
                            // set fast-forward indicator
                            msgFF = osd_new_message(OSD_TOP_RIGHT, tr("Fast Forward"));
                            osd_message_set_static(msgFF);
                            break;
                        // frame advance
                        case '/':
                        case '?':
                            main_advance_one();
                            break;

                        // pass all other keypresses to the input plugin
                        default:
                            keyDown( 0, event->key.keysym.sym );
                    }
            }
            return 0;
            break;

        case SDL_KEYUP:
            switch( event->key.keysym.sym )
            {
                case SDLK_ESCAPE:
                    break;
                case SDLK_f:
                    // cancel fast-forward
                    l_SpeedFactor = SavedSpeedFactor;
                    setSpeedFactor(l_SpeedFactor);  // call to audio plugin
                    // remove message
                    osd_delete_message(msgFF);
                    break;
                default:
                    keyUp( 0, event->key.keysym.sym );
            }
            return 0;
            break;

        // if joystick action is detected, check if it's mapped to a special function
        case SDL_JOYAXISMOTION:
            // axis events have to be above a certain threshold to be valid
            if(event->jaxis.value > -15000 && event->jaxis.value < 15000)
                break;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYHATMOTION:
            event_str = event_to_str(event);

            if(!event_str) return 0;

            if(strcmp(event_str, config_get_string("Joy Mapping Fullscreen", "")) == 0)
                changeWindow();
            else if(strcmp(event_str, config_get_string("Joy Mapping Stop", "")) == 0)
                stopEmulation();
            else if(strcmp(event_str, config_get_string("Joy Mapping Pause", "")) == 0)
                main_pause();
            else if(strcmp(event_str, config_get_string("Joy Mapping Save State", "")) == 0)
                savestates_job |= SAVESTATE;
            else if(strcmp(event_str, config_get_string("Joy Mapping Load State", "")) == 0)
                savestates_job |= LOADSTATE;
            else if(strcmp(event_str, config_get_string("Joy Mapping Increment Slot", "")) == 0)
                savestates_inc_slot();
            else if(strcmp(event_str, config_get_string("Joy Mapping Screenshot", "")) == 0)
                take_next_screenshot();
            else if(strcmp(event_str, config_get_string("Joy Mapping Mute", "")) == 0)
            {
                volumeMute();
                main_draw_volume_osd();
            }
            else if(strcmp(event_str, config_get_string("Joy Mapping Decrease Volume", "")) == 0)
            {
                volumeDown();
                main_draw_volume_osd();
            }
            else if(strcmp(event_str, config_get_string("Joy Mapping Increase Volume", "")) == 0)
            {
                volumeUp;
                main_draw_volume_osd();
            }

            free(event_str);
            return 0;
            break;
    }

    return 1;
}

/*********************************************************************************************************
* emulation thread - runs the core
*/
static void * emulationThread( void *_arg )
{
    const char *gfx_plugin = NULL,
               *audio_plugin = NULL,
           *input_plugin = NULL,
           *RSP_plugin = NULL;
    struct sigaction sa;

    // install signal handler, but only if we're running in GUI mode
    // in non-GUI mode, we don't need to catch exceptions (there's no GUI to take down)
    if (l_GuiEnabled)
    {
        memset( &sa, 0, sizeof( struct sigaction ) );
        sa.sa_sigaction = sighandler;
        sa.sa_flags = SA_SIGINFO;
        sigaction( SIGSEGV, &sa, NULL );
        sigaction( SIGILL, &sa, NULL );
        sigaction( SIGFPE, &sa, NULL );
        sigaction( SIGCHLD, &sa, NULL );
    }

    g_EmulatorRunning = 1;

    // if emu mode wasn't specified at the commandline, set from config file
    if(!l_EmuMode)
        dynacore = config_get_number( "Core", CORE_DYNAREC );

    no_compiled_jump = config_get_bool("NoCompiledJump", FALSE);

    // init sdl
    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(0);
    SDL_EnableKeyRepeat(0, 0);

    SDL_SetEventFilter(sdl_event_filter);
    SDL_EnableUNICODE(1);

    /* Determine which plugins to use:
     *  -If valid plugin was specified at the commandline, use it
     *  -Else, get plugin from config. NOTE: gui code must change config if user switches plugin in the gui)
     */  
    if(g_GfxPlugin)
        gfx_plugin = plugin_name_by_filename(g_GfxPlugin);
    else
        gfx_plugin = plugin_name_by_filename(config_get_string("Gfx Plugin", ""));

    if(g_AudioPlugin)
        audio_plugin = plugin_name_by_filename(g_AudioPlugin);
    else
        audio_plugin = plugin_name_by_filename(config_get_string("Audio Plugin", ""));

    if(g_InputPlugin)
        input_plugin = plugin_name_by_filename(g_InputPlugin);
    else
        input_plugin = plugin_name_by_filename(config_get_string("Input Plugin", ""));

    if(g_RspPlugin)
        RSP_plugin = plugin_name_by_filename(g_RspPlugin);
    else
        RSP_plugin = plugin_name_by_filename(config_get_string("RSP Plugin", ""));

    if(g_NetplayClient.isEnabled)
        new_vi();//do this only to ensure we run netplay core before loading rom-specifics

    // initialize memory, and do byte-swapping if it's not been done yet
    if (g_MemHasBeenBSwapped == 0)
    {
        init_memory(1);
        g_MemHasBeenBSwapped = 1;
    }
    else
    {
        init_memory(0);
    }

    // load the plugins and attach the ROM to them
    plugin_load_plugins(gfx_plugin, audio_plugin, input_plugin, RSP_plugin);
    romOpen_gfx();
    romOpen_audio();
    romOpen_input();

    // switch to fullscreen if enabled
    if (g_Fullscreen)
        changeWindow();

    if (g_OsdEnabled)
    {
        // init on-screen display
        void *pvPixels = NULL;
        int width = 640, height = 480;
        readScreen(&pvPixels, &width, &height); // read screen to get width and height
        if (pvPixels != NULL)
        {
            free(pvPixels);
            pvPixels = NULL;
        }
        osd_init(width, height);
    }

    // setup rendering callback from video plugin to the core, for screenshots and On-Screen-Display
    setRenderingCallback(video_plugin_render_callback);

#ifdef WITH_LIRC
    lircStart();
#endif // WITH_LIRC

#ifdef DBG
    if( g_DebuggerEnabled )
        init_debugger();
#endif
    // load cheats for the current rom
    cheat_load_current_rom();

    osd_new_message(OSD_MIDDLE_CENTER, "Mupen64Plus Started...");

    if(g_NetplayClient.isEnabled)
    {
            if (netStartNetplay(&g_NetplayClient, l_NetSettings)) {
                /* call r4300 CPU core and run the game */
                r4300_reset_hard();
                r4300_reset_soft();
                r4300_execute();
            }
            else
                printf("Failed to connect to server %s.\n", l_NetSettings.hostname);

    } 
    else 
    {
    /* call r4300 CPU core and run the game */
        r4300_reset_hard();
        r4300_reset_soft();
        r4300_execute();
    }

#ifdef WITH_LIRC
    lircStop();
#endif // WITH_LIRC

    if (g_OsdEnabled)
    {
        osd_exit();
    }

    romClosed_RSP();
    romClosed_input();
    romClosed_audio();
    romClosed_gfx();
    closeDLL_RSP();
    closeDLL_input();
    closeDLL_audio();
    closeDLL_gfx();
    free_memory();

    // clean up
    g_EmulationThread = 0;
    SDL_Quit();
    if (g_NetplayClient.isEnabled) netShutdown(&g_NetplayClient);
    if (l_Filename != 0)
    {
        // the following doesn't work - it wouldn't exit immediately but when the next event is
        // recieved (i.e. mouse movement)
/*      gdk_threads_enter();
        gtk_main_quit();
        gdk_threads_leave();*/
    }

    return NULL;
}

/*********************************************************************************************************
* signal handler
*/
static void sighandler(int signal, siginfo_t *info, void *context)
{
    if( info->si_pid == g_EmulationThread )
    {
        switch( signal )
        {
            case SIGSEGV:
                error_message(tr("The core thread recieved a SIGSEGV signal.\n"
                                "This means it tried to access protected memory.\n"
                                "Maybe you have set a wrong ucode for one of the plugins!"));
                printf( "SIGSEGV in core thread caught:\n" );
                printf( "\terrno = %d (%s)\n", info->si_errno, strerror( info->si_errno ) );
                printf( "\taddress = 0x%08lX\n", (unsigned long) info->si_addr );
#ifdef SEGV_MAPERR
                switch( info->si_code )
                {
                    case SEGV_MAPERR: printf( "                address not mapped to object\n" ); break;
                    case SEGV_ACCERR: printf( "                invalid permissions for mapped object\n" ); break;
                }
#endif
                break;
            case SIGILL:
                printf( "SIGILL in core thread caught:\n" );
                printf( "\terrno = %d (%s)\n", info->si_errno, strerror( info->si_errno ) );
                printf( "\taddress = 0x%08lX\n", (unsigned long) info->si_addr );
#ifdef ILL_ILLOPC
                switch( info->si_code )
                {
                    case ILL_ILLOPC: printf( "\tillegal opcode\n" ); break;
                    case ILL_ILLOPN: printf( "\tillegal operand\n" ); break;
                    case ILL_ILLADR: printf( "\tillegal addressing mode\n" ); break;
                    case ILL_ILLTRP: printf( "\tillegal trap\n" ); break;
                    case ILL_PRVOPC: printf( "\tprivileged opcode\n" ); break;
                    case ILL_PRVREG: printf( "\tprivileged register\n" ); break;
                    case ILL_COPROC: printf( "\tcoprocessor error\n" ); break;
                    case ILL_BADSTK: printf( "\tinternal stack error\n" ); break;
                }
#endif
                break;
            case SIGFPE:
                printf( "SIGFPE in core thread caught:\n" );
                printf( "\terrno = %d (%s)\n", info->si_errno, strerror( info->si_errno ) );
                printf( "\taddress = 0x%08lX\n", (unsigned long) info->si_addr );
                switch( info->si_code )
                {
                    case FPE_INTDIV: printf( "\tinteger divide by zero\n" ); break;
                    case FPE_INTOVF: printf( "\tinteger overflow\n" ); break;
                    case FPE_FLTDIV: printf( "\tfloating point divide by zero\n" ); break;
                    case FPE_FLTOVF: printf( "\tfloating point overflow\n" ); break;
                    case FPE_FLTUND: printf( "\tfloating point underflow\n" ); break;
                    case FPE_FLTRES: printf( "\tfloating point inexact result\n" ); break;
                    case FPE_FLTINV: printf( "\tfloating point invalid operation\n" ); break;
                    case FPE_FLTSUB: printf( "\tsubscript out of range\n" ); break;
                }
                break;
            default:
                printf( "Signal number %d in core thread caught:\n", signal );
                printf( "\terrno = %d (%s)\n", info->si_errno, strerror( info->si_errno ) );
        }
        pthread_cancel(g_EmulationThread);
        g_EmulationThread = 0;
        g_EmulatorRunning = 0;
    }
    else
    {
        printf( "Signal number %d caught:\n", signal );
        printf( "\terrno = %d (%s)\n", info->si_errno, strerror( info->si_errno ) );
        exit( EXIT_FAILURE );
    }
}

static void printUsage(const char *progname)
{
    char *str = strdup(progname);

    printf("Usage: %s [parameter(s)] rom\n"
           "\n"
           "Parameters:\n"
           "    --nogui             : do not display GUI.\n"
           "    --fullscreen        : turn fullscreen mode on.\n"
           "    --noosd             : disable onscreen display.\n"
           "    --gfx (path)        : use gfx plugin given by (path)\n"
           "    --audio (path)      : use audio plugin given by (path)\n"
           "    --input (path)      : use input plugin given by (path)\n"
           "    --rsp (path)        : use rsp plugin given by (path)\n"
           "    --emumode (number)  : set emu mode to: 0=Interpreter 1=DynaRec 2=Pure Interpreter\n"
           "    --sshotdir (dir)    : set screenshot directory to (dir)\n"
           "    --configdir (dir)   : force config dir (must contain mupen64plus.conf)\n"
           "    --installdir (dir)  : force install dir (place to look for plugins, icons, lang, etc)\n"
           "    --noask             : don't ask to force load on bad dumps.\n"
           "    --testshots (list)  : take screenshots at frames given in comma-separated list, then quit\n"
#ifdef DBG
           "    --debugger          : start with debugger enabled\n"
#endif 
           "    --connect (host)    : connect to server for netplay\n"
           "    --server            : start server\n"
           "    -h, --help          : see this help message\n"
           "\n", basename(str));

    free(str);

    return;
}

/* parseCommandLine
 *  Parses commandline options and sets global variables accordingly
 */
void parseCommandLine(int argc, char **argv)
{
    int i, shots;
    char *str = NULL;

    // option parsing vars
    int opt, option_index;
    enum
    {
        OPT_GFX = 1,
        OPT_AUDIO,
        OPT_INPUT,
        OPT_RSP,
        OPT_EMUMODE,
        OPT_SSHOTDIR,
        OPT_CONFIGDIR,
        OPT_INSTALLDIR,
#ifdef DBG
    OPT_DEBUGGER,
#endif
        OPT_NOASK,
        OPT_TESTSHOTS,
        OPT_CONNECT,
        OPT_SERVER
    };
    struct option long_options[] =
    {
        {"nogui", no_argument, &l_GuiEnabled, FALSE},
        {"noosd", no_argument, &g_OsdEnabled, FALSE},
        {"fullscreen", no_argument, &g_Fullscreen, TRUE},
        {"gfx", required_argument, NULL, OPT_GFX},
        {"audio", required_argument, NULL, OPT_AUDIO},
        {"input", required_argument, NULL, OPT_INPUT},
        {"rsp", required_argument, NULL, OPT_RSP},
        {"emumode", required_argument, NULL, OPT_EMUMODE},
        {"sshotdir", required_argument, NULL, OPT_SSHOTDIR},
        {"configdir", required_argument, NULL, OPT_CONFIGDIR},
        {"installdir", required_argument, NULL, OPT_INSTALLDIR},
#ifdef DBG
        {"debugger", no_argument, NULL, OPT_DEBUGGER},
#endif
        {"noask", no_argument, NULL, OPT_NOASK},
        {"testshots", required_argument, NULL, OPT_TESTSHOTS},
        {"connect", required_argument, NULL, OPT_CONNECT}, 
        {"server", no_argument, NULL, OPT_SERVER}, 
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}    // last opt must be empty
    };
    char opt_str[] = "h";

    /* parse commandline options */
    while((opt = getopt_long(argc, argv, opt_str,
                 long_options, &option_index)) != -1)
    {
        switch(opt)
        {
            // if getopt_long returns 0, it already set the global for us, so do nothing
            case 0:
                break;
            case OPT_GFX:
                if(plugin_scan_file(optarg, PLUGIN_TYPE_GFX))
                {
                    g_GfxPlugin = optarg;
                }
                else
                {
                    printf("***Warning: GFX Plugin '%s' couldn't be loaded!\n", optarg);
                }
                break;
            case OPT_AUDIO:
                if(plugin_scan_file(optarg, PLUGIN_TYPE_AUDIO))
                {
                    g_AudioPlugin = optarg;
                }
                else
                {
                    printf("***Warning: Audio Plugin '%s' couldn't be loaded!\n", optarg);
                }
                break;
            case OPT_INPUT:
                if(plugin_scan_file(optarg, PLUGIN_TYPE_CONTROLLER))
                {
                    g_InputPlugin = optarg;
                }
                else
                {
                    printf("***Warning: Input Plugin '%s' couldn't be loaded!\n", optarg);
                }
                break;
            case OPT_RSP:
                if(plugin_scan_file(optarg, PLUGIN_TYPE_RSP))
                {
                    g_RspPlugin = optarg;
                }
                else
                {
                    printf("***Warning: RSP Plugin '%s' couldn't be loaded!\n", optarg);
                }
                break;
            case OPT_EMUMODE:
                i = atoi(optarg);
                if(i >= CORE_INTERPRETER && i <= CORE_PURE_INTERPRETER)
                {
                    l_EmuMode = TRUE;
                    dynacore = i;
                }
                else
                {
                    printf("***Warning: Invalid Emumode: %s\n", optarg);
                }
                break;
            case OPT_SSHOTDIR:
                if(isdir(optarg))
                    SetScreenshotDir(optarg);
                else
                    printf("***Warning: Screen shot directory '%s' is not accessible or not a directory.\n", optarg);
                break;
            case OPT_CONFIGDIR:
                if(isdir(optarg))
                    strncpy(l_ConfigDir, optarg, PATH_MAX);
                else
                    printf("***Warning: Config directory '%s' is not accessible or not a directory.\n", optarg);
                break;
            case OPT_INSTALLDIR:
                if(isdir(optarg))
                    strncpy(l_InstallDir, optarg, PATH_MAX);
                else
                    printf("***Warning: Install directory '%s' is not accessible or not a directory.\n", optarg);
                break;
            case OPT_NOASK:
                g_Noask = g_NoaskParam = TRUE;
                break;
            case OPT_TESTSHOTS:
                // count the number of integers in the list
                shots = 1;
                str = optarg;
                while ((str = strchr(str, ',')) != NULL)
                {
                    str++;
                    shots++;
                }
                // create a list and populate it with the frame counter values at which to take screenshots
                if ((l_TestShotList = malloc(sizeof(int) * (shots + 1))) != NULL)
                {
                    int idx = 0;
                    str = optarg;
                    while (str != NULL)
                    {
                        l_TestShotList[idx++] = atoi(str);
                        str = strchr(str, ',');
                        if (str != NULL) str++;
                    }
                    l_TestShotList[idx] = 0;
                }
                break;
            case OPT_CONNECT:
                strncpy(l_NetSettings.hostname, optarg, 128);
                break;
            case OPT_SERVER:
                strncpy(l_NetSettings.hostname, "", 128);
                break;
#ifdef DBG
            case OPT_DEBUGGER:
                g_DebuggerEnabled = TRUE;
                break;
#endif
            // print help
            case 'h':
            case '?':
            default:
                printUsage(argv[0]);
                exit(1);
                break;
        }
    }

    // if there are still parameters left after option parsing, assume it's the rom filename
    if(optind < argc)
    {
        l_Filename = argv[optind];
    }

    // if executable name contains "_nogui", set l_GuiEnabled to FALSE.
    // This allows creation of a mupen64plus_nogui symlink to mupen64plus instead of passing --nogui
    // for backwards compatability with old mupen64_nogui program name.
    str = strdup(argv[0]);
    basename(str);
    if(strstr(str, "_nogui") != NULL)
    {
        l_GuiEnabled = FALSE;
    }
    free(str);
}

/** setPaths
 *  setup paths to config/install/screenshot directories. The config dir is the dir where all
 *  user config information is stored, e.g. mupen64plus.conf, save files, and plugin conf files.
 *  The install dir is where mupen64plus looks for common files, e.g. plugins, icons, language
 *  translation files.
 */
static void setPaths(void)
{
    char buf[PATH_MAX], buf2[PATH_MAX];

    // if the config dir was not specified at the commandline, look for ~/.mupen64plus dir
    if (strlen(l_ConfigDir) == 0)
    {
        strncpy(l_ConfigDir, getenv("HOME"), PATH_MAX);
        strncat(l_ConfigDir, "/.mupen64plus", PATH_MAX - strlen(l_ConfigDir));

        // if ~/.mupen64plus dir is not found, create it
        if(!isdir(l_ConfigDir))
        {
            printf("Creating %s to store user data\n", l_ConfigDir);
            if(mkdir(l_ConfigDir, (mode_t)0755) != 0)
            {
                printf("Error: Could not create %s: ", l_ConfigDir);
                perror(NULL);
                exit(errno);
            }

            // create save subdir
            strncpy(buf, l_ConfigDir, PATH_MAX);
            strncat(buf, "/save", PATH_MAX - strlen(buf));
            if(mkdir(buf, (mode_t)0755) != 0)
            {
                // report error, but don't exit
                printf("Warning: Could not create %s: %s", buf, strerror(errno));
            }

            // create screenshots subdir
            strncpy(buf, l_ConfigDir, PATH_MAX);
            strncat(buf, "/screenshots", PATH_MAX - strlen(buf));
            if(mkdir(buf, (mode_t)0755) != 0)
            {
                // report error, but don't exit
                printf("Warning: Could not create %s: %s", buf, strerror(errno));
            }
        }
    }

    // make sure config dir has a '/' on the end.
    if(l_ConfigDir[strlen(l_ConfigDir)-1] != '/')
        strncat(l_ConfigDir, "/", PATH_MAX - strlen(l_ConfigDir));

    // if install dir was not specified at the commandline, look for it in the default location
    if(strlen(l_InstallDir) == 0)
    {
        strncpy(l_InstallDir, PREFIX, PATH_MAX);
        strncat(l_InstallDir, "/share/mupen64plus/", PATH_MAX - strlen(l_InstallDir));

        // if install dir is not in the default location, try the same dir as the binary
        if(!isdir(l_InstallDir))
        {
            int n = readlink("/proc/self/exe", buf, PATH_MAX);

            if(n > 0)
            {
                buf[n] = '\0';
                dirname(buf);
                strncpy(l_InstallDir, buf, PATH_MAX);

                strncat(buf, "/config/mupen64plus.conf", PATH_MAX - strlen(buf));
                if(!isfile(buf))
                {
                    // try cwd as last resort
                    getcwd(l_InstallDir, PATH_MAX);
                }
            }
            else
            {
                // try cwd as last resort
                getcwd(l_InstallDir, PATH_MAX);
            }
        }
    }

    // make sure install dir has a '/' on the end.
    if(l_InstallDir[strlen(l_InstallDir)-1] != '/')
        strncat(l_InstallDir, "/", PATH_MAX - strlen(l_InstallDir));

    // make sure install dir is valid
    strncpy(buf, l_InstallDir, PATH_MAX);
    strncat(buf, "config/mupen64plus.conf", PATH_MAX - strlen(buf));
    if(!isfile(buf))
    {
        printf("Could not locate valid install directory\n");
        exit(1);
    }

    // check user config dir for mupen64plus.conf file. If it's not there, copy all
    // config files from install dir over to user dir.
    strncpy(buf, l_ConfigDir, PATH_MAX);
    strncat(buf, "mupen64plus.conf", PATH_MAX - strlen(buf));
    if(!isfile(buf))
    {
        DIR *dir;
        struct dirent *entry;

        strncpy(buf, l_InstallDir, PATH_MAX);
        strncat(buf, "config", PATH_MAX - strlen(buf));
        dir = opendir(buf);

        // should never hit this error because of previous checks
        if(!dir)
        {
            perror(buf);
            return;
        }

        while((entry = readdir(dir)) != NULL)
        {
            strncpy(buf, l_InstallDir, PATH_MAX);
            strncat(buf, "config/", PATH_MAX - strlen(buf));
            strncat(buf, entry->d_name, PATH_MAX - strlen(buf));

            // only copy regular files
            if(isfile(buf))
            {
                strncpy(buf2, l_ConfigDir, PATH_MAX);
                strncat(buf2, entry->d_name, PATH_MAX - strlen(buf2));

                printf("Copying %s to %s\n", buf, l_ConfigDir);
                if(copyfile(buf, buf2) != 0)
                    printf("Error copying file\n");
            }
        }

        closedir(dir);
    }

    // set screenshot dir if it wasn't specified by the user
    if (!ValidScreenshotDir())
    {
        char chDefaultDir[PATH_MAX + 1];
        snprintf(chDefaultDir, PATH_MAX, "%sscreenshots/", l_ConfigDir);
        SetScreenshotDir(chDefaultDir);
    }

}

/*********************************************************************************************************
* main function
*/
int main(int argc, char *argv[])
{
    int i;
    printf(" __  __                         __   _  _   ____  _             \n");  
    printf("|  \\/  |_   _ _ __   ___ _ __  / /_ | || | |  _ \\| |_   _ ___ \n");
    printf("| |\\/| | | | | '_ \\ / _ \\ '_ \\| '_ \\| || |_| |_) | | | | / __|  \n");
    printf("| |  | | |_| | |_) |  __/ | | | (_) |__   _|  __/| | |_| \\__ \\  \n");
    printf("|_|  |_|\\__,_| .__/ \\___|_| |_|\\___/   |_| |_|   |_|\\__,_|___/  \n");
    printf("             |_|         http://code.google.com/p/mupen64plus/  \n");
    printf("Version %s\n\n",MUPEN_VERSION);

    MasterServerAddToList("orbitaldecay.kicks-ass.net:8000");
    memset(&l_NetSettings, 0, sizeof(l_NetSettings));
    parseCommandLine(argc, argv);
    if (netInitialize(&g_NetplayClient)) {
    // Use isEnabled to determine whether or not to run the emu in single player mode
    // This should only be set to true if we have netplay started
      g_NetplayClient.isEnabled = 1;
    } else {
      return 0;
    }

    setPaths();
    config_read();
   

    if (l_GuiEnabled)
    {
        i = config_get_bool("OsdEnabled", 2);
        if (i == 2)
        {
            config_put_bool("OsdEnabled", 1);
        }
        else if (g_OsdEnabled == 1)
        {
            g_OsdEnabled = i;
        }
        // check "GUI always start fullscreen" setting in config file
        i = config_get_bool("GuiStartFullscreen", 2);
        if (i == 2)
        {
            config_put_bool("GuiStartFullscreen", 0);
        }
        else if (g_Fullscreen == 0)
        {
            g_Fullscreen = i;
        }

    }

#ifdef VCR_SUPPORT
    VCRComp_init();
    const char *p = config_get_string( "VCR Video Codec", "XviD" );
    for (i = 0; i < VCRComp_numVideoCodecs(); i++)
    {
        if (!strcasecmp( VCRComp_videoCodecName( i ), p ))
        {
            VCRComp_selectVideoCodec( i );
            break;
        }
    }
    p = config_get_string( "VCR Audio Codec", VCRComp_audioCodecName( 0 ) );
    for (i = 0; i < VCRComp_numAudioCodecs(); i++)
    {
        if (!strcasecmp( VCRComp_audioCodecName( i ), p ))
        {
            VCRComp_selectAudioCodec( i );
            break;
        }
    }
#endif

    // init multi-language support
    tr_init();

    // if --noask was not specified at the commandline, default to true in nogui mode, 
    // otherwise check the config file.
    if(!g_NoaskParam)
        {
        if(!l_GuiEnabled)
            {
            g_Noask = TRUE;
            }
        else
            {
            g_Noask = config_get_bool("No Ask", FALSE);
            }
        }

    cheat_read_config();
    plugin_scan_installdir();
    plugin_set_configdir(l_ConfigDir);

#ifndef NO_GUI
    if(l_GuiEnabled)
        gui_init(&argc, &argv);
#endif
    // must be called after building gui
    // look for plugins in the install dir and set plugin config dir
    savestates_set_autoinc_slot(config_get_bool("AutoIncSaveSlot", FALSE));

    if((i=config_get_number("CurrentSaveSlot",10))!=10)
    {
        savestates_select_slot((unsigned int)i);
    }
    else
    {
        config_put_number("CurrentSaveSlot",0);
    }

    main_message(1, 1, 0, 0, tr("Config Dir: \"%s\", Install Dir: \"%s\""), l_ConfigDir, l_InstallDir);

    //The database needs to be opened regardless of GUI mode.
    romdatabase_open();
#ifndef NO_GUI
    // only create the ROM Cache Thread if GUI is enabled
    if (l_GuiEnabled)
    {
        pthread_attr_t tattr;
        int ret;
        int newprio = 80;
        struct sched_param param;
        pthread_attr_init (&tattr);
        pthread_attr_getschedparam (&tattr, &param);
        param.sched_priority = newprio;
        pthread_attr_setschedparam (&tattr, &param);
        g_romcache.rcstask = RCS_INIT;
        if(pthread_create(&g_RomCacheThread, &tattr, rom_cache_system, &tattr)!=0)
            {
            g_RomCacheThread = 0;
            error_message(tr("Couldn't spawn rom cache thread!"));
            }
        else
           pthread_detach(g_RomCacheThread);
    }

    // only display gui if user wants it
    if(l_GuiEnabled)
        gui_display();
#endif

    // if rom file was specified, run it
    if (l_Filename)
    {
        if(open_rom(l_Filename, 0) < 0 && !l_GuiEnabled)
        {
            // cleanup and exit
            cheat_delete_all();
#ifndef NO_GUI
            g_romcache.rcstask = RCS_SHUTDOWN;
#endif
            romdatabase_close();
            plugin_delete_list();
            tr_delete_languages();
            config_delete();
            exit(1);
        }

        startEmulation();
    }
    // Rom file must be specified in nogui mode
    else if(!l_GuiEnabled)
    {
        error_message("Rom file must be specified in nogui mode.");
        printUsage(argv[0]);

        // cleanup and exit
        cheat_delete_all();
        romdatabase_close();
        plugin_delete_list();
        tr_delete_languages();
        config_delete();
        exit(1);
    }

#ifndef NO_GUI
    // give control of this thread to the gui
    if(l_GuiEnabled)
        gui_main_loop();
    // free allocated memory
    if (l_TestShotList != NULL)
        free(l_TestShotList);

    // cleanup and exit
    stopEmulation();
    config_write();
    cheat_write_config();
    cheat_delete_all();
    g_romcache.rcstask = RCS_SHUTDOWN;
    romdatabase_close();
    plugin_delete_list();
    tr_delete_languages();
    config_delete();
#endif
    return EXIT_SUCCESS;
}

