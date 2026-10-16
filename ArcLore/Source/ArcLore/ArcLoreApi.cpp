// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreApi.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "ISourceControlModule.h" // LogSourceControl

// Unique, file-scoped namespace for free helpers and the trampoline state
// (project rule: cpp free functions live in a unique file-scoped namespace).
namespace ArcLoreApiInternal
{
	// File-scoped lifecycle statics.
	static void* GLoreDllHandle = nullptr;
	static bool GbInitialized = false;

	// Per-call dispatch context handed to the C trampoline through the
	// user_context field. The trampoline reinterpret_casts user_context back to
	// a pointer to this struct. It lives on RunLoreOp's stack and is kept alive
	// past the callback stream by the FEvent fence.
	struct FLoreDispatchContext
	{
		// Caller-supplied domain-event handler (runs on the lore worker thread).
		const ArcLoreApi::FLoreEventCollector* Collector = nullptr;
		// Completion status copied from LORE_EVENT_COMPLETE. Defaults to fail.
		int32 CompleteStatus = 1;
		// Error message copied from LORE_EVENT_ERROR (if any).
		FString ErrorMessage;
		// Triggered by the trampoline on LORE_EVENT_END. RunLoreOp waits on it.
		FEvent* DoneEvent = nullptr;
		// [diag] total events seen by the trampoline, and whether the terminal
		// LORE_EVENT_END ever arrived. Used to diagnose the "spinner forever" hang
		// (a missing END means the fence below never releases).
		int32 EventCount = 0;
		bool bEndSeen = false;
		// [diag] human-readable label of the op, set by RunLoreOp before invoking.
		const TCHAR* OpLabel = TEXT("?");
	};

	// C trampoline. MUST be a free, non-capturing function: a lore callback is a
	// plain C function pointer and cannot carry captures. Runs on a LORE-OWNED
	// worker thread. Everything reachable from `Event` is valid only until this
	// function returns, so any retained string is copied immediately.
	static void LoreEventTrampoline(const lore_event_t* Event, uint64_t UserContext)
	{
		if (Event == nullptr || UserContext == 0)
		{
			return;
		}

		FLoreDispatchContext* Context = reinterpret_cast<FLoreDispatchContext*>(static_cast<UPTRINT>(UserContext));

		const uint32_t Tag = lore_event_type(Event);
		++Context->EventCount;
		// [diag] trace every event the lib delivers for this op. Verbose so it can be
		// silenced once the path is proven; the ERROR/END lines below stay at Log.
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [%ls] event #%d tag=%u", Context->OpLabel, Context->EventCount, Tag);
		switch (Tag)
		{
		case LORE_EVENT_ERROR:
			// Copy the error message out of lib-owned memory immediately.
			Context->ErrorMessage = ArcLoreApi::LoreStringToFString(Event->error.error_inner);
			UE_LOGF(LogSourceControl, Warning,"ArcLore: [%ls] LORE_EVENT_ERROR: %ls", Context->OpLabel, *Context->ErrorMessage);
			break;

		case LORE_EVENT_COMPLETE:
			Context->CompleteStatus = Event->complete.status;
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: [%ls] LORE_EVENT_COMPLETE status=%d", Context->OpLabel, Context->CompleteStatus);
			break;

		case LORE_EVENT_END:
			// Terminal event: release the fence. After this the dispatch context
			// (and the caller's Collector) may go out of scope.
			Context->bEndSeen = true;
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: [%ls] LORE_EVENT_END (events=%d) — releasing fence", Context->OpLabel, Context->EventCount);
			if (Context->DoneEvent != nullptr)
			{
				Context->DoneEvent->Trigger();
			}
			break;

		default:
			// Domain event: forward to the collector, which copies any strings
			// during this callback invocation.
			if (Context->Collector != nullptr)
			{
				(*Context->Collector)(*Event);
			}
			break;
		}
	}
}

namespace ArcLoreApi
{
	// ---- String marshaling ---------------------------------------------------

	FLoreUtf8String::FLoreUtf8String(const FString& InValue)
	{
		FTCHARToUTF8 Converter(*InValue);
		const int32 NumBytes = Converter.Length();
		Buffer.SetNumUninitialized(NumBytes + 1);
		if (NumBytes > 0)
		{
			FMemory::Memcpy(Buffer.GetData(), Converter.Get(), NumBytes);
		}
		// Keep the buffer NUL-terminated for safety; the lore_string_t length
		// field excludes this terminator.
		Buffer[NumBytes] = 0;
	}

