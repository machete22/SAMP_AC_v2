#include "CRPCCallback.h"
#include "../Utility.h"
#include "../Callback.h"
#include "Network.h"
#include "../CAntiCheatHandler.h"
#include "../GDK/sampgdk.h"
#include "../BanHandler.h"
#include "../../Shared/Network/ACVerifiedPacket.h"

#include <stdio.h>
#include <cstring>

#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef WIN32
typedef unsigned char BYTE;
#endif


void CRPCCallback::Initialize()
{
	// Add RPC Callback functions.
	CRPC::Add(ON_CLIENT_VERIFIED, OnClientVerified);
	CRPC::Add(ON_FILE_EXECUTED, OnFileExecuted);
	CRPC::Add(ON_MD5_CALCULATED, OnMD5Calculated);
	CRPC::Add(ON_FILE_CALCULATED, OnFileCalculated);
	CRPC::Add(ON_IMG_FILE_MODIFIED, OnImgFileModified);
	CRPC::Add(ON_MACRO_DETECTED, OnMacroDetected);
	CRPC::Add(ON_INITIAL_INFO, OnIntialInfoGotten);
	CRPC::Add(ON_TAMPER_ATTEMPT, OnTamperAttempt);
	CRPC::Add(TOGGLE_PAUSE, OnPauseToggled);
	CRPC::Add(TAKE_SCREENSHOT, OnTakeScreenshot);
	CRPC::Add(VERSION_INCOMPAT2, VersionIncompat);
}

RPC_CALLBACK CRPCCallback::VersionIncompat(RakNet::BitStream &bsData, int iExtra)
{
	// Inform the player there version of AC is not compatible with the server.
	char msg[150];

	// Format the message letting the user know their AC version will not work on this server.
	snprintf(msg, sizeof(msg), "{FF0000}Fatal Error:{FFFFFF} The servers Anti-Cheat plugin is not compatible with your version. You must update your anti-cheat at samp-ac.com");

	// Send the message to the user.
	SendClientMessage(iExtra, -1, msg);

	// Close the connection.
	SetTimer(1000, 0, Callback::KickPlayer, (void*)iExtra);
}

void CRPCCallback::ThreadedClientVerify(RakNet::BitStream &bsData, int iExtra)
{
	Utility::Printf("DEBUG: ThreadedClientVerify threaded callback start");
	int benchStart = sampgdk_GetTickCount();

	std::string rawVerifiedP = ACVerifiedPacket::RawVerifiedPacket();

	bool verified = true;

	// Convert to byte
	BYTE md5[16];
	for (int i = 0; i < 16 || verified == false; ++i)
	{
		std::string bt = rawVerifiedP.substr(i * 2, 2);
		md5[i] = static_cast<BYTE>(strtoul(bt.c_str(), NULL, 16));

		// Read what is sent from client in the same order
		BYTE read;
		bsData.Read(read);

		// See if any of the bytes sent from client does not match
		if (read != md5[i])
		{
			verified = false;
		}
	}

	if (verified == true)
	{
		Callback::SetLastTimeVerifiedClient(iExtra);
	}
	Utility::Printf("DEBUG: ThreadedClientVerify threaded callback end: %d", sampgdk_GetTickCount() - benchStart);
}

RPC_CALLBACK CRPCCallback::OnClientVerified(RakNet::BitStream &bsData, int iExtra)
{
	// Calculate verified packet
	int benchStart = sampgdk_GetTickCount();
	Utility::Printf("DEBUG: OnClientVerified callback start");
	boost::thread verify(&CRPCCallback::ThreadedClientVerify, bsData, iExtra);
	Utility::Printf("DEBUG: OnClientVerified callback end: %d", sampgdk_GetTickCount() - benchStart);
}

