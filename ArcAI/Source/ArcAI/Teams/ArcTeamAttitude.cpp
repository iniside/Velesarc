// Copyright Lukasz Baran. All Rights Reserved.

#include "Teams/ArcTeamAttitude.h"

EArcTeamAttitude ArcTeamAttitude::Resolve(int32 ObserverTeamId, int32 TargetTeamId)
{
	// The order is the contract. Teamless and same-team are answered before the table is reached, so
	// no authored row can override them and no row naming those cases is ever consulted.
	if (ObserverTeamId == INDEX_NONE || TargetTeamId == INDEX_NONE)
	{
		return EArcTeamAttitude::Neutral;
	}

	if (ObserverTeamId == TargetTeamId)
	{
		return EArcTeamAttitude::Friendly;
	}

	for (const FArcTeamRelation& Relation : GetDefault<UArcTeamAttitudeSettings>()->Relations)
	{
		if ((Relation.TeamA == ObserverTeamId && Relation.TeamB == TargetTeamId)
			|| (Relation.TeamA == TargetTeamId && Relation.TeamB == ObserverTeamId))
		{
			return Relation.Attitude;
		}
	}

	return EArcTeamAttitude::Neutral;
}

#if WITH_EDITOR
DEFINE_LOG_CATEGORY_STATIC(LogArcTeamAttitude, Log, All);

void UArcTeamAttitudeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// A courtesy for whoever is editing Project Settings, never a guard: resolution already answers
	// both of these before the table is read, and a hand-edited ini never reaches this at all.
	for (int32 Index = 0; Index < Relations.Num(); ++Index)
	{
		const FArcTeamRelation& Relation = Relations[Index];

		UE_CLOG(Relation.TeamA == Relation.TeamB, LogArcTeamAttitude, Warning,
			TEXT("Team relation %d names team %d on both sides; same-team is always Friendly and this row is never consulted."),
			Index, Relation.TeamA);

		UE_CLOG(Relation.TeamA == INDEX_NONE || Relation.TeamB == INDEX_NONE, LogArcTeamAttitude, Warning,
			TEXT("Team relation %d names a teamless side; a teamless entity is always Neutral and this row is never consulted."),
			Index);
	}
}
#endif
