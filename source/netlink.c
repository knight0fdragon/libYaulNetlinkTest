/*****************************************************************
*
* NetGame.c
*
* Sample game using XBAND Saturn Game Library. Version 1.00.
*
* This is sample code for a simple XBAND-compatible game.
* It uses most of the XBAND library functions, and uses
* XOS Simulation Mode if necessary.
*
* The master's phone number is HARD-CODED so that we wouldn't
* have to write a numerical input routine. You will need to change
* this number and recompile to test a network game.
*
* Copyright (C) 1996, Catapult Entertainment, Inc.
*
*****************************************************************/

/* Sega Saturn includes */
#include <yaul.h>
#include <stdio.h>
#include <stdlib.h>
#include "XBand/XBANDLIB.H"



void NetGame(void);

void main() { NetGame(); }

/* Change the next line to a phone number that you have control over */

const char phoneNumber[32] = "2\0phone number here";

/* Globals */

volatile unsigned long gTimer;

/* Sega Saturn controller buttons */

const unsigned kRIGHT = 1<<15;
const unsigned kLEFT = 1<<14;
const unsigned kDOWN = 1<<13;
const unsigned kUP = 1<<12;
const unsigned kButtonStart = 1<<11;
const unsigned kButtonA = 1<<10;
const unsigned kButtonC = 1<<9;
const unsigned kButtonB = 1<<8;

const unsigned kButtonR = 1<<7;
const unsigned kButtonX = 1<<6;
const unsigned kButtonY = 1<<5;
const unsigned kButtonZ = 1<<4;
const unsigned kButtonL = 1<<3;

typedef struct
{
	char id;
	char size;
	short data;
} SmpJoyData;

/* some timeouts, measured in ticks (tick = 1/60th of a second) */

const int kGameEndingTimeout = 128;
const int kPlayAgainTimeout = 400;
const int kReadyToPlayTimeout = 120;

typedef unsigned short joypad_state;

/* Enums for the game mode. */

typedef enum
{
	kDemoMode,
	kGameMode,
	kGameEnding,
	kPlayAgain		/* asking if users want to play another network game */
} GameMode;

/* XBAND-related information. */

#define XBMaxNameSize 34

typedef struct
{
	XBGameType		gameType;
	XBGameResults	gameResults;
	int				gameDataSize;
	unsigned int	ticksPerFrame;
	int				needToOpenSession;
	char			p1Name[XBMaxNameSize];
	char			p2Name[XBMaxNameSize];
} NetworkInfo;


/* Used for blinking "Yes/No" selection when */
/* asking if users want to play another network game */

typedef enum
{
	kChoosingNo,
	kChoosingYes,
	kChosenNo,
	kChosenYes
} YesNoChoice;


/* This game state is explicitly passed around to all the functions that need it */
/* It's indended to minimize globals */

typedef struct
{
	NetworkInfo		netInfo;
	GameMode		gameMode;
	/* used as a timeout during some modes */
	int				modeTimeout;
	/* used during the game */
	int				p1Score;
	int				p2Score;
	int				p1Wins;
	int				p2Wins;
	/* used during "Play again?" */
	YesNoChoice		masterChoice;
	YesNoChoice		slaveChoice;
	/* joypad states */
	joypad_state	p1Pad;
	joypad_state	p2Pad;
	joypad_state	bothPads;
	joypad_state	p1PadDown;
	joypad_state	p2PadDown;
	joypad_state	bothPadsDown;
	joypad_state	oldP1Pad;
	joypad_state	oldP2Pad;
} GameState;


/* During a network game, the users can change the game data */
/* packet size. This is for testing only. */

const int kMinGameDataSize = 4;
const int kMaxGameDataSize = 14;

/* Game data struct. This is the packet that is passed into XBExchangeGameData */

typedef struct
{
	joypad_state joypad;
	short finished;
	long frameCount;
	char checksum;
	char padding[14];	/* to make room for game data packets up to 14 bytes in length */
} GameData;
static void DBG_ClearScreen()
{
	//dbgio_printf("[2J");
}

static void DBG_SetCursol(int x, int y)
{
	dbgio_printf("[%d;%dH",y,x);
}


/*
//
// This function is called in v-blank out.
//
*/

