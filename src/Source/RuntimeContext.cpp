// ----------------------------------------------------------------------------
// 
// RuntimeContext.cpp
// Copyright (c) 2016 Corona Labs Inc. All rights reserved.
// This software may be modified and distributed under the terms
// of the MIT license.  See the LICENSE file for details.
//
// ----------------------------------------------------------------------------

#include "RuntimeContext.h"
#include "CoronaLua.h"
#include "DispatchEventTask.h"
#include <exception>
#include <memory>
#include <unordered_set>

extern "C"
{
#	include "lua.h"
}

/** Stores a collection of all RuntimeContext instances that currently exist in the application. */
static std::unordered_set<RuntimeContext*> sRuntimeContextCollection;

RuntimeContext::RuntimeContext(lua_State* luaStatePointer)
:	fLuaEnterFrameCallback(this, &RuntimeContext::OnCoronaEnterFrame, luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		throw std::exception();
	}

	// If the given Lua state belongs to a coroutine, then use the main Lua state instead.
	{
		auto mainLuaStatePointer = CoronaLuaGetCoronaThread(luaStatePointer);
		if (mainLuaStatePointer && (mainLuaStatePointer != luaStatePointer))
		{
			luaStatePointer = mainLuaStatePointer;
		}
	}

	// Create a Lua EventDispatcher object.
	// Used to dispatch global events to listeners
	fLuaEventDispatcherPointer = std::make_shared<LuaEventDispatcher>(luaStatePointer);

	// Add Corona runtime event listeners.
	fLuaEnterFrameCallback.AddToRuntimeEventListeners("enterFrame");

	// Add this class instance to the global collection.
	sRuntimeContextCollection.insert(this);
}

RuntimeContext::~RuntimeContext()
{
	// Remove our Corona runtime event listeners.
	fLuaEnterFrameCallback.RemoveFromRuntimeEventListeners("enterFrame");

	// Remove this class instance from the global collection.
	sRuntimeContextCollection.erase(this);
}

lua_State* RuntimeContext::GetMainLuaState() const
{
	if (fLuaEventDispatcherPointer)
	{
		return fLuaEventDispatcherPointer->GetLuaState();
	}
	return nullptr;
}

std::shared_ptr<LuaEventDispatcher> RuntimeContext::GetLuaEventDispatcher() const
{
	return fLuaEventDispatcherPointer;
}

RuntimeContext* RuntimeContext::GetInstanceBy(lua_State* luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		return nullptr;
	}

	// If the given Lua state belongs to a coroutine, then use the main Lua state instead.
	{
		auto mainLuaStatePointer = CoronaLuaGetCoronaThread(luaStatePointer);
		if (mainLuaStatePointer && (mainLuaStatePointer != luaStatePointer))
		{
			luaStatePointer = mainLuaStatePointer;
		}
	}

	// Return the first runtime context instance belonging to the given Lua state.
	for (auto&& runtimePointer : sRuntimeContextCollection)
	{
		if (runtimePointer && runtimePointer->GetMainLuaState() == luaStatePointer)
		{
			return runtimePointer;
		}
	}
	return nullptr;
}

int RuntimeContext::GetInstanceCount()
{
	return (int)sRuntimeContextCollection.size();
}

int RuntimeContext::OnCoronaEnterFrame(lua_State* luaStatePointer)
{
	// Validate.
	if (!luaStatePointer)
	{
		return 0;
	}

    galaxy::api::ProcessData();

	// Dispatch all queued events received from the above ProcessData() call to Lua.
	while (fDispatchEventTaskQueue.size() > 0)
	{
		auto dispatchEventTaskPointer = fDispatchEventTaskQueue.front();
		fDispatchEventTaskQueue.pop();
		if (dispatchEventTaskPointer)
		{
			dispatchEventTaskPointer->Execute();
		}
	}

	return 0;
}

template<class TGogResultType, class TDispatchEventTask>
void RuntimeContext::OnHandleGlobalGogEvent(TGogResultType* eventDataPointer)
{
	// Triggers a compiler error if template type "TDispatchEventTask" does not derive from "BaseDispatchEventTask".
	static_assert(
			std::is_base_of<BaseDispatchEventTask, TDispatchEventTask>::value,
			"OnReceivedGlobalGogEvent<TGogResultType, TDispatchEventTask>() method's 'TDispatchEventTask' type"
			" must be set to a class type derived from the 'BaseDispatchEventTask' class.");

	// Validate.
	if (!eventDataPointer)
	{
		return;
	}

	// Create and configure the event dispatcher task.
	auto taskPointer = new TDispatchEventTask();
	if (!taskPointer)
	{
		return;
	}
	taskPointer->SetLuaEventDispatcher(fLuaEventDispatcherPointer);
	taskPointer->AcquireEventDataFrom(*eventDataPointer);

	// Special handling of particular GOG events goes here if we had any.

	// Queue the received Steam event data to be dispatched to Lua later.
	// This ensures that Lua events are only dispatched while Corona is running (ie: not suspended).
	fDispatchEventTaskQueue.push(std::shared_ptr<BaseDispatchEventTask>(taskPointer));
}

 void RuntimeContext::OnAuthResponse(bool success)
 {
 	OnHandleGlobalGogEvent<bool, DispatchAuthResponseEventTask>(&success);
 }
