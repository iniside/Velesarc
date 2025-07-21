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
#include "Player/ArcCorePlayerController.h"

class AArcCorePlayerController;
class UObject;

namespace Arcx
{
	template <typename CommandType, typename... Args>
	inline bool SendServerCommand(UObject* Context, Args... args)
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(Context);
		if (World == nullptr)
		{
			return false;
		}
		
		if (AArcCorePlayerController* ArcPC = Cast<AArcCorePlayerController>(GEngine->GetFirstLocalPlayerController(World)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
			MakeShareable<CommandType>(new CommandType {args...}));

			return ArcPC->SendReplicatedCommand(CommandHandle);
		}

		return false;
	}

	inline bool SendServerCommand(UObject* Context, FArcReplicatedCommand*&& CommandPtr)
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(Context);
		if (World == nullptr)
		{
			return false;
		}
		
		if (AArcCorePlayerController* ArcPC = Cast<AArcCorePlayerController>(GEngine->GetFirstLocalPlayerController(World)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
			MakeShareable<FArcReplicatedCommand>(CommandPtr));

			return ArcPC->SendReplicatedCommand(CommandHandle);
		}

		return false;
	}
	
	template <typename CommandType, typename... Args>
	inline bool SendServerCommand(UWorld* InWorld, Args... args)
	{
		if (InWorld == nullptr)
		{
			return false;
		}
		
		if (AArcCorePlayerController* ArcPC = Cast<AArcCorePlayerController>(GEngine->GetFirstLocalPlayerController(InWorld)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
			MakeShareable<CommandType>(new CommandType {args...}));

			return ArcPC->SendReplicatedCommand(CommandHandle);
		}

		return false;
	}

	template <typename CommandType, typename... Args>
	inline bool SendServerCommandConfirm(UObject* Context, FArcCommandConfirmedDelegate ConfirmDelegate, Args... args)
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(Context);
		if (World == nullptr)
		{
			return false;
		}
		
		if (AArcCorePlayerController* ArcPC = Cast<AArcCorePlayerController>(GEngine->GetFirstLocalPlayerController(World)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
				MakeShareable<CommandType>(new CommandType {args...})
				, FArcReplicatedCommandId::Generate()
			);

			return ArcPC->SendReplicatedCommand(CommandHandle, MoveTemp(ConfirmDelegate));
		}

		return false;
	}

	template <typename CommandType, typename... Args>
	inline bool SendServerCommandConfirm(UWorld* InWorld, FArcCommandConfirmedDelegate ConfirmDelegate, Args... args)
	{
		if (InWorld == nullptr)
		{
			return false;
		}
		
		if (AArcCorePlayerController* ArcPC = Cast<AArcCorePlayerController>(GEngine->GetFirstLocalPlayerController(InWorld)))
		{
			FArcReplicatedCommandHandle CommandHandle = FArcReplicatedCommandHandle(
					MakeShareable<CommandType>(new CommandType {args...})
					, FArcReplicatedCommandId::Generate()
				);

			return ArcPC->SendReplicatedCommand(CommandHandle);
		}

		return false;
	}
	
	template <typename CommandType, typename... Args>
	inline bool SendClientCommand(UObject* Context, Args... args)
	{
		AArcCorePlayerController* ArcPC = Cast<AArcCorePlayerController>(Context);
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
