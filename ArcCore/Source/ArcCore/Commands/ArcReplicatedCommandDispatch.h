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

#include "Commands/ArcReplicatedCommand.h"

struct FArcPredictedCommand
{
	FGuid CommandId;
	uint32 PredictionSequence = 0;
	double SendTimeSeconds = 0.0;
	TSharedPtr<FArcReplicatedCommand> Command;
};

namespace Arcx::ReplicatedCommands
{
	inline bool PrepareSend(const FArcReplicatedCommandHandle& Command)
	{
		if (Command.CanSendCommand())
		{
			Command.PreSendCommand();
			return true;
		}

		return false;
	}

	inline bool PrepareSendWithConfirm(const FArcReplicatedCommandHandle& Command
		, FArcCommandConfirmedDelegate&& ConfirmDelegate
		, TMap<FGuid, FArcCommandConfirmedDelegate>& CommandResponses)
	{
		if (Command.CanSendCommand())
		{
			CommandResponses.Add(Command.GetCommandId(), MoveTemp(ConfirmDelegate));
			Command.PreSendCommand();
			return true;
		}

		return false;
	}

	ARCCORE_API bool PreparePredictedSend(const FArcReplicatedCommandHandle& Command
		, UObject* WorldContext
		, double NowSeconds
		, bool bPredictionReplaying
		, uint32& PredictionSequence
		, TArray<FArcPredictedCommand>& PredictedCommands);

	ARCCORE_API bool PreparePredictedSendWithConfirm(const FArcReplicatedCommandHandle& Command
		, FArcCommandConfirmedDelegate&& ConfirmDelegate
		, UObject* WorldContext
		, double NowSeconds
		, bool bPredictionReplaying
		, uint32& PredictionSequence
		, TArray<FArcPredictedCommand>& PredictedCommands
		, TMap<FGuid, FArcCommandConfirmedDelegate>& CommandResponses);

	/** Returns true when CommandId was a predicted command the server rejected; OutRejected holds its removed entry. */
	ARCCORE_API bool HandlePredictionConfirm(const FGuid& CommandId
		, bool bResult
		, UObject* WorldContext
		, bool& bPredictionReplaying
		, TArray<FArcPredictedCommand>& PredictedCommands
		, FArcPredictedCommand& OutRejected);

	inline bool PrepareClientSend(const FArcReplicatedCommandHandle& Command)
	{
		if (Command.CanSendCommand())
		{
			Command.PreSendCommand();
			return true;
		}

		return false;
	}

	inline void HandleConfirm(const FGuid& CommandId
		, bool bResult
		, TMap<FGuid, FArcCommandConfirmedDelegate>& CommandResponses)
	{
		if (FArcCommandConfirmedDelegate* Function = CommandResponses.Find(CommandId))
		{
			Function->ExecuteIfBound(bResult);
			CommandResponses.Remove(CommandId);
		}
	}

	inline void HandleClientExecute(const FArcReplicatedCommandHandle& Command, UObject* WorldContext)
	{
		Command.Execute(WorldContext);
	}
}
