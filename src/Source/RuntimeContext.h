// ----------------------------------------------------------------------------
// 
// RuntimeContext.h
// Copyright (c) 2016 Corona Labs Inc. All rights reserved.
// This software may be modified and distributed under the terms
// of the MIT license.  See the LICENSE file for details.
//
// ----------------------------------------------------------------------------

#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>

#include "DispatchEventTask.h"
#include "LuaEventDispatcher.h"
#include "LuaMethodCallback.h"
#include "GalaxyApi.h"

// Forward declarations.
extern "C"
{
	struct lua_State;
}


/**
  Manages the plugin's event handling and current state between 1 Corona runtime and Steam.

  Automatically polls for and dispatches global Steam events, such as "LoginResponse_t", to Lua.
  Provides easy handling of Steam's CCallResult async operation via this class' AddEventHandlerFor() method.
  Also ensures that Steam events are only dispatched to Lua while the Corona runtime is running (ie: not suspended).
 */
class RuntimeContext
{
	public:

		/**
		  Struct to be passed to a RuntimeContext's AddEventHandlerFor() method.
		  Sets up a Steam CCallResult async listener and then passes the received steam data to Lua as an
		  event to the given Lua function.
		 */
		struct EventHandlerSettings
		{
			/** Lua state that the "LuaFunctionStackIndex" field indexes. */
			lua_State* LuaStatePointer;

			/** Index to the Lua function that will receive a Lua event providing Steam's CCallResult data. */
			int LuaFunctionStackIndex;

		};

		/**
		  Creates a new Corona runtime context bound to the given Lua state.
		  Sets up a private Lua event dispatcher and listens for Lua runtime events such as "enterFrame".
		  @param luaStatePointer Pointer to a Lua state to bind this context to.

		                         Cannot be null or else an exception will be thrown.
		 */
		RuntimeContext(lua_State* luaStatePointer);

		/** Removes Lua listeners and frees allocated memory. */
		virtual ~RuntimeContext();


		/**
		  Gets a pointer to the Lua state that this Corona runtime context belongs to.
		  This will never return a Lua state belonging to a coroutine.
		  @return Returns a pointer to the Lua state that this Corona runtime context belongs to.
		 */
		lua_State* GetMainLuaState() const;

		/**
		  Gets the main event dispatcher that the plugin's Lua addEventListener() and removeEventListener() functions
		  are expected to be bound to. This runtime context will automatically dispatch global Steam events to
		  Lua listeners added to this dispatcher.
		  @return Returns a pointer to plugin's main event dispatcher for global Steam events.
		 */
		std::shared_ptr<LuaEventDispatcher> GetLuaEventDispatcher() const;

		/**
		  Fetches an active RuntimeContext instance that belongs to the given Lua state.
		  @param luaStatePointer Lua state that was passed to a RuntimeContext instance's constructor.
		  @return Returns a pointer to a RuntimeContext that belongs to the given Lua state.

		          Returns null if there is no RuntimeContext belonging to the given Lua state, or if there
		          was one, then it was already delete.
		 */
		static RuntimeContext* GetInstanceBy(lua_State* luaStatePointer);

		/**
		  Fetches the number of RuntimeContext instances currently active in the application.
		  @return Returns the number of instances current active. Returns zero if all instances have been destroyed.
		 */
		static int GetInstanceCount();

		/** Set up global GOG event handlers via their macros. */
		void OnAuthResponse(bool success);

	private:
		/** Copy constructor deleted to prevent it from being called. */
		RuntimeContext(const RuntimeContext&) = delete;

		/** Method deleted to prevent the copy operator from being used. */
		void operator=(const RuntimeContext&) = delete;

		/**
		  Called when a Lua "enterFrame" event has been dispatched.
		  @param luaStatePointer Pointer to the Lua state that dispatched the event.
		  @return Returns the number of return values pushed to Lua. Returns 0 if no return values were pushed.
		 */
		int OnCoronaEnterFrame(lua_State* luatStatePointer);

		template<class TGogResultType, class TDispatchEventTask>
		/**
		  To be called by this class' global GOG event handler methods.
		  Pushes the given GOG event data to the queue to be dispatched to Lua later once this runtime
		  context verifies that Corona is currently running (ie: not suspended).

		  This is a templatized method.
		  The 1st template type must be set to the Steam event struct type, such as "LoginResponse_t".
		  The 2nd template type must be set to a "BaseDispatchEventTask" derived class,
		  such as the "DispatchAuthResponseEventTask" class.
		  @param eventDataPointer Pointer to the Steam event data received. Can be null.
		 */
		void OnHandleGlobalGogEvent(TGogResultType* eventDataPointer);

		/**
		  The main event dispatcher that the plugin's Lua addEventListener() and removeEventListener() functions
		  are bound to. Used to dispatch global steam events such as "LoginResponse_t".
		 */
		std::shared_ptr<LuaEventDispatcher> fLuaEventDispatcherPointer;

		/** Lua "enterFrame" listener. */
		LuaMethodCallback<RuntimeContext> fLuaEnterFrameCallback;

		/**
		  Queue of task objects used to dispatch various Steam related events to Lua.
		  Native Steam event callbacks are expected to push their event data to this queue to be dispatched
		  by this context later and only while the Corona runtime is running (ie: not suspended).
		 */
		std::queue<std::shared_ptr<BaseDispatchEventTask>> fDispatchEventTaskQueue;
};