/* =======================================================================================

	network.c
	by orbitaldecay

	Alright, so far the most basic network functionality has been included.  It still
	needs to be incorporated in the gui, there are no ROM checks to ensure the clients
	are running the same rom, and various other essential features are lacking; but
	what is here should be enough to enable netplay.  In the running directory of
	mupen64plus is a file titled mupennet.conf.  The first number represents whether
	or not to start the server, the second word is the hostname to connect to and the
	final number is the port to connect on.  For example:

	1 localhost 7000

	Run the server and connect to the localhost on port 7000.

	0 192.168.0.152 7000

	Don't run a server, connect to 192.168.0.152 on port 7000.

	You get the idea.  Once the clients are connected, you must press F9 on the server
	to send a message to all the clients telling them to begin running the ROM, this is
	extremely crude and will be fixed later.  Like I say, this is the most basic functionality
	imaginable, but the code does compensate for network latency, up to 100ms pings should 
	be fine and remain perfectly synchronized.  Email me with any questions or comments
	orbitaldecay@gmail.com.  Thanks ^_^

	Bob

	KNOWN BUGS THAT NEED FIXING:

	When playing multiplayer, all of the controllers must be enabled in the plugin
	if input is being received over the net.  Haven't found an easy way of fixing
	this yet.

   =======================================================================================
*/ 

#include "main.h"
#include "../r4300/r4300.h"
#include "plugin.h"
#include "SDL_net.h"
#include "../opengl/osd.h"
#include "network.h"

/* =======================================================================================

	Globals for use within network.c
	TODO: Actually define scope, don't be lazy!

   =======================================================================================
*/ 

static BUTTONS		netKeys[4];		// Key cache
static unsigned short	SyncCounter = 0;	// Track VI in order to syncrhonize button events over network

static TCPsocket	clientSocket;		// Socket descriptor for connection to server
static SDLNet_SocketSet	clientSocketSet;	// Set for client connection to server
static unsigned char 	bClientIsConnected = 0;	// Are we connected to a server?

static NetEvent	*NetEventQueue = NULL;	// Pointer to queue of upcoming button events
static FILE		*netLog = NULL;

static TCPsocket	serverSocket;		// Socket descriptor for server (if this is one)
static SDLNet_SocketSet	serverSocketSet;	// Set of all connected clients, along with the server socket descriptor

static unsigned char	bServerIsActive = 0;	// Is the server active?
static unsigned char    bWaitForPlayers = 0;
static unsigned char	bNetplayEnabled = 0;

static int		netDelay = 0;

static int		netLag[MAX_CLIENTS];
static TCPsocket	Client[MAX_CLIENTS];

static char PlayerName[20][MAX_CLIENTS] = {"Player 1", "Player 2", "Player 3", "Player 4"};


unsigned int gettimeofday_msec(void)
{
    struct timeval tv;
    unsigned int foo;
    
    gettimeofday(&tv, NULL);
    foo = ((tv.tv_sec % 1000000) * 1000) + (tv.tv_usec / 1000);
    return foo;
}
/* =======================================================================================

	Initialize!

   =======================================================================================
*/ 

void netInitialize() {
  int n = 0, start_server, hostport;
  char hostname[20];

  FILE *netConfig;
  netLog = fopen("netlog.txt", "w");

  netConfig = fopen("mupennet.conf", "r");
  fscanf(netConfig, "server: %d\nhost: %s\nport: %d\n", &start_server, &hostname, &hostport);
  fclose(netConfig);

  fprintf(netLog, "Begining net log...\n");

  fprintf(netLog, "Start_server %d\nHostname %s\nHostport %d\n", start_server, hostname, hostport, netDelay);
  for (n = 0; n < 4; n++) netKeys[n].Value = 0;
  for (n = 0; n < MAX_CLIENTS; n++) Client[n] = 0;
  if (SDLNet_Init() < 0) fprintf(netLog, "Failure to initialize SDLNet!\n");

  // If server started locally, always connect!
  if (start_server) {
    serverStart(SERVER_PORT);
    n = clientConnect("localhost", 7000);
  }
  else n = clientConnect(hostname, hostport);

  if (n) {
	netInitEventQueue();
	bNetplayEnabled = 1;
  }
  else {
    fprintf(netLog, "Client failed to connect to a server, playing offline.\n");
    netShutdown();
  }

}

