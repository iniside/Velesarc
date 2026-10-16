// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnviroEffectsSubsystem.h"

#include "ArcEnviroVolume.h"

namespace ArcEnviroEffectsSubsystemPrivate
{
	struct FAxisPick
	{
		TOptional<int32> Level;
		int32 Priority = 0;

		void Consider(const TOptional<int32>& CandidateLevel, int32 CandidatePriority)
		{
			if (!CandidateLevel.IsSet())
			{
				return;
			}

			const bool bWins = !Level.IsSet()
				|| CandidatePriority > Priority
				|| (CandidatePriority == Priority && CandidateLevel.GetValue() < Level.GetValue());
			if (bWins)
			{
				Level = CandidateLevel;
				Priority = CandidatePriority;
			}
		}
	};
}

void UArcEnviroEffectsSubsystem::Deinitialize()
{
	Volumes.Empty();
	Super::Deinitialize();
}

void UArcEnviroEffectsSubsystem::RegisterVolume(AArcEnviroVolume* Volume)
{
	if (Volume)
	{
		Volumes.AddUnique(Volume);
	}
}

void UArcEnviroEffectsSubsystem::UnregisterVolume(AArcEnviroVolume* Volume)
{
	Volumes.RemoveSingleSwap(Volume);
}

FArcEnviroLevelOverrides UArcEnviroEffectsSubsystem::GetOverridesAtLocation(const FVector& Location) const
{
	ArcEnviroEffectsSubsystemPrivate::FAxisPick Rain;
	ArcEnviroEffectsSubsystemPrivate::FAxisPick Cold;
	ArcEnviroEffectsSubsystemPrivate::FAxisPick Heat;

	for (const TObjectPtr<AArcEnviroVolume>& Volume : Volumes)
	{
		if (!Volume || !Volume->ContainsLocation(Location))
		{
			continue;
		}

		const int32 Priority = Volume->GetPriority();
		Rain.Consider(Volume->GetRainOverride(), Priority);
		Cold.Consider(Volume->GetColdOverride(), Priority);
		Heat.Consider(Volume->GetHeatOverride(), Priority);
	}

	FArcEnviroLevelOverrides Result;
	Result.Rain = Rain.Level;
	Result.Cold = Cold.Level;
	Result.Heat = Heat.Level;
	return Result;
}
