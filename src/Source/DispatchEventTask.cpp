// --------------------------------------------------------------------------------
// 
// DispatchEventTask.cpp
// Copyright (c) 2016 Corona Labs Inc. All rights reserved.
// This software may be modified and distributed under the terms
// of the MIT license.  See the LICENSE file for details.
//
// --------------------------------------------------------------------------------

#include "DispatchEventTask.h"
#include "CoronaLua.h"

//---------------------------------------------------------------------------------
// BaseDispatchEventTask Class Members
//---------------------------------------------------------------------------------

BaseDispatchEventTask::BaseDispatchEventTask()
{
}

BaseDispatchEventTask::~BaseDispatchEventTask()
{
}

std::shared_ptr<LuaEventDispatcher> BaseDispatchEventTask::GetLuaEventDispatcher() const
{
	return fLuaEventDispatcherPointer;
}

void BaseDispatchEventTask::SetLuaEventDispatcher(const std::shared_ptr<LuaEventDispatcher>& dispatcherPointer)
{
	fLuaEventDispatcherPointer = dispatcherPointer;
}

bool BaseDispatchEventTask::Execute()
{
	// Do not continue if not assigned a Lua event dispatcher.
	if (!fLuaEventDispatcherPointer)
	{
		return false;
	}

	// Fetch the Lua state the event dispatcher belongs to.
	auto luaStatePointer = fLuaEventDispatcherPointer->GetLuaState();
	if (!luaStatePointer)
	{
		return false;
	}

	// Push the derived class' event table to the top of the Lua stack.
	bool wasPushed = PushLuaEventTableTo(luaStatePointer);
	if (!wasPushed)
	{
		return false;
	}

	// Dispatch the event to all subscribed Lua listeners.
	bool wasDispatched = fLuaEventDispatcherPointer->DispatchEventWithoutResult(luaStatePointer, -1);

	// Pop the event table pushed above from the Lua stack.
	// Note: The DispatchEventWithoutResult() method above does not pop off this table.
	lua_pop(luaStatePointer, 1);

	// Return true if the event was successfully dispatched to Lua.
	return wasDispatched;
}

//---------------------------------------------------------------------------------
// DispatchAuthResponseEventTask Class Members
//---------------------------------------------------------------------------------

const char DispatchAuthResponseEventTask::kLuaEventName[] = "authResponse";

DispatchAuthResponseEventTask::DispatchAuthResponseEventTask()
: fSuccess(false)
{
}

DispatchAuthResponseEventTask::~DispatchAuthResponseEventTask()
{
}

void DispatchAuthResponseEventTask::AcquireEventDataFrom(bool success)
{
	fSuccess = success;
}

const char* DispatchAuthResponseEventTask::GetLuaEventName() const
{
	return kLuaEventName;
}

bool DispatchAuthResponseEventTask::PushLuaEventTableTo(lua_State* luaStatePointer) const
{
	// Validate.
	if (!luaStatePointer)
	{
		return false;
	}

	// Push the event data to Lua.
	CoronaLuaNewEvent(luaStatePointer, kLuaEventName);

	lua_pushboolean(luaStatePointer, fSuccess ? 1 : 0);
	lua_setfield(luaStatePointer, -2, "isError");
	return true;
}