void netShutdown() {
  clientDisconnect();
  serverStop();
  netKillEventQueue();
  fprintf(netLog, "Goodbye.\n");
  fclose(netLog);
}

/* =======================================================================================

	Access to assorted variables outside of network.c

   =======================================================================================
*/ 


unsigned short  getNetDelay() {return netDelay;}
FILE *		getNetLog() {return netLog;}
unsigned short	getSyncCounter() {return SyncCounter;}
void		setSyncCounter(unsigned short v) {SyncCounter = v;}
unsigned short  incSyncCounter() {SyncCounter++;}
DWORD		getNetKeys(int control) {return netKeys[control].Value;}
void		setNetKeys(int control, DWORD value) {netKeys[control].Value = value;}

unsigned short	clientIsConnected() {return bClientIsConnected;}
unsigned short	serverIsActive() {return bServerIsActive;}
unsigned short  serverWaitingForPlayers() {return bWaitForPlayers;}
unsigned short  netplayEnabled() {return bNetplayEnabled;}

/* =======================================================================================

	netMain: do the regular mainloop stuff

   =======================================================================================
*/

void netInteruptLoop() {
            struct timespec ts;
	    if (serverWaitingForPlayers()) {
              fprintf(netLog, "waiting for signal to begin...\n");

              while (serverWaitingForPlayers()) {
                    osd_render();  // Continue updating OSD
                    SDL_GL_SwapBuffers();
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
                    ts.tv_sec = 0;
                    ts.tv_nsec = 5000000;
                    nanosleep(&ts, NULL); // sleep for 5 milliseconds so it doesn't rail the processor
	      }
            SyncCounter = 0;
            }
            else {
                    incSyncCounter();
                    if (serverIsActive()) serverProcessMessages();
                    if (clientIsConnected()) {
                         clientProcessMessages();
                         netProcessEventQueue();
                    }
            }
}

/* =======================================================================================

	Server routines

   =======================================================================================
*/ 

void serverStop() {
  int n;
  fprintf(netLog, "serverStop() called.\n");
  for (n = 0; n < MAX_CLIENTS; n++) if (Client[n]) serverKillClient(n);
  SDLNet_TCP_Close(serverSocket);
  SDLNet_FreeSocketSet(serverSocketSet);
  bServerIsActive = 0;
}

void serverStopWaitingForPlayers() {
	SDLNet_TCP_Close(serverSocket);
	SDLNet_TCP_DelSocket(serverSocketSet, serverSocket);
}

int serverStart(unsigned short port) {
        IPaddress serverAddr;

	fprintf(netLog, "serverStart() called.\n");
	if (serverIsActive()) serverStop();

	serverSocketSet = SDLNet_AllocSocketSet(MAX_CLIENTS + 1);
	SDLNet_ResolveHost(&serverAddr, NULL, port);
	
	if (serverSocket = SDLNet_TCP_Open(&serverAddr)) {
		SDLNet_TCP_AddSocket(serverSocketSet, serverSocket);
		fprintf(netLog, "Server successfully initialized on port %d.\n", port);
                bServerIsActive = 1;
		bWaitForPlayers = 1;
	}
	else fprintf(netLog, "Failed to initialize server on port %d.\n", port);
	return bServerIsActive;
}

int serverRecvMessage(TCPsocket client, NetMessage *msg) {
	int netRet = 0;
	SDLNet_CheckSockets(serverSocketSet, 0);
	if (SDLNet_SocketReady(client))
		netRet = SDLNet_TCP_Recv(client, msg, sizeof(NetMessage));
	return netRet;
}


int serverSendMessage(TCPsocket client, NetMessage *msg) {
  return SDLNet_TCP_Send(client, msg, sizeof(NetMessage));
}