static void GameVblankOut(void *work __unused)
{
	smpc_peripheral_intback_issue();
	gTimer++;
	XBVBLTask();	/* we should call XBDebugInit before calling XBVBLTask! */
}


/*
//
// Wait for v-blank out.
//
*/

static void WaitForVBLOut(void)
{
	unsigned long now;

	now = gTimer;

	while (now == gTimer)
	{
	};
}


/*
//
// This function turns on VBL-out routines and starts reading the joypads.
//
*/

static void TurnOnVBLs(void)
{
	gTimer = 0;
	
        vdp_sync_vblank_out_set(GameVblankOut, NULL);
	
	WaitForVBLOut();
	smpc_peripheral_init();
}


/*
//
// This function reads the physical joypads.
//
// WARNING: This code appears to work, but
//	should NOT be used as a good example
//	of how to read the joypads. It is kept small
//	and simple because the focus of this sample code is
//	to be an example of how to use the game library --
//	NOT how to read and deal with the many different
//	kinds of Sega peripherals.
//
*/

static void GetJoypads(joypad_state *pad1, joypad_state *pad2)
{
	static smpc_peripheral_digital_t _digital;
	smpc_peripheral_process();

    	smpc_peripheral_digital_port(1, &_digital);
	*pad1 = 0xffff ^ _digital.pressed.raw;
	
	smpc_peripheral_digital_port(2, &_digital);
	*pad2 = 0xffff ^ _digital.pressed.raw;
}


/*
//
// This function builds XBAND game results out of the current game state.
//
*/

static void BuildGameResults(GameState *theState, XBGameResults *theResults)
{
	int i;

	theResults->masterScore = theState->p1Score;
	theResults->slaveScore = theState->p2Score;

	/* userData can be used for anything the game developer desires */
	/* It will be uploaded to the Catapult server */
	/* We'll just fill it with the values 0 through 31 */

	for (i = 0; i<32; i++)
		theResults->userData[i] = i;
}


/*
//
// This function checks XBAND library calls for errors and does a reasonable thing
// if an error is returned -- namely, aborts the current game.
//
*/

static void HandleXBErr(GameState *theState, XBErr inErr)
{
	XBGameResults gameResults;
	unsigned long waitUntil;

	if (inErr==XBNoErr)
		return;

	BuildGameResults(theState, &gameResults);

	DBG_ClearScreen();

	DBG_SetCursol(1,2);
	dbgio_printf("Sorry, your game could not be\n");
	dbgio_printf("  completed because we lost the\n");
	dbgio_printf("  connection to your opponent.\n\n");
	dbgio_printf("          Error code = %d.\n", inErr);

	XBNetworkGameError(&gameResults, inErr);

	waitUntil = gTimer + 300; /* 300 ticks = 5 seconds */

	/* wait at least five seconds before exiting */
	/* so the user can read the onscreen message */

	while (gTimer < waitUntil)
	{
	};

	exit(0);	/* never returns -- reboots to XBAND OS */
}


/*
//
// This function asks the user whether to try master, slave, or local game.
//
*/

static void DetermineXBANDRole(void)
{
	joypad_state pad1, pad2, bothPads;

	DBG_ClearScreen();

	DBG_SetCursol(1,2);
	dbgio_printf("Type one of start, A, B, C\n\n");
	dbgio_printf(" start: pretend no XBAND installed\n A: be master, dial %s\n B: be slave\n C: local game\n", phoneNumber);

	do
	{
		/* wait for vertical blank */
		WaitForVBLOut();

		GetJoypads(&pad1, &pad2);
		bothPads = pad1 | pad2;

		if (bothPads & kButtonStart)
		{
			break;	/* no XBAND whatsoever */
		}

		if (bothPads & kButtonA)
		{
			dbgio_printf("\n\n Dialing %s, hang on...\n", phoneNumber);
			XBMakeMaster(phoneNumber);
			break;
		}
		if (bothPads & kButtonB)
		{
			dbgio_printf("\n\n Waiting for call...\n");
			XBMakeSlave();
			break;
		}
		if (bothPads & kButtonC)
		{
			XBMakeLocalGame();
			break;
		}
	} while (1);


	/* Wait for users to release pad buttons */

	do
	{
		WaitForVBLOut();
		GetJoypads(&pad1, &pad2);
		bothPads = pad1 | pad2;
	} while ((bothPads & (kButtonA | kButtonB | kButtonC | kButtonStart)) != 0);

	DBG_ClearScreen();
}