	lore_string_t FLoreUtf8String::View() const
	{
		lore_string_t Result;
		// Buffer always holds at least the trailing NUL, so GetData() is valid.
		// An empty value yields a non-null pointer with length 0, which the lib
		// treats as the empty string.
		Result.string = reinterpret_cast<const char*>(Buffer.GetData());
		Result.length = static_cast<uintptr_t>(Buffer.Num() - 1);
		return Result;
	}

	lore_string_t EmptyLoreString()
	{
		lore_string_t Result;
		Result.string = "";
		Result.length = 0;
		return Result;
	}

	FString LoreStringToFString(const lore_string_t& String)
	{
		if (String.string == nullptr || String.length == 0)
		{
			return FString();
		}
		const int32 NumBytes = static_cast<int32>(String.length);
		const FUTF8ToTCHAR Converter(String.string, NumBytes);
		return FString(Converter.Length(), Converter.Get());
	}

	FString DescribeGlobalArgs(const lore_global_args_t& Args)
	{
		return FString::Printf(
			TEXT("repo='%s' identity='%s' offline=%u local=%u remote=%u force=%u cache=%u correlation='%s'"),
			*LoreStringToFString(Args.repository_path),
			*LoreStringToFString(Args.identity),
			Args.offline, Args.local, Args.remote, Args.force, Args.cache,
			*LoreStringToFString(Args.correlation_id));
	}

	// ---- Global args ---------------------------------------------------------

	FLoreGlobalArgsScope::FLoreGlobalArgsScope(const FString& RepoRoot, const FString& Identity)
		: RepositoryPath(RepoRoot)
		, Identity(Identity)
	{
		FMemory::Memzero(GlobalArgs);
		GlobalArgs.repository_path = RepositoryPath.View();
		GlobalArgs.identity = this->Identity.View();
		GlobalArgs.working_directory = EmptyLoreString();
		GlobalArgs.correlation_id = EmptyLoreString();
		GlobalArgs.identity_token = EmptyLoreString();
		GlobalArgs.access_token = EmptyLoreString();
		GlobalArgs.no_gc = 1;
	}

	// ---- Lifecycle -----------------------------------------------------------

	bool Init()
	{
		if (ArcLoreApiInternal::GbInitialized && ArcLoreApiInternal::GLoreDllHandle != nullptr)
		{
			return true;
		}

#if PLATFORM_WINDOWS
		// lore.dll is staged next to the editor executable via the Lore module's
		// RuntimeDependency. Resolve it relative to the binary directory so the
		// delay-load stubs bind against this handle.
		const FString DllName = TEXT("lore.dll");
		const FString DllPath = FPaths::Combine(FPlatformProcess::BaseDir(), DllName);
		ArcLoreApiInternal::GLoreDllHandle = FPlatformProcess::GetDllHandle(*DllPath);
		if (ArcLoreApiInternal::GLoreDllHandle == nullptr)
		{
			// Fall back to letting the loader search (PATH / default dirs).
			ArcLoreApiInternal::GLoreDllHandle = FPlatformProcess::GetDllHandle(*DllName);
		}
#else
		ArcLoreApiInternal::GLoreDllHandle = nullptr;
#endif

		if (ArcLoreApiInternal::GLoreDllHandle == nullptr)
		{
			return false;
		}

		// Thread limit MUST be set before the first op; 0 = library default.
		lore_set_thread_limit(static_cast<uintptr_t>(0));

		// Minimal log config: zero-initialize the struct (logging to file
		// disabled by default per the header), then pick a sane minimum level.
		// The struct is trivial, so configuring it here is cheap and explicit.
		lore_log_config_t LoreLogConfig;
		FMemory::Memzero(LoreLogConfig);
		LoreLogConfig.file_path = EmptyLoreString();
		LoreLogConfig.file_prefix = EmptyLoreString();
		LoreLogConfig.level = LORE_LOG_LEVEL_WARN;
		lore_log_configure(&LoreLogConfig);

		ArcLoreApiInternal::GbInitialized = true;
		return true;
	}

	bool IsReady()
	{
		return ArcLoreApiInternal::GLoreDllHandle != nullptr && ArcLoreApiInternal::GbInitialized;
	}

