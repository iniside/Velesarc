// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcLayeredMove_Root.h"

#include "Mover/ArcMovementModifiers.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLayeredMove_Root)

namespace ArcMover
{
	static bool IsRootMarkerFor(const TSharedPtr<FMovementModifierBase>& Modifier, const FGuid& SourceId)
	{
		return Modifier.IsValid()
			&& Modifier->GetScriptStruct() == FArcMovementModifier_Root::StaticStruct()
			&& static_cast<const FArcMovementModifier_Root*>(Modifier.Get())->SourceId == SourceId;
	}

	static bool HasRootMarker(const FMovementModifierGroup& Modifiers, const FGuid& SourceId)
	{
		for (auto It = Modifiers.GetActiveModifiersIterator(); It; ++It)
		{
			if (IsRootMarkerFor(*It, SourceId))
			{
				return true;
			}
		}
		for (auto It = Modifiers.GetQueuedModifiersIterator(); It; ++It)
		{
			if (IsRootMarkerFor(*It, SourceId))
			{
				return true;
			}
		}
		return false;
	}
}

FMovementModifierHandle ArcMover::QueueRoot(UMoverComponent& Mover, TSharedPtr<FArcMovementModifier_Root> Marker)
{
	if (!Marker.IsValid())
	{
		return FMovementModifierHandle();
	}

	if (!Marker->SourceId.IsValid())
	{
		Marker->SourceId = FGuid::NewGuid();
	}

	const FMovementModifierHandle MarkerHandle = Mover.QueueModifier(FQueueModifierParams(Marker));
	if (!MarkerHandle.IsValid())
	{
		return MarkerHandle;
	}

	TSharedPtr<FArcLayeredMove_Root> RootMove = MakeShared<FArcLayeredMove_Root>();
	RootMove->SourceId = Marker->SourceId;
	Mover.QueueLayeredMove(FQueueLayeredMoveParams(RootMove));
	return MarkerHandle;
}

FArcLayeredMove_Root::FArcLayeredMove_Root()
{
	MixMode = EMoveMixMode::OverrideAll;
	Priority = 255;
	DurationMs = -1.0f;
}

bool FArcLayeredMove_Root::GenerateMove(const FLayeredMoveGenerateMoveParams& Params, FProposedMove& OutProposedMove)
{
	return GenerateRootMove(Params.StartState, OutProposedMove);
}

bool FArcLayeredMove_Root::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return GenerateRootMove(StartState, OutProposedMove);
}

bool FArcLayeredMove_Root::GenerateRootMove(const FMoverTickStartData& StartState, FProposedMove& OutProposedMove)
{
	if (!SourceId.IsValid() || !ArcMover::HasRootMarker(StartState.SyncState.MovementModifiers, SourceId))
	{
		DurationMs = 0.0f;
		return false;
	}

	OutProposedMove.MixMode = EMoveMixMode::OverrideAll;
	OutProposedMove.LinearVelocity = FVector::ZeroVector;
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
	OutProposedMove.DirectionIntent = FVector::ZeroVector;
	OutProposedMove.bHasDirIntent = false;
	return true;
}

FLayeredMoveBase* FArcLayeredMove_Root::Clone() const
{
	return new FArcLayeredMove_Root(*this);
}

void FArcLayeredMove_Root::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << SourceId;
}

UScriptStruct* FArcLayeredMove_Root::GetScriptStruct() const
{
	return FArcLayeredMove_Root::StaticStruct();
}