/*
//
// This function prints an error message on the screen, if it seems warranted.
//
*/

static void PrintErrorMessage(XBErr inErr)
{
	if (inErr == XBRemoteDataInTransit)
		return;

	DBG_SetCursol(2, 20);
	if (inErr != XBNoErr)
		dbgio_printf("Error code %d: trying to recover...", inErr);
	else
		dbgio_printf("                                        ");

	DBG_SetCursol(2, 7);

	switch (inErr)
	{
		/* This may take a long time! */
		case XBConnectionLost:
			if (XBLocalIsMaster())
				dbgio_printf("Redialing...               \n");
			else
				dbgio_printf("Waiting for call...        \n");
			break;

		/* This may take a second or two. */
		case XBBadPacket:
			dbgio_printf("Line noise, hang on...     \n");
			break;

		/* You may wish to just ignore this error. */
		case XBNoData:
			dbgio_printf("Waiting for data...        \n");
			break;

		/* It's probably best to simply ignore this error. */
		case XBRemoteDataInTransit:
			dbgio_printf("Waiting for initial data...\n");
			break;

		default:
		case XBNoErr:
			dbgio_printf("                             ");
	};
}


/*
//
// This function is called when the remote user decided he doesn't want
// to play another network game.
//
*/

static void RemoteChoseNo(void)
{
	unsigned long waitUntil;

	DBG_ClearScreen();
	DBG_SetCursol(1, 10);
	dbgio_printf("'%s' didn't want to play again.\n", XBRemotePlayerName());

	waitUntil = gTimer + 300; /* 300 ticks = 5 seconds */

	XBCloseSession(); /* XBCloseSession may take a few seconds */

	/* wait at least five seconds before exiting */
	/* so the user can read the onscreen message */

	while (gTimer < waitUntil)
	{
	};

	XBReadyToExit();
	exit(0);
}


/*
//
// This function is called when the local user decided he doesn't want
// to play another network game.
//
*/

static void LocalChoseNo(void)
{
	XBCloseSession();
	XBReadyToExit();
	exit(0);
}


/*
//
// This function sets up the game state for demo mode.
//
*/

static void InitDemoMode(GameState *theState)
{
	DBG_ClearScreen();
	theState->gameMode = kDemoMode;
	theState->modeTimeout = 0;
}


/*
//
// This function sets up the game state for a "new game".
//
*/

static void InitPlayGame(GameState *theState)
{
	DBG_ClearScreen();
	theState->gameMode = kGameMode;
	theState->p1Score = 0;
	theState->p2Score = 0;
	theState->p1Wins = 0;
	theState->p2Wins = 0;
	theState->modeTimeout = 0;
}



/*
//
// This function sets up the game state for "game over" mode.
//
*/

static void InitGameEnding(GameState *theState)
{
	theState->gameMode = kGameEnding;
	theState->modeTimeout = kGameEndingTimeout;
}



/*
//
// This function sets up the game state for "Play again?" mode.
//
*/

static void InitPlayAgain(GameState *theState)
{
	DBG_ClearScreen();

	theState->gameMode = kPlayAgain;
	theState->modeTimeout = kPlayAgainTimeout;

	theState->masterChoice = kChoosingNo;
	theState->slaveChoice = kChoosingNo;
}



/*
//
// This function actually advances the game state "one frame" during demo mode.
//
*/

static void AdvanceDemoMode(GameState *theState)
{
	DBG_SetCursol(4, 2);
	dbgio_printf("Press <start> to begin game");

	DBG_SetCursol(12, 5);

	/* make it blink */

	if (gTimer & 0x20)
		dbgio_printf("DEMO MODE");
	else
		dbgio_printf("         ");

	if (XBAllowReturnToXOS())
	{
		DBG_SetCursol(2,9);
		dbgio_printf("Press L & R to return to XBAND");
		if (((theState->p1Pad & (kButtonL | kButtonR)) == (kButtonL | kButtonR)) 
			|| ((theState->p2Pad & (kButtonL | kButtonR)) == (kButtonL | kButtonR)))
		{
			/* Give the game library a chance to do clean up */
			XBReadyToExit();

			/* Return to XBAND */
			exit(0);
		}
	}

	if ((theState->bothPads) & kButtonStart)
		InitPlayGame(theState); /* switch to game play */
}


