// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWBlueprintLibrary.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "Engine/Engine.h"

FMassEntityHandle UArcIWBlueprintLibrary::ResolveHitToEntity(UObject* WorldContextObject, const FHitResult& HitResult)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return FMassEntityHandle();
	}

	UArcIWVisualizationSubsystem* VisSub = World->GetSubsystem<UArcIWVisualizationSubsystem>();
	if (!VisSub)
	{
		return FMassEntityHandle();
	}

	return VisSub->ResolveHitToEntity(HitResult);
}

void UArcIWBlueprintLibrary::HydrateEntity(UObject* WorldContextObject, FMassEntityHandle EntityHandle)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	UArcIWVisualizationSubsystem* VisSub = World->GetSubsystem<UArcIWVisualizationSubsystem>();
	if (!VisSub)
	{
		return;
	}

	VisSub->HydrateEntity(EntityHandle);
}
