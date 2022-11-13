// --------------------------------------------------------------------------------
// 
// GogLuaInterface.cpp
// Copyright (c) 2016 Corona Labs Inc. All rights reserved.
// This software may be modified and distributed under the terms
// of the MIT license.  See the LICENSE file for details.
//
// --------------------------------------------------------------------------------

#include "CoronaLua.h"
#include "CoronaMacros.h"
#include "DispatchEventTask.h"
#include "LuaEventDispatcher.h"
#include "PluginConfigLuaSettings.h"
#include "RuntimeContext.h"
#include <cmath>
#include <sstream>
#include <stdint.h>

#include <string>
#include <thread>

extern "C"
{
#	include "lua.h"
#	include "lauxlib.h"
}

#include "GalaxyApi.h"

#ifdef _WIN32
#include <windows.h>
#include <windef.h>
#include <winbase.h>
#endif

//---------------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------------


//---------------------------------------------------------------------------------
// Private Static Variables
//---------------------------------------------------------------------------------

/**
  Gets the thread ID that all plugin instances are currently running in.
  This member is only applicable if at least 1 plugin instance exists.
  Intended to avoid multiple plugin instances from being loaded at the same time on different threads.
 */
static std::thread::id sMainThreadId;

//---------------------------------------------------------------------------------
// Private Static Functions
//---------------------------------------------------------------------------------
/**
  Determines if the given Lua state is running under the Corona Simulator.
  @param luaStatePointer Pointer to the Lua state to check.
  @return Returns true if the given Lua state is running under the Corona Simulator.

          Returns false if running under a real device/desktop application or if given a null pointer.
 */
bool IsRunningInCoronaSimulator(lua_State* luaStatePointer)
{
	bool isSimulator = false;
	lua_getglobal(luaStatePointer, "system");
	if (lua_istable(luaStatePointer, -1))
	{
		lua_getfield(luaStatePointer, -1, "getInfo");
		if (lua_isfunction(luaStatePointer, -1))
		{
			lua_pushstring(luaStatePointer, "environment");
			int callResultCode = CoronaLuaDoCall(luaStatePointer, 1, 1);
			if (!callResultCode && (lua_type(luaStatePointer, -1) == LUA_TSTRING))
			{
				isSimulator = (strcmp(lua_tostring(luaStatePointer, -1), "simulator") == 0);
			}
		}
		lua_pop(luaStatePointer, 1);
	}
	lua_pop(luaStatePointer, 1);
	return isSimulator;
}

//---------------------------------------------------------------------------------
// Lua API Handlers
//---------------------------------------------------------------------------------
/** gog.getEncryptedAppTicket() */
int OnGetEncryptedAppTicket(lua_State* luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		return 0;
	}

	// Fetch this plugin's runtime context associated with the calling Lua state.
	auto contextPointer = (RuntimeContext*)lua_touserdata(luaStatePointer, lua_upvalueindex(1));
	if (!contextPointer)
	{
		return 0;
	}

	auto user = galaxy::api::User();
	if (!user->SignedIn())
	{
		return 0;
	}

	if (!user->IsLoggedOn())
	{
		return 0;
	}
	
	const uint32_t encryptedAppTicketSizeMax = 1024;
	char encryptedAppTicketData[encryptedAppTicketSizeMax] = { 0 };
	uint32_t encryptedAppTicketSize = 0;
	user->GetEncryptedAppTicket(encryptedAppTicketData, encryptedAppTicketSizeMax, encryptedAppTicketSize);
	
	lua_pushstring(luaStatePointer, std::string(encryptedAppTicketData, encryptedAppTicketSize).c_str());
	return 1;
}

/** gog.getEncryptedAppTicket() */
int OnRequestEncryptedAppTicket(lua_State* luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		return 0;
	}

	// Fetch this plugin's runtime context associated with the calling Lua state.
	auto contextPointer = (RuntimeContext*)lua_touserdata(luaStatePointer, lua_upvalueindex(1));
	if (!contextPointer)
	{
		return 0;
	}

	auto user = galaxy::api::User();
	if (!user->SignedIn())
	{
		return 0;
	}

	if (!user->IsLoggedOn())
	{
		return 0;
	}

	user->RequestEncryptedAppTicket(nullptr, 0);
	return 0;
}

