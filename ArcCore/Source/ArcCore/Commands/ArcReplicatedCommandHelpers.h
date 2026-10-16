/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once
#include "ArcReplicatedCommand.h"
#include "Commands/ArcReplicatedCommandHost.h"

class UObject;

/**
 * Client-side helpers for dispatching FArcReplicatedCommand subclasses to the server.
 *
 * Preferred path for game code:
 *   Arcx::SendServerCommand<MyCommand>(WorldContextObject, ctorArg0, ctorArg1, ...);
 * The command struct must expose a constructor matching the forwarded args (see
 * FArcMassEquipFromStoreCommand, FArcInteractCommand). Dispatch runs CanSendCommand()
 * before the RPC is sent; the bool return reflects whether the command was accepted.
 *
 * Raw-pointer overloads (SendServerCommand(Context, new MyCommand(...))) exist for
 * Blueprint exec dispatch (UArcStaticsBFL::SendCommandToServer) and tests that build
 * a command out-of-band. Do not use them in normal C++ call sites when a typed template
 * dispatch is available.
 *
 * Context is used only to resolve UWorld; the local player controller is looked up via
 * GEngine->GetFirstLocalPlayerController(World). Pass any valid world context (PC, pawn,
 * widget, actor). SendServerCommandConfirm mirrors the above but registers a delegate
 * for the server's success/failure response.
 */
namespace Arcx
{
	/** Preferred C++ dispatch — constructs CommandType from ctor args and sends to server. */
	template <typename CommandType, typename... Args>
	inline bool SendServerCommand(UObject* Context, Args... args)
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(Context);
		if (World == nullptr)
		{
			return false;
		}

		if (IArcReplicatedCommandHost* ArcPC = Cast<IArcReplicatedCommandHost>(GEngine->GetFirstLocalPlayerController(World)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
				MakeShareable<CommandType>(new CommandType {args...})
				, FGuid::NewGuid()
			);

			return ArcPC->SendReplicatedCommand(CommandHandle);
		}

		return false;
	}

	/** Escape hatch for pre-built commands (Blueprint SendCommandToServer, tests). Prefer the template overload in C++. */
	inline bool SendServerCommand(UObject* Context, FArcReplicatedCommand*&& CommandPtr)
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(Context);
		if (World == nullptr)
		{
			return false;
		}

		if (IArcReplicatedCommandHost* ArcPC = Cast<IArcReplicatedCommandHost>(GEngine->GetFirstLocalPlayerController(World)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
				MakeShareable<FArcReplicatedCommand>(CommandPtr)
				, FGuid::NewGuid()
			);

			return ArcPC->SendReplicatedCommand(CommandHandle);
		}

		return false;
	}

	/** Same as SendServerCommand(Context, ...) but takes UWorld* directly (debuggers, tests). */
	template <typename CommandType, typename... Args>
	inline bool SendServerCommand(UWorld* InWorld, Args... args)
	{
		if (InWorld == nullptr)
		{
			return false;
		}

		if (IArcReplicatedCommandHost* ArcPC = Cast<IArcReplicatedCommandHost>(GEngine->GetFirstLocalPlayerController(InWorld)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
				MakeShareable<CommandType>(new CommandType {args...})
				, FGuid::NewGuid()
			);

			return ArcPC->SendReplicatedCommand(CommandHandle);
		}

		return false;
	}

	/** Typed dispatch with a confirmation delegate called with the server's result. */
	template <typename CommandType, typename... Args>
	inline bool SendServerCommandConfirm(UObject* Context, FArcCommandConfirmedDelegate ConfirmDelegate, Args... args)
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(Context);
		if (World == nullptr)
		{
			return false;
		}

		if (IArcReplicatedCommandHost* ArcPC = Cast<IArcReplicatedCommandHost>(GEngine->GetFirstLocalPlayerController(World)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
				MakeShareable<CommandType>(new CommandType {args...})
				, FGuid::NewGuid()
			);

			return ArcPC->SendReplicatedCommand(CommandHandle, MoveTemp(ConfirmDelegate));
		}

		return false;
	}

	/** Pre-built command + confirmation delegate; prefer the typed template overload in C++. */
	inline bool SendServerCommandConfirm(UObject* Context, FArcCommandConfirmedDelegate ConfirmDelegate, FArcReplicatedCommand*&& CommandPtr)
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(Context);
		if (World == nullptr)
		{
			return false;
		}

		if (IArcReplicatedCommandHost* ArcPC = Cast<IArcReplicatedCommandHost>(GEngine->GetFirstLocalPlayerController(World)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
				MakeShareable<FArcReplicatedCommand>(CommandPtr)
				, FGuid::NewGuid()
			);

			return ArcPC->SendReplicatedCommand(CommandHandle, MoveTemp(ConfirmDelegate));
		}

		return false;
	}

	/** SendServerCommandConfirm variant that takes UWorld* directly. */
	template <typename CommandType, typename... Args>
	inline bool SendServerCommandConfirm(UWorld* InWorld, FArcCommandConfirmedDelegate ConfirmDelegate, Args... args)
	{
		if (InWorld == nullptr)
		{
			return false;
		}

		if (IArcReplicatedCommandHost* ArcPC = Cast<IArcReplicatedCommandHost>(GEngine->GetFirstLocalPlayerController(InWorld)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
					MakeShareable<CommandType>(new CommandType {args...})
					, FGuid::NewGuid()
				);

			return ArcPC->SendReplicatedCommand(CommandHandle, MoveTemp(ConfirmDelegate));
		}

		return false;
	}

	/** Server → owning client dispatch; Context must implement IArcReplicatedCommandHost (typically the player controller). */
	template <typename CommandType, typename... Args>
	inline bool SendClientCommand(UObject* Context, Args... args)
	{
		IArcReplicatedCommandHost* ArcPC = Cast<IArcReplicatedCommandHost>(Context);
		// TODO:: Try to get it from PlayerState, Pawn, etc.
		if (ArcPC)
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
			MakeShareable<CommandType>(new CommandType {args...}));

			return ArcPC->SendClientReplicatedCommand(CommandHandle);
		}

		return false;
	}

	
}