int serverBroadcastMessage(NetMessage *msg) {
  int n;
  for (n = 0; n < MAX_CLIENTS; n++) if (Client[n]) {
	if (serverSendMessage(Client[n], msg) != sizeof(NetMessage)) {
		serverKillClient(n);
	}
  }
}

void serverKillClient(int n) {
	NetMessage msg;
	SDLNet_TCP_Close(Client[n]);
	SDLNet_TCP_DelSocket(serverSocketSet, Client[n]);
	Client[n] = 0;
	fprintf(netLog, "Client %d disconnected.\n", n);

	msg.type = NETMSG_PLAYERQUIT;
	msg.genEvent.controller = n;
	serverBroadcastMessage(&msg);
	fprintf(netLog, "Broadcast player quit.\n");
}

void serverAcceptConnection() {
  TCPsocket newClient;
  NetMessage msg;

  int n;
  SDLNet_CheckSockets(serverSocketSet, 0);
  if (SDLNet_SocketReady(serverSocket)) {
    if (newClient = SDLNet_TCP_Accept(serverSocket)) {
        for (n = 0; n < MAX_CLIENTS; n++)
          if (!Client[n]) {
            SDLNet_TCP_AddSocket(serverSocketSet, (Client[n] = newClient));
            fprintf(netLog, "New connection accepted; Client %d\n", n);
            if (n > 0) {
		fprintf(netLog, "Sending ping.\n", n);
                msg.type = NETMSG_PING;
                msg.genEvent.type = NETMSG_PING;
                msg.genEvent.value = gettimeofday_msec();
                serverSendMessage(newClient, &msg);
            }
            break;
          }
        if (n == MAX_CLIENTS) SDLNet_TCP_Close(newClient); // No open slots
    }
  }
}

void serverProcessMessages() {
	NetMessage msg, nmsg;
	int recvRet, n, netlag;

	SDLNet_CheckSockets(serverSocketSet, 0);
	for (n = 0; n < MAX_CLIENTS; n++) {
		if (Client[n]) {
			if (SDLNet_SocketReady(Client[n])) {
				if (recvRet = serverRecvMessage(Client[n], &msg)) {
					switch (msg.type) {
						case NETMSG_EVENT:
                                                        if (msg.genEvent.type == NETMSG_BUTTON) {
							    msg.genEvent.timer += netDelay;
							    if (n < 4) {
								msg.genEvent.controller = n;
								serverBroadcastMessage(&msg);
							    }
                                                        }
						break;
						case NETMSG_SETNAME:
							msg.genEvent.controller = n;
							serverBroadcastMessage(&msg);
						break;
						case NETMSG_PING:
						     netLag[n] = (gettimeofday_msec() - msg.genEvent.value);
                                                                // should divide by 2 for one way trip
                                                     fprintf(netLog, "Server: ping received, lag %d\n", netLag[n]);
                                                break;
					}
				} else {
					serverKillClient(n);
				}
			}
		}
	}
}