/** bool gog.setAchievementUnlocked(achievementName) */
int OnSetAchievementUnlocked(lua_State* luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		lua_pushboolean(luaStatePointer, 0);
		return 1;
	}

	auto user = galaxy::api::User();
	if (!user->SignedIn())
	{
		lua_pushboolean(luaStatePointer, 0);
		return 1;
	}

	// Fetch the achievement name.
	const char* achievementName = nullptr;
	if (lua_type(luaStatePointer, 1) == LUA_TSTRING)
	{
		achievementName = lua_tostring(luaStatePointer, 1);
	}
	if (!achievementName)
	{
		CoronaLuaError(luaStatePointer, "1st argument must be set to the achievement's unique name.");
		lua_pushboolean(luaStatePointer, 0);
		return 1;
	}

	// Attempt to unlock the given achievement.
	galaxy::api::Stats()->SetAchievement(achievementName);

	auto galaxySetAchievementError = galaxy::api::GetError();
	if (galaxySetAchievementError) {
		CoronaLuaWarning(luaStatePointer, "[GOG ERROR] %s: %s", galaxySetAchievementError->GetName(), galaxySetAchievementError->GetMsg());
	}

	galaxy::api::Stats()->StoreStatsAndAchievements();

	auto galaxyStoreStatsAndAchievementsError = galaxy::api::GetError();
	if (galaxyStoreStatsAndAchievementsError) {
		CoronaLuaWarning(luaStatePointer, "[GOG ERROR] %s: %s", galaxyStoreStatsAndAchievementsError->GetName(), galaxyStoreStatsAndAchievementsError->GetMsg());
	}

	lua_pushboolean(luaStatePointer, 1);
	return 1;
}

/** gog.addEventListener(eventName, listener) */
int OnAddEventListener(lua_State* luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		return 0;
	}

	// Fetch the global GOG event name to listen to.
	const char* eventName = nullptr;
	if (lua_type(luaStatePointer, 1) == LUA_TSTRING)
	{
		eventName = lua_tostring(luaStatePointer, 1);
	}
	if (!eventName || ('\0' == eventName[0]))
	{
		CoronaLuaError(luaStatePointer, "1st argument must be set to an event name.");
		return 0;
	}

	// Determine if the 2nd argument references a Lua listener function/table.
	if (!CoronaLuaIsListener(luaStatePointer, 2, eventName))
	{
		CoronaLuaError(luaStatePointer, "2nd argument must be set to a listener.");
		return 0;
	}

	// Fetch the runtime context associated with the calling Lua state.
	auto contextPointer = (RuntimeContext*)lua_touserdata(luaStatePointer, lua_upvalueindex(1));
	if (!contextPointer)
	{
		return 0;
	}

	// Add the given listener for the global GOG event.
	auto luaEventDispatcherPointer = contextPointer->GetLuaEventDispatcher();
	if (luaEventDispatcherPointer)
	{
		luaEventDispatcherPointer->AddEventListener(luaStatePointer, eventName, 2);
	}

	return 0;
}

/** gog.removeEventListener(eventName, listener) */
int OnRemoveEventListener(lua_State* luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		return 0;
	}

	// Fetch the global GOG event name to stop listening to.
	const char* eventName = nullptr;
	if (lua_type(luaStatePointer, 1) == LUA_TSTRING)
	{
		eventName = lua_tostring(luaStatePointer, 1);
	}
	if (!eventName || ('\0' == eventName[0]))
	{
		CoronaLuaError(luaStatePointer, "1st argument must be set to an event name.");
		return 0;
	}

	// Determine if the 2nd argument references a Lua listener function/table.
	if (!CoronaLuaIsListener(luaStatePointer, 2, eventName))
	{
		CoronaLuaError(luaStatePointer, "2nd argument must be set to a listener.");
		return 0;
	}

	// Fetch the runtime context associated with the calling Lua state.
	auto contextPointer = (RuntimeContext*)lua_touserdata(luaStatePointer, lua_upvalueindex(1));
	if (!contextPointer)
	{
		return 0;
	}

	// Remove the given listener from the global GOG event.
	auto luaEventDispatcherPointer = contextPointer->GetLuaEventDispatcher();
	if (luaEventDispatcherPointer)
	{
		luaEventDispatcherPointer->RemoveEventListener(luaStatePointer, eventName, 2);
	}

	return 0;
}

