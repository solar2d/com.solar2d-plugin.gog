// ----------------------------------------------------------------------------
// 
// PluginConfigLuaSettings.cpp
// Copyright (c) 2016 Corona Labs Inc. All rights reserved.
// This software may be modified and distributed under the terms
// of the MIT license.  See the LICENSE file for details.
//
// ----------------------------------------------------------------------------

#include "PluginConfigLuaSettings.h"
#include "CoronaLua.h"
#include <sstream>
#include <string>


PluginConfigLuaSettings::PluginConfigLuaSettings()
{
}

PluginConfigLuaSettings::~PluginConfigLuaSettings()
{
}

const char* PluginConfigLuaSettings::GetStringClientId() const
{
	return fStringClientId.c_str();
}

void PluginConfigLuaSettings::SetStringClientId(const char* stringId)
{
	if (stringId)
	{
		fStringClientId = stringId;
	}
	else
	{
		fStringClientId.clear();
	}
}

const char* PluginConfigLuaSettings::GetStringClientSecret() const
{
	return fStringClientSecret.c_str();
}

void PluginConfigLuaSettings::SetStringClientSecret(const char* stringId)
{
	if (stringId)
	{
		fStringClientSecret = stringId;
	}
	else
	{
		fStringClientSecret.clear();
	}
}

void PluginConfigLuaSettings::Reset()
{
	fStringClientId.clear();
	fStringClientSecret.clear();
}

bool PluginConfigLuaSettings::LoadFrom(lua_State* luaStatePointer)
{
	bool wasLoaded = false;

	// Validate.
	if (!luaStatePointer)
	{
		return false;
	}

	// Determine if the "config.lua" file was already loaded.
	bool wasConfigLuaAlreadyLoaded = false;
	lua_getglobal(luaStatePointer, "package");
	if (lua_istable(luaStatePointer, -1))
	{
		lua_getfield(luaStatePointer, -1, "loaded");
		if (lua_istable(luaStatePointer, -1))
		{
			lua_getfield(luaStatePointer, -1, "config");
			if (!lua_isnil(luaStatePointer, -1))
			{
				wasConfigLuaAlreadyLoaded = true;
			}
			lua_pop(luaStatePointer, 1);
		}
		lua_pop(luaStatePointer, 1);
	}
	lua_pop(luaStatePointer, 1);

	// Check if an "application" global already exists.
	// If so, then push it to the top of the stack. Otherwise, we push nil to the stack.
	// Note: We need to do this since loading the "config.lua" down below will replace the "application" global.
	//       This allows us to restore this global to its previous state when we're done loading everything.
	lua_getglobal(luaStatePointer, "application");

	// These curly braces indicate the stack count is up by 1 due to the above.
	{
		// Load the "config.lua" file via the Lua require() function.
		lua_getglobal(luaStatePointer, "require");
		if (lua_isfunction(luaStatePointer, -1))
		{
			lua_pushstring(luaStatePointer, "config");
			CoronaLuaDoCall(luaStatePointer, 1, 1);
			lua_pop(luaStatePointer, 1);
		}
		else
		{
			lua_pop(luaStatePointer, 1);
		}

		// Fetch this plugin's settings from the "config.lua" file's application settings.
		lua_getglobal(luaStatePointer, "application");
		if (lua_istable(luaStatePointer, -1))
		{
			lua_getfield(luaStatePointer, -1, "gog");
			if (lua_istable(luaStatePointer, -1))
			{
				// Flag that this plugin's table was found in the "config.lua" file.
				wasLoaded = true;

				// Fetch the GOG client ID in string form.
				lua_getfield(luaStatePointer, -1, "clientId");
				const auto clientIdLuaValueType = lua_type(luaStatePointer, -1);
				if (clientIdLuaValueType == LUA_TSTRING)
				{
					auto stringValue = lua_tostring(luaStatePointer, -1);
					if (stringValue)
					{
						fStringClientId = stringValue;
					}
				}
				lua_pop(luaStatePointer, 1);

				// Fetch the GOG client secret in string form.
				lua_getfield(luaStatePointer, -1, "clientSecret");
				const auto clientSecretLuaValueType = lua_type(luaStatePointer, -1);
				if (clientSecretLuaValueType == LUA_TSTRING)
				{
					auto stringValue = lua_tostring(luaStatePointer, -1);
					if (stringValue)
					{
						fStringClientSecret = stringValue;
					}
				}
				lua_pop(luaStatePointer, 1);

				// *** In the future, other "config.lua" plugin settings can be loaded here. ***
			}
			lua_pop(luaStatePointer, 1);
		}
		lua_pop(luaStatePointer, 1);

		// Unload the "config.lua", but only if it wasn't loaded before.
		if (!wasConfigLuaAlreadyLoaded)
		{
			lua_getglobal(luaStatePointer, "package");
			if (lua_istable(luaStatePointer, -1))
			{
				lua_getfield(luaStatePointer, -1, "loaded");
				if (lua_istable(luaStatePointer, -1))
				{
					lua_pushnil(luaStatePointer);
					lua_setfield(luaStatePointer, -2, "config");
				}
				lua_pop(luaStatePointer, 1);
			}
			lua_pop(luaStatePointer, 1);
		}
	}

	// Restore the "application" global to its previous value which is at the top of the stack.
	// If it wasn't set before then nil we be at the top of stack, which will remove this global.
	lua_setglobal(luaStatePointer, "application");

	// Returns true if this plugin's settings table was found in the "config.lua" file.
	// Note that this does not necessarily mean that any fields were found under this table.
	return wasLoaded;
}