void serverBroadcastStart() {
  int tc = 0, ptr = 0, n, lc;
  NetMessage startmsg;
  struct timespec ts;

  fprintf(getNetLog(), "Server: F9 pressed\n");

  short sort_array[MAX_CLIENTS];

  for (n = 0; n < MAX_CLIENTS; n++) sort_array[n] = -1;

  // Sort out ping speeds stored in net lag from highest to lowest
  for (n = 0; n < MAX_CLIENTS; n++)
      if  ( (netLag[n] >= netLag[tc])
         && (Client[n]) ) 
            tc = n;

  sort_array[ptr] = tc;
  lc = tc;
  tc = 0;

  for (ptr = 1; ptr < MAX_CLIENTS; ptr++) {
     for (n = 0; n < MAX_CLIENTS; n++) 
          if ( (netLag[n] >= netLag[tc])
            && (netLag[n] <= netLag[lc]) 
            && (n != lc) 
            && (Client[n]))
               tc = n;
     if (tc == lc) break;
     sort_array[ptr] = tc;
     lc = tc;
     tc = 0;
  }

  fprintf(netLog, "Client Lag(ms):\n");
  for (n = 0; n < MAX_CLIENTS; n++) if (Client[n]) fprintf(netLog, "   Player %d: %d\n", sort_array[n]+1, netLag[sort_array[n]]);

  startmsg.type = NETMSG_STARTEMU;
  serverStopWaitingForPlayers();

  netDelay = (netLag[sort_array[0]] / 17) + 7; // 60 VI/s = ~17ms per VI (+ 7 to be safe)
  fprintf(netLog, "Net Delay: %d\n", netDelay);

  // Send STARTEMU signals, in order of slowest to fastest connection, compensate for net lag
  for (n = 0; n < MAX_CLIENTS; n++) {
     ptr = sort_array[n];
     lc  = sort_array[n+1];
     if (ptr >= 0) {
       fprintf(netLog, "Sending start message to Player %d (lag %d ms)\n", ptr + 1, netLag[ptr]);
       serverSendMessage(Client[ptr], &startmsg);
       if (lc >= 0) {
         ts.tv_sec = 0;
         ts.tv_nsec = 1000000 * (netLag[ptr] - netLag[lc]);
         fprintf(netLog, "Sleeping for %d ms\n", netLag[ptr] - netLag[lc]);
         nanosleep(&ts, NULL);
       }
     }
  }
  fprintf(netLog, "Finished sending STARTEMU messages.\n");
}

/* =======================================================================================

	Client routines

   =======================================================================================
*/ 

int clientConnect(char *server, int port) {
	IPaddress serverAddr;
	int n;

	fprintf(netLog, "clientConnect() called.\n");
	SDLNet_ResolveHost(&serverAddr, server, port);
	if (clientSocket = SDLNet_TCP_Open(&serverAddr)) {
		clientSocketSet = SDLNet_AllocSocketSet(1);
		SDLNet_TCP_AddSocket(clientSocketSet, clientSocket);
		bClientIsConnected = 1;
		fprintf(netLog, "Client successfully connected to %s:%d... waiting for msg from server to start\n", server, port);
		bWaitForPlayers = 1;
	} else fprintf(netLog, "Client failed to connected to %s:%d.\n", server, port);
	return bClientIsConnected;
}

void clientDisconnect() {
	fprintf(netLog, "clientDisconnect() called.\n");
	SDLNet_FreeSocketSet(clientSocketSet);
	SDLNet_TCP_Close(clientSocket);
	bClientIsConnected = 0;
}

int clientSendMessage(NetMessage *msg) {
  if (msg) {
    if (clientIsConnected()) {
	if (SDLNet_TCP_Send(clientSocket, msg, sizeof(NetMessage)) != sizeof(NetMessage))
	  clientDisconnect();
    }
  }
}

void clientProcessMessages() {
	NetMessage incomingMessage;
	char announceString[64];
	int n, pn;

        if (!clientIsConnected()) return; // exit now if the client isnt' connected
        SDLNet_CheckSockets(clientSocketSet, 0);
        if (!SDLNet_SocketReady(clientSocket)) return; // exit now if there aren't any messages to fetch.

	if ((n = SDLNet_TCP_Recv(clientSocket, &incomingMessage, sizeof(NetMessage))) == sizeof(NetMessage)) {
		switch (incomingMessage.type) {
			case NETMSG_EVENT:
				if (incomingMessage.genEvent.timer > getSyncCounter()) {
					netAddEvent(incomingMessage.genEvent.type, incomingMessage.genEvent.controller,
						  incomingMessage.genEvent.value,
						  incomingMessage.genEvent.timer);
                                fprintf(netLog, "Event received %d %d (%d)\n", incomingMessage.genEvent.type,
                                                                               incomingMessage.genEvent.timer,
                                                                               getSyncCounter());
				}
				else fprintf(netLog, "Desync Warning!: Event received for %d, current %d\n", 						incomingMessage.genEvent.timer, getSyncCounter());
			break;
                        case NETMSG_STARTEMU:
                                fprintf(netLog, "Client: STARTEMU message received.\n");
                                bWaitForPlayers = 0;
                        break;
			case NETMSG_PLAYERQUIT:
				pn = incomingMessage.genEvent.controller;
				fprintf(netLog, "Player quit announcement %d\n", pn);
				sprintf(announceString, "%s has disconnected.", PlayerName[pn]);
				osd_new_message(OSD_BOTTOM_LEFT, tr(announceString));
			break;
			case NETMSG_PING:
                                fprintf(netLog, "Ping received\n", incomingMessage.genEvent.value);
                                clientSendMessage(&incomingMessage);
			break;
			case NETMSG_SYNC:
                                fprintf(netLog, "Sync packet received (%d)\n", incomingMessage.genEvent.value);
                                setSyncCounter(incomingMessage.genEvent.value);
			break;
			default:
				fprintf(netLog, "Client message type error.  Dropping packet.\n");
			break;

		}
	} else fprintf(netLog, "Client message size error (%d).\n", n);

}