/** Called when a property field is being read from the plugin's Lua table. */
int OnAccessingField(lua_State* luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		return 0;
	}

	// Fetch the field name being accessed.
	if (lua_type(luaStatePointer, 2) != LUA_TSTRING)
	{
		return 0;
	}
	auto fieldName = lua_tostring(luaStatePointer, 2);
	if (!fieldName)
	{
		return 0;
	}

	// Attempt to fetch the requested field value.
	int resultCount = 0;
	if (!strcmp(fieldName, "isLoggedOn"))
	{
		auto contextPointer = (RuntimeContext*)lua_touserdata(luaStatePointer, lua_upvalueindex(1));
		if (!contextPointer)
		{
			return 0;
		}

		auto user = galaxy::api::User();
		if (!user->SignedIn())
		{
			return 0;
		}

		if (!user->IsLoggedOn())
		{
			return 0;
		}

		lua_pushboolean(luaStatePointer, 1);
		resultCount = 1;
	}
	else
	{
		// Unknown field.
		CoronaLuaError(luaStatePointer, "Accessing unknown field: '%s'", fieldName);
	}

	// Return the number of value pushed to Lua as return values.
	return resultCount;
}

/** Called when a property field is being written to in the plugin's Lua table. */
int OnAssigningField(lua_State* luaStatePointer)
{
	// Writing to fields is not currently supported.
	return 0;
}

/**
  Called when the Lua plugin table is being destroyed.
  Expected to happen when the Lua runtime is being terminated.

  Performs finaly cleanup and terminates connection with the GOG client.
 */
int OnFinalizing(lua_State* luaStatePointer)
{
	// Delete this plugin's runtime context from memory.
	auto contextPointer = (RuntimeContext*)lua_touserdata(luaStatePointer, lua_upvalueindex(1));
	if (contextPointer)
	{
		delete contextPointer;
	}

	// Shutdown our connection with Steam if this is the last plugin instance.
	// This must be done after deleting the RuntimeContext above.
	if (RuntimeContext::GetInstanceCount() <= 0)
	{
    	galaxy::api::Shutdown();
	}

	return 0;
}


//---------------------------------------------------------------------------------
// Public Exports
//---------------------------------------------------------------------------------
static class GOGAuthListener : public galaxy::api::IAuthListener{
public:
	void OnAuthSuccess() {
		galaxy::api::Stats()->RequestUserStatsAndAchievements();
	}
	void OnAuthFailure(FailureReason failureReason) {}
	void OnAuthLost() {}
} g_GOGAuthListener;

/**
  Called when this plugin is being loaded from Lua via a require() function.
  Initializes itself with GOG and returns the plugin's Lua table.
 */
