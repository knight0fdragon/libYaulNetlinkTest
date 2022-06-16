/*****************************************************************
*
* XBANDLib.h
*
* XBAND Saturn Game Library header file, version 1.00.
*
* This include file should be used with files that call functions
* in the XBAND game libraries.
*
* Copyright (C) 1996, Catapult Entertainment, Inc.
*
*****************************************************************/


#ifndef __XBANDLIB__
#define	__XBANDLIB__

/*
// XBGameType -- returned by XBInitXBAND
*/

typedef unsigned char XBGameType;

enum
{
	XBLocalGame = 0,
	XBNetworkGame = 2
};


/*
// XBGameResults -- passed in to XBNetworkGameOver & XBNetworkGameError
//
// The 32-byte userData area can be set
// to anything the developer wishes.
//
// This information is uploaded to the XBAND server
// and kept by Catapult.
*/

typedef struct
{
	long masterScore;
	long slaveScore;
	char userData[32];
} XBGameResults;


/*
// Game library error codes
*/

typedef short XBErr;

#define	XBNoErr					0
#define XBSessionClosed			-405
#define XBNoDialtone			-416
#define XBConnectionLost		-421
#define XBParityError			-423
#define XBFrameError			-424
#define XBOverrunError			-425
#define XBTimeout				-426
#define	XBBadPacket		 		-601
#define XBNoData				-602

#define XBRemoteDataInTransit	-607

#define XBOutOfSync				-612
#define XBSerialFIFOError		-613
#define XBMismatchedExchangeRate -712
#define XBMismatchedPacketSizes	-713


/*
// XBInfo
// Interesting statistics and information kept by
// the game library. Use XBGetInfo to retrieve.
*/

typedef struct
{
	unsigned short gameDataQueueSize;
	unsigned short roundTripLatency;
	unsigned short packetSize;

	unsigned short errorRecoveriesCount;
	unsigned short badPacketCount;
	unsigned short noDataCount;
	unsigned short redialCount;
	unsigned short overrunErrorCount;
	unsigned short parityErrorCount;
	unsigned short frameErrorCount;
	unsigned long bytesReadCount;
	unsigned long bytesWrittenCount;
	unsigned long packetCount;	/* total number of game data packets sent back and forth */
} XBInfo;


/*
// Defines that create the implementation of these functions.
// All these functions go through a dispatch table.
//
// First we create some macros to call dispatched functions of zero,
// one, two and three variables.
*/


/* what address is the game dispatch table at? */

#define	DISPATCH_TABLE_ADDRESS	0x06002F80

/* For Catapult purposes: our pointer to the dispatch table */

#define	gGameDispatchTable	( (unsigned long *) DISPATCH_TABLE_ADDRESS )



#define	XBLIB_FUNC(f)					(&gGameDispatchTable[f])

#define XBFa(fn,r)						((**(r(**)(void))XBLIB_FUNC(fn))())
#define XBFb(fn,r,t1,a1)				((**(r(**)(t1))XBLIB_FUNC(fn))(a1))
#define XBFc(fn,r,t1,a1,t2,a2)			((**(r(**)(t1,t2))XBLIB_FUNC(fn))(a1, a2))
#define XBFd(fn,r,t1,a1,t2,a2,t3,a3)	((**(r(**)(t1,t2,t3))XBLIB_FUNC(fn))(a1,a2,a3))

typedef void (*XBErrorCallback)(XBErr inError);

/* Functions valid in XOS Simulation Only */

#define XBDebugInit()			XBFa(1,int)

#define XBMakeMaster(a)			XBFb(2,void,const char *,a)
#define XBMakeSlave()			XBFb(2,void,const char *,0)
#define XBMakeLocalGame()		XBFa(5,void)

#define XBLineNoise(a,b,c)		XBFd(4,void,int,a,int,b,int,c)

/* Initialize */

#define XBInitXBAND()			XBFa(9,XBGameType)

/* Vertical Blanking Task */

#define XBVBLTask()				XBFa(23,void)

/* Communications protocol */

#define XBSetErrorCallback(a)	XBFb(19,void,XBErrorCallback,a)

#define XBExchangeGameData(a,b,c) 	XBFd(7,XBErr,const void*,a,void*,b,void*,c)

#define XBOpenSession(a,b)		XBFc(28,XBErr,int,a,int,b)
#define XBCloseSession()		XBFa(15,XBErr)

#define XBGetInfo()				XBFa(8,const XBInfo *)

/* XBAND resources */

#define XBMasterPlayerName()	XBFa(10,const char*)
#define XBSlavePlayerName()		XBFa(11,const char*)
#define XBLocalPlayerName()		XBFa(12,const char*)
#define XBRemotePlayerName()	XBFa(13,const char*)

#define	XBLocalIsMaster()		XBFa(14,int)
#define	XBRemoteIsSlave()		XBFa(14,int)
#define	XBLocalIsSlave()		(!XBRemoteIsSlave())
#define	XBRemoteIsMaster()		(!XBLocalIsMaster())

#define XBGetRandomSeed()		XBFa(16,unsigned long)

#define XBAllowReturnToXOS()	XBFa(24,int)
#define XBHangupModem()			XBFa(29,void)

#define XBNetworkGameError(a,b)	XBFc(18,void,XBGameResults*,a,XBErr,b)
#define XBNetworkGameOver(a)	XBFc(18,void,XBGameResults*,a,XBErr,XBNoErr)

#define XBReadyToExit()			XBFa(30,void)


#endif	/* __XBANDLIB__ */
