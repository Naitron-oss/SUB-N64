/* =======================================================================================

	network.c
	by orbitaldecay

	Current problems (bugs that need fixing):

	When playing multiplayer, all of the controllers must be enabled in the plugin
	if input is being received over the net.  Haven't found an easy way of fixing
	this yet.

        All clients must be using the same core.

        Obviously all clients must use the same rom, the server doesn't check yet.

   =======================================================================================
*/ 


#include "network.h"


static FILE		*netLog = NULL;
static unsigned char	bNetplayEnabled = 0;

FILE *		getNetLog() {return netLog;}
unsigned short  netplayEnabled() {return bNetplayEnabled;}

unsigned int gettimeofday_msec(void)
{
    struct timeval tv;
    unsigned int foo;
    
    gettimeofday(&tv, NULL);
    foo = ((tv.tv_sec % 1000000) * 1000) + (tv.tv_usec / 1000);
    return foo;
}

/* ======================================================================================

      net_init()

        to be called when program starts


========================================================================================= */

void net_init() {
  netLog = fopen("netlog.txt", "w");
  fprintf(netLog, "Begining net log...\n");
  clientInitialize();
  serverInitialize();
  setEventCounter(0);
  if (SDLNet_Init() < 0) {
     fprintf(netLog, "Failure to initialize SDLNet!\n");
     return;
  }
}

/* ======================================================================================

      netStartNetplay()

        to be called when rom starts


========================================================================================= */

void netStartNetplay() {
  int              n = 0;
  NetPlaySettings  netSettings;
  FILE            *netConfig;

  // Right now we're reading the netplay settings from a conf file, soon this will be part of the GUI
  // ===============================================================================================
  netConfig = fopen("mupennet.conf", "r");
  if (!netConfig) {
     fprintf(netLog, "Failed to open mupennet.conf configuration file.  Playing on local server.\n");
     netSettings.runServer = 1;
  } else {
     fscanf(netConfig, "server: %d\nhost: %s\nport: %d\n", &netSettings.runServer, &netSettings.hostname, &netSettings.port);
     fclose(netConfig);
  }
  // ===============================================================================================
  // Right now we're reading the netplay settings from a conf file, soon this will be part of the GUI

  fprintf(netLog, "runServer %d\nhostname %s\nport %d\n", netSettings.runServer, netSettings.hostname, netSettings.port);


  if (netSettings.runServer) {
      serverStart(SERVER_PORT);
      strcpy(netSettings.hostname, "localhost"); // If we're hosting a game, connect to it.
      netSettings.port = SERVER_PORT;
  }

  if (clientConnect(netSettings.hostname, netSettings.port))
      bNetplayEnabled = 1;
  else {
      fprintf(netLog, "Client failed to connect to a server, playing offline.\n");
      netShutdown();
  }

}
/* ======================================================================================

      netShutdown()

========================================================================================= */

void netShutdown() {
  if (clientIsConnected()) clientDisconnect();
  if (serverIsActive()) serverStop();
  bNetplayEnabled = 0;
  fprintf(netLog, "Goodbye.\n");
  fclose(netLog);
}

/* ======================================================================================

      netInteruptLoop()

          This function is called each time a vertical interupt is issued.

========================================================================================= */

void netInteruptLoop() {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 5000000;
            NetMessage syncMsg;

	    if (clientWaitingForServer()) {
              fprintf(netLog, "Waiting for sync msg...\n");
              osd_render();  // Updating OSD
              SDL_GL_SwapBuffers();

              while (clientWaitingForServer()) {
                    SDL_PumpEvents();
#ifdef WITH_LIRC
                    lircCheckInput();
#endif //WITH_LIRC 
                    if (clientIsConnected()) {
			clientProcessMessages();
		    }
                    if (serverIsActive()) {
			serverAcceptConnection();
			serverProcessMessages();
		    }
//                    nanosleep(&ts, NULL); We can't sleep because the timing is inaccurate
	      }
            }
            else {
                    incEventCounter();
                    if (serverIsActive()) serverProcessMessages();
                    if (clientIsConnected()) {
                         clientProcessMessages();
                         processEventQueue();
                    }
	      if (getEventCounter() % 60 == 0) {
                  if ((clientIsConnected()) && (clientLastSyncMsg() < getEventCounter())) {
                      fprintf(netLog, "Client: Pausing for server now. EventCounter = %d.\n", getEventCounter());
                      clientPauseForServer();
                  }
                  if (serverIsActive()) {
                      syncMsg.type = NETMSG_SYNC;
                      syncMsg.genEvent.timer = getEventCounter();
                      serverBroadcastMessage(&syncMsg);  // Don't bother using serverBroadcastSync to time them
                  }
              }

            }
}