CORONA_EXPORT int luaopen_plugin_gog(lua_State* luaStatePointer)
{
	// Validate.ogg
	if (!luaStatePointer)
	{
		return 0;
	}

	// If this plugin instance is being loaded while another one already exists, then make sure that they're
	// both running on the same thread to avoid race conditions since GOG's event handlers are global.
	// Note: This can only happen if multiple Corona runtimes are running at the same time.
	if (RuntimeContext::GetInstanceCount() > 0)
	{
		if (std::this_thread::get_id() != sMainThreadId)
		{
			luaL_error(luaStatePointer, "Cannot load another instance of 'plugin.gog' from another thread.");
			return 0;
		}
	}
	else
	{
		sMainThreadId = std::this_thread::get_id();
	}

	// Create a new runtime context used to receive GOG's event and dispatch them to Lua.
	// Also used to ensure that the GOG overlay is rendered when requested on Windows.
	auto contextPointer = new RuntimeContext(luaStatePointer);
	if (!contextPointer)
	{
		return 0;
	}

	// Push this plugin's Lua table and all of its functions to the top of the Lua stack.
	// Note: The RuntimeContext pointer is pushed as an upvalue to all of these functions via luaL_openlib().
	{
		const struct luaL_Reg luaFunctions[] =
		{
			{ "getEncryptedAppTicket", OnGetEncryptedAppTicket },
			{ "requestEncryptedAppTicket", OnRequestEncryptedAppTicket },
			{ "setAchievementUnlocked", OnSetAchievementUnlocked },
			{ "addEventListener", OnAddEventListener },
			{ "removeEventListener", OnRemoveEventListener },
			{ nullptr, nullptr }
		};
		lua_createtable(luaStatePointer, 0, 0);
		lua_pushlightuserdata(luaStatePointer, contextPointer);
		luaL_openlib(luaStatePointer, nullptr, luaFunctions, 1);
	}

	// Add a Lua finalizer to the plugin's Lua table and to the Lua registry.
	// Note: Lua 5.1 tables do not support the "__gc" metatable field, but Lua light-userdata types do.
	{
		// Create a Lua metatable used to receive the finalize event.
		const struct luaL_Reg luaFunctions[] =
		{
			{ "__gc", OnFinalizing },
			{ nullptr, nullptr }
		};
		luaL_newmetatable(luaStatePointer, "plugin.gog.__gc");
		lua_pushlightuserdata(luaStatePointer, contextPointer);
		luaL_openlib(luaStatePointer, nullptr, luaFunctions, 1);
		lua_pop(luaStatePointer, 1);

		// Add the finalizer metable to the Lua registry.
		CoronaLuaPushUserdata(luaStatePointer, nullptr, "plugin.gog.__gc");
		int luaReferenceKey = luaL_ref(luaStatePointer, LUA_REGISTRYINDEX);

		// Add the finalizer metatable to the plugin's Lua table as an undocumented "__gc" field.
		// Note that a developer can overwrite this field, which is why we add it to the registry above too.
		lua_rawgeti(luaStatePointer, LUA_REGISTRYINDEX, luaReferenceKey);
		lua_setfield(luaStatePointer, -2, "__gc");
	}

	// Wrap the plugin's Lua table in a metatable used to provide readable/writable property fields.
	{
		const struct luaL_Reg luaFunctions[] =
		{
			{ "__index", OnAccessingField },
			{ "__newindex", OnAssigningField },
			{ nullptr, nullptr }
		};
		luaL_newmetatable(luaStatePointer, "plugin.gog");
		lua_pushlightuserdata(luaStatePointer, contextPointer);
		luaL_openlib(luaStatePointer, nullptr, luaFunctions, 1);
		lua_setmetatable(luaStatePointer, -2);
	}

	// Fetch the GOG properties from the "config.lua" file.
	PluginConfigLuaSettings configLuaSettings;
	configLuaSettings.LoadFrom(luaStatePointer);
	
	// Initialize our connection with GOG if this is the first plugin instance.
	// Note: This avoid initializing twice in case multiple plugin instances exist at the same time.
	if (RuntimeContext::GetInstanceCount() == 1)
	{
    	const char *clientID = configLuaSettings.GetStringClientId();
    	const char *clientSecret = configLuaSettings.GetStringClientSecret();
    	galaxy::api::Init(galaxy::api::InitOptions(clientID, clientSecret));

		auto galaxyInitError = galaxy::api::GetError();
		if (galaxyInitError) {
			CoronaLuaWarning(luaStatePointer, "[GOG ERROR] %s: %s", galaxyInitError->GetName(), galaxyInitError->GetMsg());
		}
	}

	auto user = galaxy::api::User();
	user->SignInGalaxy(false, &g_GOGAuthListener);

	auto galaxySignInError = galaxy::api::GetError();
	if (galaxySignInError) {
		CoronaLuaWarning(luaStatePointer, "[GOG ERROR] %s: %s", galaxySignInError->GetName(), galaxySignInError->GetMsg());
	}

	// We're returning 1 Lua plugin table.
	return 1;
}
