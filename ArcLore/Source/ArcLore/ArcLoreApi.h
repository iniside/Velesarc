// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"

// lore.h declares a plain C ABI but carries no extern "C" guard of its own;
// wrap it so symbols are not C++-mangled (the import lib exports C names).
extern "C"
{
#include "lore.h"
}

// FFI marshaling + lifecycle helper for the Lore C API.
//
// The Lore C API is a global singleton (no session handle). Every operation
// has a synchronous form:
//
//     int32_t lore_x(const lore_global_args_t*, const lore_x_args_t*,
//                    lore_event_callback_config_t);
//
// Results stream through the callback, which runs on a LORE-OWNED worker
// thread, NOT the calling thread. Event strings (lore_string_t) are valid only
// for the duration of a single callback invocation and must be copied
// immediately. The per-op event order is:
//     domain events... -> LORE_EVENT_COMPLETE{status} -> LORE_EVENT_END.
//
// lore.dll is delay-loaded (see Lore.Build.cs PublicDelayLoadDLLs). Calling any
// lore_* import before the dll is loaded resolves the delay-load stub against
// an unloaded module and crashes. ArcLoreApi::Init() loads the dll up front;
// every entry point that touches a lore_* import first checks IsReady().
namespace ArcLoreApi
{
	// ---- Lifecycle -----------------------------------------------------------

	// Loads lore.dll (delay-load target), sets the worker thread limit to the
	// library default, and configures minimal logging. Idempotent: a second
	// call while already initialized is a no-op that returns true. Returns true
	// only when the dll handle is non-null and initialization succeeded.
	bool Init();

	// True when the dll handle is loaded AND Init() succeeded. Every operation
	// MUST gate on this before calling any lore_* import.
	bool IsReady();

	// Calls lore_shutdown(), frees the dll handle, and clears the file-scoped
	// statics. Guards against double-shutdown.
	void Shutdown();

	// ---- String marshaling ---------------------------------------------------

	// Owns a UTF-8 byte buffer and exposes a lore_string_t that points into it.
	// The lore library copies argument strings before the lore_x() call returns,
	// so a builder of this kind only needs to outlive the single op call. Keep
	// the FLoreUtf8String alive across the lore_x() invocation that consumes its
	// view; once the call returns the caller may let it go out of scope.
	struct FLoreUtf8String
	{
		explicit FLoreUtf8String(const FString& InValue);

		// View into the owned buffer. Valid only while this object is alive.
		lore_string_t View() const;

	private:
		// NUL-terminated UTF-8 bytes (length excludes the trailing NUL).
		TArray<uint8> Buffer;
	};

	// Copies a lore_string_t (UTF-8, lib-owned, callback-lifetime) into an
	// FString. Safe to call from inside the callback; it copies immediately.
	FString LoreStringToFString(const lore_string_t& String);

	// Project policy: argument strings are never NULL, although lore.h also accepts NULL with length 0 as empty.
	lore_string_t EmptyLoreString();

	// [diag] One-line summary of a lore_global_args_t (paths, identity, and the
	// connectivity flags — offline/local/remote) for logging. Used to tell a
	// genuine offline op from a misrouted/unauthenticated one.
	FString DescribeGlobalArgs(const lore_global_args_t& Args);

	// ---- Global args ---------------------------------------------------------

	// Owns the UTF-8 backing buffers for a lore_global_args_t and exposes the
	// populated struct. lore_global_args_t holds lore_string_t views that point
	// into this object's buffers, so the scope MUST outlive the lore_x() call
	// that consumes Args(). Non-string fields other than no_gc are zero-initialized.
	struct FLoreGlobalArgsScope
	{
		FLoreGlobalArgsScope(const FString& RepoRoot, const FString& Identity);

		const lore_global_args_t& Args() const
		{
			return GlobalArgs;
		}

	private:
		FLoreUtf8String RepositoryPath;
		FLoreUtf8String Identity;
		lore_global_args_t GlobalArgs;
	};

	// ---- Op execution (the fence) --------------------------------------------

	// Domain-event handler. Invoked ON THE LORE WORKER THREAD for every event
	// that is NOT one of the terminal control events (ERROR / COMPLETE / END).
	// It must only copy data into caller-owned memory (via LoreStringToFString);
	// it must NOT touch UObjects or game-thread state.
	// Owning TFunction: call sites store lambdas in named locals, which would dangle as TFunctionRef.
	using FLoreEventCollector = TFunction<void(const lore_event_t& Event)>;

	// Invokes the typed sync op. The thunk receives the fully built callback
	// config and must call the concrete lore_x(&Globals, &Args, Config),
	// returning its int32 result. This keeps RunLoreOp non-template while
	// staying type-safe across the per-op arg-struct types (a template would
	// have to be parameterized on the arg-struct type; the thunk closes over the
	// typed pointers at the call site instead, avoiding `auto` and template
	// instantiation per op).
	using FLoreOpInvoker = TFunction<int32(const lore_event_callback_config_t& Config)>;

	// Runs a synchronous Lore op and blocks until the callback stream has fully
	// drained (LORE_EVENT_END). The fence guarantees the dispatch context (and
	// therefore the caller's Collector) outlives the lore-owned callback thread:
	// we wait on an FEvent that the C trampoline triggers on LORE_EVENT_END, so
	// no callback can run after RunLoreOp returns.
	//
	// Returns 0 on success (both lore_x() returned 0 AND COMPLETE status == 0),
	// non-zero otherwise. OutError receives the LORE_EVENT_ERROR message if one
	// was emitted. If !IsReady(), returns failure immediately WITHOUT calling any
	// lore_* import.
	// OpLabel is a diagnostic tag (e.g. TEXT("Connect/status")) used only for log
	// lines so a hang can be attributed to a specific op; it has no functional role.
	int32 RunLoreOp(const FLoreOpInvoker& Invoker, const FLoreEventCollector& Collector, FString& OutError, const TCHAR* OpLabel = TEXT("op"));
}
