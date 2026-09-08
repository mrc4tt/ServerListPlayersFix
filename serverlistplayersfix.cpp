/**
 * =============================================================================
 * ServerListPlayersFix
 * Copyright (C) 2024 Poggu
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "serverlistplayersfix.h"
#include <iserver.h>
#include <steam/steam_gameserver.h>
#include "utils/module.h"
#include "schemasystem/schemasystem.h"
#include "cs2_sdk/entity/cbaseplayercontroller.h"

class GameSessionConfiguration_t { };

KHook::Virtual<IServerGameDLL, void, bool, bool, bool> gameFrameHook(&IServerGameDLL::GameFrame, &g_ServerListPlayersFix, nullptr, &ServerListPlayersFix::Hook_GameFrame);
KHook::Virtual<IServerGameDLL, void> gameServerSteamAPIActivatedHook(&IServerGameDLL::GameServerSteamAPIActivated, &g_ServerListPlayersFix, &ServerListPlayersFix::Hook_GameServerSteamAPIActivated, nullptr);
KHook::Virtual<IServerGameDLL, void> gameServerSteamAPIDeactivatedHook(&IServerGameDLL::GameServerSteamAPIDeactivated, &g_ServerListPlayersFix, &ServerListPlayersFix::Hook_GameServerSteamAPIDeactivated, nullptr);

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif


ServerListPlayersFix g_ServerListPlayersFix;
ICvar *icvar = NULL;
CSteamGameServerAPIContext g_steamAPI;
IServerGameDLL* server = NULL;
IVEngineServer* engine = NULL;
IServerGameClients* gameclients = NULL;
CGameEntitySystem* g_pEntitySystem = nullptr;

CGameEntitySystem* GameEntitySystem()
{
#ifdef WIN32
	static int offset = 88;
#else
	static int offset = 80;
#endif
	return *reinterpret_cast<CGameEntitySystem**>((uintptr_t)(g_pGameResourceServiceServer)+offset);
}


PLUGIN_EXPOSE(ServerListPlayersFix, g_ServerListPlayersFix);
bool ServerListPlayersFix::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, server, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_ANY(GetServerFactory, gameclients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );

	gameFrameHook.Add(server);
	gameServerSteamAPIActivatedHook.Add(g_pSource2Server);
	gameServerSteamAPIDeactivatedHook.Add(g_pSource2Server);

	g_pCVar = icvar;
	ConVar_Register( FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL );

	return true;
}

bool ServerListPlayersFix::Unload(char *error, size_t maxlen)
{
	gameFrameHook.Remove(server);
	gameServerSteamAPIActivatedHook.Remove(g_pSource2Server);
	gameServerSteamAPIDeactivatedHook.Remove(g_pSource2Server);

	return true;
}

void ServerListPlayersFix::UpdatePlayers()
{
	auto gpGlobals = engine->GetServerGlobals();
	g_pEntitySystem = GameEntitySystem();

	if(!gpGlobals || !g_pEntitySystem)
		return;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		auto steamId = engine->GetClientSteamID(CPlayerSlot(i));
		if (steamId)
		{
			auto controller = (CBasePlayerController*)g_pEntitySystem->GetEntityInstance(CEntityIndex(i+1));
			if(controller)
				g_steamAPI.SteamGameServer()->BUpdateUserData(*steamId, controller->GetPlayerName(), gameclients->GetPlayerScore(CPlayerSlot(i)));
		}
	}
}

KHook::Return<void> ServerListPlayersFix::Hook_GameFrame(IServerGameDLL* pThis, bool simulating, bool bFirstTick, bool bLastTick)
{
	static double g_flNextUpdate = 0.0;

	double curtime = Plat_FloatTime();
	if (curtime > g_flNextUpdate)
	{
		UpdatePlayers();
		
		g_flNextUpdate = curtime + 5.0;
	}

	return {KHook::Action::Ignore};
}

void ServerListPlayersFix::AllPluginsLoaded()
{
}

KHook::Return<void> ServerListPlayersFix::Hook_GameServerSteamAPIActivated(IServerGameDLL* pThis)
{
	g_steamAPI.Init();

	return {KHook::Action::Ignore};
}

KHook::Return<void> ServerListPlayersFix::Hook_GameServerSteamAPIDeactivated(IServerGameDLL* pThis)
{
	return {KHook::Action::Ignore};
}

void ServerListPlayersFix::OnLevelInit(char const* pMapName,
	char const* pMapEntities,
	char const* pOldLevel,
	char const* pLandmarkName,
	bool loadGame,
	bool background)
{
}

void ServerListPlayersFix::OnLevelShutdown()
{
}

bool ServerListPlayersFix::Pause(char *error, size_t maxlen)
{
	return true;
}

bool ServerListPlayersFix::Unpause(char *error, size_t maxlen)
{
	return true;
}

const char *ServerListPlayersFix::GetLicense()
{
	return "GPLv3";
}

const char *ServerListPlayersFix::GetVersion()
{
	return "2.0";
}

const char *ServerListPlayersFix::GetDate()
{
	return __DATE__;
}

const char *ServerListPlayersFix::GetLogTag()
{
	return "ServerListPlayersFix";
}

const char *ServerListPlayersFix::GetAuthor()
{
	return "Poggu";
}

const char *ServerListPlayersFix::GetDescription()
{
	return "Populates user information in the steam api";
}

const char *ServerListPlayersFix::GetName()
{
	return "ServerListPlayersFix";
}

const char *ServerListPlayersFix::GetURL()
{
	return "https://poggu.me";
}