/*
//
// This function actually advances the game state "one frame" during game play.
// The game presented here isn't very fun.
//
*/

static void AdvanceGame(GameState *theState)
{
	XBGameResults results;
	XBErr theErr;

	if (theState->p1PadDown & kButtonA)
	{
		theState->p1Score++;
	}

	if (theState->p1PadDown & kButtonB)
	{
		if (--theState->p1Score < 0)
			theState->p1Score = 0;
	}

	if (theState->p2PadDown & kButtonA)
	{
		theState->p2Score++;
	}

	if (theState->p2PadDown & kButtonB)
	{
		if (--theState->p2Score < 0)
			theState->p2Score = 0;
	}

	if ((theState->p1Score >= 5) || (theState->p2Score >= 5))
	{
		if (theState->p1Score >= 5)
			theState->p1Wins++;
		else
			theState->p2Wins++;
		theState->p1Score = 0;
		theState->p2Score = 0;
		if ((theState->p1Wins > 3) || (theState->p2Wins > 3))
		{
			if (theState->netInfo.gameType == XBNetworkGame)
			{
				/* Commit game results */
				BuildGameResults(theState, &results);
				XBNetworkGameOver(&results);
			}
			/* transfer control to "game over" */
			InitGameEnding(theState);
		}
	}

	theState->bothPadsDown = theState->p1PadDown | theState->p2PadDown;

	if (theState->netInfo.gameType == XBNetworkGame)
	{
		if (XBLocalIsMaster())
		{
			if (theState->p1PadDown & kButtonC)
			{
				XBLineNoise(20, 18, 5);
				/* simulate line errors on the master */
			}
			if (theState->p1PadDown & kButtonStart)
			{
				theState->netInfo.needToOpenSession = 1;
				/* open a new session */
			}
		}
		else
		{
			if (theState->p2PadDown & kButtonC)
			{
				XBLineNoise(20, 18, 5);
				/* simulate line errors on the slave*/
			}
		}

		if (theState->bothPadsDown & kButtonZ)
		{
			/* flush data request */
			theErr = XBCloseSession();
			if (theErr != XBNoErr)
				return;
			HandleXBErr(theState, theErr);
			theState->netInfo.needToOpenSession = 1;
		}

		if (theState->bothPadsDown & kButtonL)
		{
			if (--theState->netInfo.ticksPerFrame <= 0)
				theState->netInfo.ticksPerFrame = 1;
			theState->netInfo.needToOpenSession = 1;
		}

		if (theState->bothPadsDown & kButtonR)
		{
			if (++theState->netInfo.ticksPerFrame > 30)
				theState->netInfo.ticksPerFrame = 30;
			theState->netInfo.needToOpenSession = 1;
		}

		if (theState->bothPadsDown & kButtonX)
		{
			if (--theState->netInfo.gameDataSize > kMaxGameDataSize)
				theState->netInfo.gameDataSize = kMaxGameDataSize;
			theState->netInfo.needToOpenSession = 1;
		}

		if (theState->bothPadsDown & kButtonY)
		{
			if (++theState->netInfo.gameDataSize < kMinGameDataSize)
				theState->netInfo.gameDataSize = kMinGameDataSize;
			theState->netInfo.needToOpenSession = 1;
		}
	}

	/* Update screen */

	DBG_SetCursol(2, 3);
	dbgio_printf("%s", theState->netInfo.p1Name);

	DBG_SetCursol(22, 3);
	dbgio_printf("%s", theState->netInfo.p2Name);

	DBG_SetCursol(2, 4);
	dbgio_printf("Score : %d", theState->p1Score);

	DBG_SetCursol(22, 4);
	dbgio_printf("Score : %d", theState->p2Score);

	DBG_SetCursol(3, 5);
	dbgio_printf("Wins : %d", theState->p1Wins);

	DBG_SetCursol(23, 5);
	dbgio_printf("Wins : %d", theState->p2Wins);

	DBG_SetCursol(0, 10);
	dbgio_printf("  A: add a point    B: subtract a point\n");

	/* Display network game specific stuff */

	if (theState->netInfo.gameType == XBNetworkGame)
	{
		dbgio_printf("  C: sim line error Z: flush data\n\n");
		dbgio_printf("  L: -- exch rate    R: ++ exch rate\n");
		dbgio_printf("         Current rate: %d \n\n", theState->netInfo.ticksPerFrame);
		dbgio_printf("  X: -- packet size  Y: ++ packet size\n");
		dbgio_printf("         Current size: %d \n", theState->netInfo.gameDataSize);
	}
}