void netSendButtonState(int control, DWORD value) {
  NetMessage msg;
  msg.type = NETMSG_BUTTON;
  msg.genEvent.controller = control;
  msg.genEvent.value = value;
  msg.genEvent.timer = getSyncCounter();
  clientSendMessage(&msg);
}

/* =======================================================================================

	The following functions are used to process the button event queue 

   =======================================================================================
*/ 

// netProcessEventQueue() : Process the events in the queue, if necessary.
void netProcessEventQueue() {
  int			controller, queueNotEmpty;
  DWORD			value;
  unsigned short	timer;
  unsigned short	type;
  
  if (NetEventQueue) {
    queueNotEmpty = getNextEvent(&type, &controller, &value, &timer);
    while ((timer == SyncCounter) && (queueNotEmpty)) {
	switch (type) {
          case NETMSG_BUTTON:     
            netKeys[controller].Value = value;
            break;
       }
       netPopEvent();
       queueNotEmpty = getNextEvent(&type, &controller, &value, &timer);

    }
  }
}

// netAddEvent() : Add a new button event to the button event queue.
void netAddEvent(unsigned short type, int controller, DWORD value, unsigned short timer) {
  NetEvent *newEvent, *currEvent;

  newEvent = malloc(sizeof(NetEvent)); // TODO: Check for fail, even if it is unlikely
  newEvent->type = type;
  newEvent->controller = controller;
  newEvent->value = value;
  newEvent->timer = timer;
  newEvent->next = NULL;

 // TODO: Make sure queue is in order (lowest timer to highest timer) the packets may arrive out of order
  if (NetEventQueue) {
	currEvent = NetEventQueue;
	while(currEvent->next) {currEvent = currEvent->next;}
	currEvent->next = newEvent;
  }
  else {
	NetEventQueue = newEvent;
  }
}

// netPopEvent() : Remove the button event in the front of the queue.
void netPopEvent() {
  NetEvent *temp = NetEventQueue;

  if (NetEventQueue) {
	NetEventQueue = NetEventQueue->next;
	free(temp);
  }
}

// getNextEvent() : Retrieve information about the button event in the front of the queue.
int getNextEvent(unsigned short *type, int *controller, DWORD *value, unsigned short *timer) {
  int retValue = 1;
  NetEvent *currEvent = NetEventQueue;

  if (NetEventQueue) {
	while (NetEventQueue->timer < getSyncCounter()) {
		fprintf(netLog, "Desync Warning!: Event queue out of date (%d curr %d)! Popping next.\n",
			NetEventQueue->timer, getSyncCounter());
                netPopEvent();
	}
        *type = NetEventQueue->type;
	*controller = NetEventQueue->controller;
	*value = NetEventQueue->value;
	*timer = NetEventQueue->timer;
  }
  else {
    retValue = 0;
  }
  return retValue;
}

void netInitEventQueue() {
  SyncCounter = 0;
  while (NetEventQueue) netPopEvent();
  fprintf(netLog, "Event queue initialized.\n");
}

void netKillEventQueue() {
  while (NetEventQueue) netPopEvent();
  fprintf(netLog, "Event queue killed.\n");
}
  
