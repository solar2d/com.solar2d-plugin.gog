// ----------------------------------------------------------------------------
// 
// DispatchEventTask.h
// Copyright (c) 2016 Corona Labs Inc. All rights reserved.
// This software may be modified and distributed under the terms
// of the MIT license.  See the LICENSE file for details.
//
// ----------------------------------------------------------------------------

#pragma once

#include "LuaEventDispatcher.h"
#include <memory>

// Forward declarations.
extern "C"
{
	struct lua_State;
}


/**
  Abstract class used to dispatch an event table to Lua.

  The intended usage is that a derived class' constructor would copy a Steam event's data structure and then
  the event task instance would be queued to a RuntimeContext. A RuntimeContext would then dispatch all queued
  event tasks to Lua via their Execute() methods only if the Corona runtime is currently running (ie: not suspended).
 */
class BaseDispatchEventTask
{
	public:
		BaseDispatchEventTask();
		virtual ~BaseDispatchEventTask();

		std::shared_ptr<LuaEventDispatcher> GetLuaEventDispatcher() const;
		void SetLuaEventDispatcher(const std::shared_ptr<LuaEventDispatcher>& dispatcherPointer);
		virtual const char* GetLuaEventName() const = 0;
		virtual bool PushLuaEventTableTo(lua_State* luaStatePointer) const = 0;
		bool Execute();

	private:
		std::shared_ptr<LuaEventDispatcher> fLuaEventDispatcherPointer;
};

/** Dispatches a Gog "AuthListener" event and its data to Lua. */
class DispatchAuthResponseEventTask : public BaseDispatchEventTask
{
	public:
		static const char kLuaEventName[];

		DispatchAuthResponseEventTask();
		virtual ~DispatchAuthResponseEventTask();

		void AcquireEventDataFrom(bool success);
		virtual const char* GetLuaEventName() const;
		virtual bool PushLuaEventTableTo(lua_State* luaStatePointer) const;

	private:
		bool fSuccess;
};