/*
//
// This function actually advances the game state "one frame" during the flashing
// "game over".
//
*/

static void AdvanceGameEnding(GameState *theState)
{
	DBG_SetCursol( 16, 19 );
	if (gTimer & 0x20)
		dbgio_printf("Game over");
	else
		dbgio_printf("         ");

	if (--theState->modeTimeout == 0)
	{
		theState->gameMode = kDemoMode;
		if (theState->netInfo.gameType == XBNetworkGame)
		{
			InitPlayAgain(theState);
		}
		DBG_ClearScreen();
	}
}


/*
//
// This function actually advances the game state "one frame" during the 
// "play again?" screen.
//
*/

static void AdvancePlayAgain(GameState *theState)
{
	YesNoChoice localChoice, remoteChoice;
	int drawNo, drawYes;

	DBG_SetCursol(1, 5);
	dbgio_printf("Do you wish to play\n  '%s' again?\n This game will not count towards your stats.\n",
		XBRemotePlayerName());

	/* only display local selection */

	if (XBLocalIsMaster())
	{
		localChoice = theState->masterChoice;
		remoteChoice = theState->slaveChoice;
	}
	else
	{
		remoteChoice = theState->masterChoice;
		localChoice = theState->slaveChoice;
	}

	/* Blink the currently selected word. If selection has already */
	/* been made, only draw the chosen work. */

	drawNo = 1;
	drawYes = 1;

	if ((localChoice == kChoosingNo) && (gTimer & 0x20)) /* blink No if selected*/
		drawNo = 0;

	if (localChoice == kChosenYes)
		drawNo = 0;

	if ((localChoice == kChoosingYes) && (gTimer & 0x20)) /* blink Yes if selected */
		drawYes = 0;

	if (localChoice == kChosenNo)
		drawYes = 0;

	DBG_SetCursol(8, 9);
	if (drawNo)
		dbgio_printf("No ");
	else
		dbgio_printf("   ");

	DBG_SetCursol(28, 9);
	if (drawYes)
		dbgio_printf("Yes");
	else
		dbgio_printf("   ");

	/* update master selection */

	if ((theState->masterChoice == kChoosingYes) && (theState->p1PadDown == kLEFT))
		theState->masterChoice = kChoosingNo;

	if ((theState->masterChoice == kChoosingNo) && (theState->p1PadDown == kRIGHT))
		theState->masterChoice = kChoosingYes;

	if ((theState->masterChoice == kChoosingYes) && (theState->p1PadDown == kButtonStart))
		theState->masterChoice = kChosenYes;

	if ((theState->masterChoice == kChoosingNo) && (theState->p1PadDown == kButtonStart))
		theState->masterChoice = kChosenNo;

	/* update slave selection */

	if ((theState->slaveChoice == kChoosingYes) && (theState->p2PadDown == kLEFT))
		theState->slaveChoice = kChoosingNo;

	if ((theState->slaveChoice == kChoosingNo) && (theState->p2PadDown == kRIGHT))
		theState->slaveChoice = kChoosingYes;

	if ((theState->slaveChoice == kChoosingYes) && (theState->p2PadDown == kButtonStart))
		theState->slaveChoice = kChosenYes;

	if ((theState->slaveChoice == kChoosingNo) && (theState->p2PadDown == kButtonStart))
		theState->slaveChoice = kChosenNo;

	if (--theState->modeTimeout == 0)
	{
		if ((theState->masterChoice == kChoosingYes) || (theState->masterChoice == kChoosingNo))
			theState->masterChoice = kChosenNo;

		if ((theState->slaveChoice == kChoosingYes) || (theState->slaveChoice == kChoosingNo))
			theState->slaveChoice = kChosenNo;
	}

	if (remoteChoice == kChosenNo)
		RemoteChoseNo();

	if (localChoice == kChosenNo)
		LocalChoseNo();

	if ((localChoice == kChosenYes) && (remoteChoice == kChosenYes))
		InitPlayGame(theState);
}


