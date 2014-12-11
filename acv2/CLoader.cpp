#include "CLoader.h"
#include "../Shared/Network/Network.h"
#include "CParseCommandLine.h"
#include "Network/Network.h"
#include "Addresses.h"
#include "CClientUpdater.h"
#include "Misc.h"
#include "../Shared/MD5_Info/Cmd5Info.h"
#include "CHookManager.h"
#include "VMProtectSDK.h"
#include "VersionHelpers.h"
#include "../Shared/Network/CRPC.h"
#include "CCrashHandler.h"
#include "CLog.h"
#include "Detours\detours.h"
#include "CModuleSecurity.h"

#include <map>
#include <Shellapi.h>
#include <Boost\thread.hpp>

CInjectedLibraries CLoader::Modules = CInjectedLibraries();
CProcessList CLoader::Processes = CProcessList();
CDirectoryScanner CLoader::GtaDirectory = CDirectoryScanner();
bool CLoader::isLoaded = false;
HMODULE CLoader::ThishMod = NULL;

void CLoader::Initialize(HMODULE hMod)
{
	// Install the crash handler.
	new CCrashHandler();

	// Hook LoadLibrary function.
	CModuleSecurity::HookLoadLibrary();

	// Record that we're loaded
	isLoaded = true;

	// Keep track of the hModule.
	ThishMod = hMod;

	// Hook the D3D9Device functions.
	CDirectX::HookD3DFunctions();
	
	// Load the command line in a string (mostly the host, and port so we can connect later)
	std::map < std::string, std::string > cmdline;
	cmdline = CParseCommandLine::parseCmdLine(GetCommandLineA());
	
	// Wait until the game is loaded.
	while (ADDRESS_LOADED < 6)
	{
		// Stop CLEO from loading, and other memory hooks.
		CHookManager::Load();

		//CDirectX::LoadImages();

		// Wait until the game is loaded in an infinite loop.
		Sleep(5);
	}

	CModuleSecurity::AddAllowedModules();
	
	// Make sure we're using the latest version of this mod.
	CClientUpdater::CheckForUpdate(hMod);

	// Force process elevation once the game has loaded. This will terminate the current process and run a new one.
	if (IsWindowsVistaOrGreater())
	{
		CheckElevation();
	}


	// Connect to AC Network.
	Network::Initialize(cmdline["Host"], atoi(cmdline["Port"].c_str()) - 7);
	Network::Connect();

	// Setup memory one more time.
	CHookManager::Load();

	while (true)
	{
		// Scan for new processes.
		Processes.Scan();

		// Scan for new injected modules.
		Modules.Scan();

		// Check for a tamper attempt.
		if (VMProtectIsDebuggerPresent(true) || VMProtectIsVirtualMachinePresent())
		{
			// Make sure the user is connected.
			if (!Network::IsConnected())
			{
				// Close the process.
				ExitProcess(0);
			}

			// Tell the server we've done some naughty stuff.
			Network::SendRPC(ON_TAMPER_ATTEMPT);
		}

		// Sleep
		Sleep(1000);
	}
}

void CLoader::CheckElevation()
{
	// Check if process is elevated already
	if (!IsProcessElevated())
	{
		// If it's not, we need to elevate it.
		// run our elevator .exe program
		RunElevated();
	}
}

void CLoader::RunElevated()
{
	// Set our info to run the samp_elevator.exe
	SHELLEXECUTEINFO sei = { sizeof(sei) };
	sei.lpVerb = "runas";
	sei.lpFile = "samp_elevator.exe";
	sei.nShow = SW_NORMAL;
	sei.lpParameters = GetCommandLineA();

	ShellExecuteEx(&sei);

	// Exit process since we're not elevated, and the elevator.exe will create a new process with elevation permissions.
	ExitProcess(0);
}

BOOL CLoader::IsProcessElevated()
{
	BOOL fIsElevated = FALSE;
	DWORD dwError = ERROR_SUCCESS;
	HANDLE hToken = NULL;

	// Open the primary access token of the process with TOKEN_QUERY.
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Retrieve token elevation information.
	TOKEN_ELEVATION elevation;
	DWORD dwSize;
	if (!GetTokenInformation(hToken, TokenElevation, &elevation,
		sizeof(elevation), &dwSize))
	{
		// When the process is run on operating systems prior to Windows 
		// Vista, GetTokenInformation returns FALSE with the 
		// ERROR_INVALID_PARAMETER error code because TokenElevation is 
		// not supported on those operating systems.
		dwError = GetLastError();
		goto Cleanup;
	}

	fIsElevated = elevation.TokenIsElevated;

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (hToken)
	{
		CloseHandle(hToken);
		hToken = NULL;
	}

	// Throw the error if something failed in the function.
	if (ERROR_SUCCESS != dwError)
	{
		throw dwError;
	}

	return fIsElevated;
}

bool CLoader::IsLoaded()
{
	return isLoaded;
}