	void Shutdown()
	{
		if (!ArcLoreApiInternal::GbInitialized && ArcLoreApiInternal::GLoreDllHandle == nullptr)
		{
			// Guard against double-shutdown.
			return;
		}

		if (ArcLoreApiInternal::GbInitialized)
		{
			lore_shutdown();
		}

		if (ArcLoreApiInternal::GLoreDllHandle != nullptr)
		{
			FPlatformProcess::FreeDllHandle(ArcLoreApiInternal::GLoreDllHandle);
			ArcLoreApiInternal::GLoreDllHandle = nullptr;
		}

		ArcLoreApiInternal::GbInitialized = false;
	}

	// ---- Op execution (the fence) --------------------------------------------

	int32 RunLoreOp(const FLoreOpInvoker& Invoker, const FLoreEventCollector& Collector, FString& OutError, const TCHAR* OpLabel)
	{
		OutError.Reset();

		// Never resolve a delay-load import against an unloaded dll.
		if (!IsReady())
		{
			OutError = TEXT("Lore library is not initialized.");
			UE_LOGF(LogSourceControl, Warning,"ArcLore: [%ls] RunLoreOp aborted — library not initialized.", OpLabel);
			return 1;
		}

		FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
		// Return the event to the pool on every exit path.
		ON_SCOPE_EXIT
		{
			FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
		};

		ArcLoreApiInternal::FLoreDispatchContext Context;
		Context.Collector = &Collector;
		Context.CompleteStatus = 1; // default to fail until COMPLETE arrives
		Context.DoneEvent = DoneEvent;
		Context.OpLabel = OpLabel;

		lore_event_callback_config_t CallbackConfig;
		CallbackConfig.func = &ArcLoreApiInternal::LoreEventTrampoline;
		CallbackConfig.user_context = static_cast<uint64_t>(reinterpret_cast<UPTRINT>(&Context));

		// Run the typed sync op. The thunk supplies the concrete arg-struct types.
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [%ls] invoking lore op...", OpLabel);
		const double InvokeStart = FPlatformTime::Seconds();
		const int32 OpResult = Invoker(CallbackConfig);
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [%ls] lore op returned %d (events so far=%d); waiting for LORE_EVENT_END...", OpLabel, OpResult, Context.EventCount);

		// FENCE: block until the callback stream is fully drained. Do NOT assume
		// lore_x() has drained the callback by the time it returns. Only after
		// this Wait() returns may Context (and Collector) go out of scope, so no
		// lore-owned callback can outlive this stack frame.
		//
		// [diag] Poll the fence in bounded slices so a missing LORE_EVENT_END (the
		// suspected cause of the editor's perpetual "waiting" spinner) shows up as a
		// periodic warning in the log instead of a silent deadlock. We never break
		// out of the wait: abandoning the fence while a lore-owned thread might still
		// fire the trampoline would let Context die under it (use-after-free). If
		// these warnings repeat forever, the lib is not delivering END for this op.
		const uint32 WaitSliceMs = 2000;
		double NextReportSeconds = 4.0;
		while (!DoneEvent->Wait(WaitSliceMs))
		{
			const double Elapsed = FPlatformTime::Seconds() - InvokeStart;
			if (Elapsed >= NextReportSeconds)
			{
				UE_LOGF(LogSourceControl, Warning,"ArcLore: [%ls] STILL waiting for LORE_EVENT_END after %.0fs (OpResult=%d, CompleteStatus=%d, events=%d, endSeen=%d). The op may never deliver a terminal event — this is what hangs the source-control menu.",
					OpLabel, Elapsed, OpResult, Context.CompleteStatus, Context.EventCount, Context.bEndSeen ? 1 : 0);
				NextReportSeconds = Elapsed + 5.0;
			}
		}

		const double TotalSeconds = FPlatformTime::Seconds() - InvokeStart;
		OutError = Context.ErrorMessage;
		const int32 FinalResult = (OpResult != 0 || Context.CompleteStatus != 0)
			? (OpResult != 0 ? OpResult : Context.CompleteStatus)
			: 0;
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: [%ls] done in %.2fs — result=%d (OpResult=%d, CompleteStatus=%d, events=%d)%ls%ls",
			OpLabel, TotalSeconds, FinalResult, OpResult, Context.CompleteStatus, Context.EventCount,
			OutError.IsEmpty() ? TEXT("") : TEXT(" error="), OutError.IsEmpty() ? TEXT("") : *OutError);
		return FinalResult;
	}
}