/*
//
// This function initializes some library stuff and
// asks the user whether to be master, slave or local game
// if necessary.
//
*/

static void Initialize(GameState *theState)
{
	const int	kInitialSwapRate = 2;
	const int	kInitialGameDataSize = 12;
	int			XOSIsAbsent;
	int			iii;

	const char *player1Name, *player2Name;
	const char name1[] = "Player 1";
	const char name2[] = "Player 2";

	DBG_ClearScreen();
	DBG_SetCursol ( 1, 2 );

	dbgio_printf("Sample game: NetGame");
	vdp2_sync();
        vdp2_sync_wait();
        dbgio_flush();
		
	XOSIsAbsent = !gGameDispatchTable[1] || XBDebugInit();

	TurnOnVBLs();

	if (XOSIsAbsent)
		DetermineXBANDRole();	/* Ask user: "Are we master, slave or local game?" */

	theState->netInfo.gameType = XBInitXBAND();
	theState->netInfo.gameDataSize = kInitialGameDataSize;
	theState->netInfo.ticksPerFrame = kInitialSwapRate;
	theState->netInfo.needToOpenSession = 1;	/* we need to initialize a new session */

	theState->p1Pad = 0;
	theState->p2Pad = 0;

	if (theState->netInfo.gameType == XBNetworkGame)
	{
		player1Name = XBMasterPlayerName();
		player2Name = XBSlavePlayerName();
	}
	else
	{
		player1Name = name1;
		player2Name = name2;
	}

	/* Copy the player's names */

	for (iii=0; iii<XBMaxNameSize; iii++)
	{
		theState->netInfo.p1Name[iii] = player1Name[iii];
		theState->netInfo.p2Name[iii] = player2Name[iii];
	}

	if (theState->netInfo.gameType == XBLocalGame)
	{
		InitDemoMode(theState);
	}


	if (theState->netInfo.gameType == XBNetworkGame)
	{
		InitPlayGame(theState);		/* skip demo mode in a network game */

		DBG_SetCursol(2, 8);
		dbgio_printf("Random number seed is %d\n\n", XBGetRandomSeed());
		dbgio_printf("   I am the ");
		if (XBLocalIsMaster())
			dbgio_printf("master\n");
		else
			dbgio_printf("slave\n");

		dbgio_printf("\n  Get ready to play '%s'!\n", XBRemotePlayerName());

		for (iii = 0; iii<kReadyToPlayTimeout; iii++)
			WaitForVBLOut();		/* wait a bit before clearing screen */

		DBG_ClearScreen();
	}
}


/*
//
// This function is the game's main loop. It never exits.
//
*/