RPC_CALLBACK CRPCCallback::OnFileExecuted(RakNet::BitStream& bsData, int iExtra)
{
	// Allocate space for the process path and md5 parameters.
	unsigned char processpath[MAX_PATH + 1];
	BYTE md5[16];

	memset(processpath, 0, sizeof(processpath));
	memset(md5, 0, sizeof(md5));

	// Read new value for filename
	if (bsData.ReadString(processpath))
	{
		// Convert the md5 from bytes to char
		char digestChars[33];
		for (int i = 0; i < 16; ++i)
		{
			bsData.Read(md5[i]);
			sprintf(digestChars + (i * 2), "%02X", md5[i]);
		}

		// lower case
		boost::algorithm::to_lower(digestChars);

		// Make sure they've already sent intial info
		CAntiCheat *ac = CAntiCheatHandler::GetAntiCheat(iExtra);
		if (ac)
		{
			// Call the main OnFileExecuted function.
			ac->OnFileExecuted((char*)processpath, (char*)digestChars);
		}
	}
	else
	{
		// user sent an invalid length for the strings

		char str[128];
		snprintf(str, sizeof(str), "(1) AC Error: playerid: %d sent invalid string length to the server. You should report this problem.", iExtra);
		SendClientMessage(iExtra, -1, str);
	}
}

RPC_CALLBACK CRPCCallback::OnMD5Calculated(RakNet::BitStream &bsData, int iExtra)
{
	// Create variables
	int address, size;
	BYTE md5[16];

	// Reset string value
	memset(md5, 0, sizeof(md5));

	// Read values sent from client.
	if (bsData.Read(address) && bsData.Read(size))
	{
		// Convert the md5 from bytes to char
		char digestChars[33];
		for (int i = 0; i < 16; ++i)
		{
			bsData.Read(md5[i]);
			sprintf(digestChars + (i * 2), "%02X", md5[i]);
		}

		// lower case
		boost::algorithm::to_lower(digestChars);

		// Make sure the AC pointer is created
		CAntiCheat *ac = CAntiCheatHandler::GetAntiCheat(iExtra);
		if (ac)
		{
			// Call the main function with the info we got.
			ac->OnMD5Calculated(address, size, digestChars);
		}
	}
}

RPC_CALLBACK CRPCCallback::OnFileCalculated(RakNet::BitStream &bsData, int iExtra)
{
	// Create variables to hold the file path and md5
	unsigned char path[MAX_PATH + 1];
	BYTE md5[16];

	// Reset memory.
	memset(path, 0, sizeof(path));
	memset(md5, 0, sizeof(md5));

	// Read the data the client sent us.
	if(bsData.ReadString(path)) 
	{
		// Convert the md5 from bytes to char
		char digestChars[33];
		for (int i = 0; i < 16; ++i)
		{
			bsData.Read(md5[i]);
			sprintf(digestChars + (i * 2), "%02X", md5[i]);
		}

		// lower case
		boost::algorithm::to_lower(digestChars);

		// Check the CAntiCheat pointer is valid.
		CAntiCheat *ac = CAntiCheatHandler::GetAntiCheat(iExtra);
		if (ac)
		{
			ac->OnFileCalculated((char*)path, digestChars);
		}
	}
	else
	{
		// user sent an invalid length for the strings

		char str[128];
		snprintf(str, sizeof(str), "(2) AC Error: playerid: %d sent invalid string length to the server. You should report this problem.", iExtra);
		SendClientMessage(iExtra, -1, str);
	}
}

RPC_CALLBACK CRPCCallback::OnImgFileModified(RakNet::BitStream &bsData, int iExtra)
{
	// Create variables to hold the file path and md5
	unsigned char path[MAX_PATH + 1];
	BYTE md5[16];

	// Reset memory
	memset(path, 0, sizeof(path));
	memset(md5, 0, sizeof(md5));

	// Read the data sent to us by the server.
	if (bsData.ReadString(path))
	{
		// Convert the md5 from bytes to char
		char digestChars[33];
		for (int i = 0; i < 16; ++i)
		{
			bsData.Read(md5[i]);
			sprintf(digestChars + (i * 2), "%02X", md5[i]);
		}

		// lower case
		boost::algorithm::to_lower(digestChars);

		// Check the CAntiCheat pointer is valid.
		CAntiCheat *ac = CAntiCheatHandler::GetAntiCheat(iExtra);
		if (ac)
		{
			ac->OnImgFileModified((char*)path, digestChars);
		}
	}
}

RPC_CALLBACK CRPCCallback::OnMacroDetected(RakNet::BitStream &bsData, int iExtra)
{
	// Create an int var to hold the virtual key the macro was detected on.
	int vKey;

	// Read the data the client said was a key used by a macro
	if (bsData.Read(vKey))
	{
		// Let our helper class know.
		CAntiCheatHandler::GetAntiCheat(iExtra)->OnMacroDetected(vKey);
	}
}

