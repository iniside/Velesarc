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

#include "Commands/ArcReplicatedCommandDispatch.h"

#include "ArcLogs.h"

namespace Arcx::ReplicatedCommands
{
	bool PreparePredictedSend(const FArcReplicatedCommandHandle& Command
		, UObject* WorldContext
		, double NowSeconds
		, bool bPredictionReplaying
		, uint32& PredictionSequence
		, TArray<FArcPredictedCommand>& PredictedCommands)
	{
		if (bPredictionReplaying)
		{
			UE_LOGF(LogArcCore, Warning, "Predicted command %ls not sent: sent from inside a prediction undo or replay"
				, *Command.GetData()->GetScriptStruct()->GetName());
			return false;
		}

		if (!Command.CanSendCommand())
		{
			return false;
		}

		Command.PreSendCommand();

		if (!Command.Execute(WorldContext))
		{
			return false;
		}

		FArcPredictedCommand& Entry = PredictedCommands.AddDefaulted_GetRef();
		Entry.CommandId = Command.GetCommandId();
		Entry.PredictionSequence = ++PredictionSequence;
		Entry.SendTimeSeconds = NowSeconds;
		Entry.Command = Command.GetData();
		return true;
	}

	bool PreparePredictedSendWithConfirm(const FArcReplicatedCommandHandle& Command
		, FArcCommandConfirmedDelegate&& ConfirmDelegate
		, UObject* WorldContext
		, double NowSeconds
		, bool bPredictionReplaying
		, uint32& PredictionSequence
		, TArray<FArcPredictedCommand>& PredictedCommands
		, TMap<FGuid, FArcCommandConfirmedDelegate>& CommandResponses)
	{
		if (!PreparePredictedSend(Command, WorldContext, NowSeconds, bPredictionReplaying, PredictionSequence, PredictedCommands))
		{
			return false;
		}

		CommandResponses.Add(Command.GetCommandId(), MoveTemp(ConfirmDelegate));
		return true;
	}

	bool HandlePredictionConfirm(const FGuid& CommandId
		, bool bResult
		, UObject* WorldContext
		, bool& bPredictionReplaying
		, TArray<FArcPredictedCommand>& PredictedCommands
		, FArcPredictedCommand& OutRejected)
	{
		const int32 Index = PredictedCommands.IndexOfByPredicate([&CommandId](const FArcPredictedCommand& Entry)
		{
			return Entry.CommandId == CommandId;
		});

		if (Index == INDEX_NONE)
		{
			return false;
		}

		if (bResult)
		{
			PredictedCommands.RemoveAt(Index);
			return false;
		}

		OutRejected = PredictedCommands[Index];

		TArray<FArcPredictedCommand> LaterCommands;
		for (const FArcPredictedCommand& Entry : PredictedCommands)
		{
			if (Entry.PredictionSequence > OutRejected.PredictionSequence)
			{
				LaterCommands.Add(Entry);
			}
		}

		LaterCommands.Sort([](const FArcPredictedCommand& Left, const FArcPredictedCommand& Right)
		{
			return Left.PredictionSequence < Right.PredictionSequence;
		});

		TGuardValue<bool> ReplayGuard(bPredictionReplaying, true);

		for (int32 LaterIndex = LaterCommands.Num() - 1; LaterIndex >= 0; --LaterIndex)
		{
			LaterCommands[LaterIndex].Command->UndoPrediction(WorldContext);
		}

		OutRejected.Command->UndoPrediction(WorldContext);
		PredictedCommands.RemoveAt(Index);

		for (const FArcPredictedCommand& Entry : LaterCommands)
		{
			Entry.Command->Execute(WorldContext);
		}

		return true;
	}
}