static void MainLoop(GameState *theState)
{
	unsigned long lastSwapTime;
	XBErr err;
	joypad_state localJoypad1, localJoypad2;
	joypad_state masterPad, slavePad;
	GameData localGameData, masterGameData, slaveGameData;
	int counter;

	counter = 0;

	lastSwapTime = gTimer;
	for (uint32_t i = 0; i < 1000; i++)
	{
		dbgio_flush();
		vdp2_sync();
		vdp2_sync_wait();
	}
	XBSetErrorCallback(PrintErrorMessage);

	while (1)
	{
		dbgio_flush();
		vdp2_sync();
		vdp2_sync_wait();
		/* Wait, as we don't want to call XBExchangeData too quickly */
		/* However, even if we do, XBExchangeData will automatically */
		/* wait enough time, so really these lines aren't necessary. */
		/* It simply reduces the incidence of "XBNoData" errors. */

		while (gTimer - lastSwapTime < theState->netInfo.ticksPerFrame)
		{
		};

		/* read the hardware joypads */

		GetJoypads(&localJoypad1, &localJoypad2);

		/* If we're in a network game, we've got to do some communications */

		if (theState->netInfo.gameType == XBNetworkGame)
		{
			localGameData.joypad = localJoypad1;
			localGameData.finished = 0;
			localGameData.frameCount = counter++;

			localGameData.checksum = (theState->p1Score * 64 + theState->p2Score * 16
				+ theState->p1Wins * 4 + theState->p2Wins);

			/* Open the session if necessary */

			if (theState->netInfo.needToOpenSession)
			{
				theState->netInfo.needToOpenSession = 0;

				DBG_SetCursol(2, 7);
				dbgio_printf("Measuring line connection quality...");

				err = XBOpenSession(theState->netInfo.gameDataSize, theState->netInfo.ticksPerFrame);
				if (err == XBOutOfSync)
					continue;
				HandleXBErr(theState, err);

				DBG_SetCursol(2, 7);
				dbgio_printf("                                    ");
			}

			err = XBExchangeGameData(&localGameData, &masterGameData, &slaveGameData);
			if (err == XBSessionClosed)
			{
				/* this error means one side closed and the other did not */
				theState->netInfo.needToOpenSession = 1;
				continue;
			}
			HandleXBErr(theState, err);

			/* During development, send a checksum of game state */
			/* (like sum of object X & Y positions) */
			/* to remote and ensure that they are the same. */
			/* This way, you can halt early if the two worlds diverge. */

			/* Sync-sniffer check, for development */

			DBG_SetCursol(1, 22);
			dbgio_printf("Master : %d       Slave : %d   ", masterGameData.frameCount, slaveGameData.frameCount);

			DBG_SetCursol(10, 23);
			dbgio_printf("Master - slave = %d    ", masterGameData.frameCount - slaveGameData.frameCount);

			DBG_SetCursol(10, 24);
			dbgio_printf("Sync-sniffer?");
			if (masterGameData.checksum == slaveGameData.checksum)
			{
				dbgio_printf(" OK ");
			}
			else
			{
				dbgio_printf(" **** BAD **** ");
			}

			masterPad = masterGameData.joypad;
			slavePad = slaveGameData.joypad;
		}
		else /* just read the local Joypads */
		{
			masterPad = localJoypad1;
			slavePad = localJoypad2;
		}

		/* We now have joypad values in masterPad and slavePad. */

		/* Update all the joypad fields */

		theState->oldP1Pad = theState->p1Pad;
		theState->oldP2Pad = theState->p2Pad;

		theState->p1Pad = masterPad;
		theState->p2Pad = slavePad;
		theState->bothPads = masterPad | slavePad;

		theState->p1PadDown = theState->p1Pad & (~theState->oldP1Pad);
		theState->p2PadDown = theState->p2Pad & (~theState->oldP2Pad);
		theState->bothPadsDown = theState->p1PadDown | theState->p2PadDown;

		/* Now make a note of the current time. This is done after XBExchangeGameData */
		/* rather than before since XBExchangeGameData may take a long time */

		lastSwapTime = gTimer;

		/* Advance the game a frame */

		switch (theState->gameMode)
		{
			case kDemoMode:
				AdvanceDemoMode(theState);
				break;

			case kGameMode:
				AdvanceGame(theState);
				break;

			case kGameEnding:
				AdvanceGameEnding(theState);
				break;

			case kPlayAgain:
				AdvancePlayAgain(theState);
				break;

			default:
				break;
		}
	};
}

void NetGame(void)
{
	GameState theState;
	Initialize(&theState);
        dbgio_flush();
	vdp2_sync();
	vdp2_sync_wait();
	MainLoop(&theState);
}

void user_init(void)
{
	cpu_intc_mask_set(0);
	vdp_sync_vblank_in_clear();
	vdp_sync_vblank_out_clear();
        vdp2_tvmd_display_res_set(VDP2_TVMD_INTERLACE_NONE, VDP2_TVMD_HORZ_NORMAL_A,
            VDP2_TVMD_VERT_240);
        vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x01FFFE), COLOR_RGB1555(1, 0, 3, 15));
	vdp2_tvmd_display_clear();
        vdp2_tvmd_display_set();
	
        
	dbgio_init();
        dbgio_dev_default_init(DBGIO_DEV_VDP2_ASYNC);
        dbgio_dev_font_load();
}