void SAMPGDK_CALL KickUnverifiedClient(int timerid, void *params)
{
	// Make sure the player is connected.
	int playerid = (int)params;
	if (IsPlayerConnected(playerid))
	{
		char kickmsg[144], name[MAX_PLAYER_NAME];
		GetPlayerName(playerid, name, sizeof name);
		snprintf(kickmsg, sizeof(kickmsg), "Kicking %s (%d) for not verifying anti-cheat client properly.", name, playerid);

		SendClientMessageToAll(0xFF0000FF, kickmsg);
		Utility::Printf(kickmsg);

		SetTimer(1000, 0, Callback::KickPlayer, (void*)playerid);
	}
}

RPC_CALLBACK CRPCCallback::OnIntialInfoGotten(RakNet::BitStream &bsData, int iExtra)
{
	Utility::Printf("DEBUG: CRPCCallback > OnIntialInfoGotten start");
	int benchStart = sampgdk_GetTickCount();
	CAntiCheatHandler::Init(iExtra);

	// Calculate verified packet
	//std::string rawVerifiedP = ACVerifiedPacket::RawVerifiedPacket();

	// Convert to byte
	BYTE md5[16];
	/*bool verified = true;
	for (int i = 0; i < 16; ++i)
	{
		std::string bt = rawVerifiedP.substr(i * 2, 2);
		md5[i] = static_cast<BYTE>(strtoul(bt.c_str(), NULL, 16));

		// Read what is sent from client in the same order
		BYTE read;
		bsData.Read(read);

		// See if any of the bytes sent from client does not match
		if (read != md5[i])
		{
			// Kick the client
			// We delay the kick otherwise GetPlayerName returns incorrect data.
			SetTimer(1000, 0, KickUnverifiedClient, (void*)iExtra);
			verified = false;
			break;
		}
	}

	if (verified == true)
	{
		Callback::SetLastTimeVerifiedClient(iExtra);
	}*/

	Callback::SetLastTimeVerifiedClient(iExtra);

	// Create a big variable to hold hardware ID.
	float version;

	// Convert the md5 from bytes to char
	char digestChars[33];
	for (int i = 0; i < 16; ++i)
	{
		bsData.Read(md5[i]);
		sprintf(digestChars + (i * 2), "%02X", md5[i]);
	}

	// lower case
	boost::algorithm::to_lower(digestChars);

	// Read the hardware ID from the client.
	if (bsData.Read(version))
	{
		// Make sure AC pointer is valid
		CAntiCheat *ac = CAntiCheatHandler::GetAntiCheat(iExtra);
		if (ac)
		{
			// Send to our helper class so it can store it.
			ac->OnHardwareCalculated((char*)digestChars);

			// Check the version compatiblity.
			ac->CheckVersionCompatible(version);
		}
	}
	Utility::Printf("DEBUG: CRPCCallback > OnIntialInfoGotten end: %d", sampgdk_GetTickCount() - benchStart);
}

RPC_CALLBACK CRPCCallback::OnTamperAttempt(RakNet::BitStream &bsData, int iExtra)
{
	CAntiCheat *ac = CAntiCheatHandler::GetAntiCheat(iExtra);
	if (ac)
	{
		ac->OnTamperAttempt();
	}

	// Create packet
	RakNet::BitStream bitStream;

	// Write header to packet
	bitStream.Write((unsigned char)PACKET_RPC);
	bitStream.Write(EXIT_PROCESS);

	Network::PlayerSend(iExtra, &bitStream);
}

RPC_CALLBACK CRPCCallback::OnPauseToggled(RakNet::BitStream &bsData, int iExtra)
{
	// Record the data the client sent us.
	int iType;
	bool bPause;

	// Read the data.
	if (bsData.Read(iType) && bsData.Read(bPause))
	{
		// Make sure AC pointer is valid
		CAntiCheat *ac = CAntiCheatHandler::GetAntiCheat(iExtra);
		if (ac)
		{
			// Call togglepause func
			ac->TogglePause(iType, bPause);
		}
	}
}

RPC_CALLBACK CRPCCallback::OnTakeScreenshot(RakNet::BitStream &bsData, int iExtra)
{
	// Make sure AC pointer is valid.
	CAntiCheat *ac = CAntiCheatHandler::GetAntiCheat(iExtra);
	if (ac)
	{
		ac->OnScreenshotTaken();
	}
}
