// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "ZoneGraphTypes.h"
#include "Navigation/ArcWorldNavTypes.h"
#include "ArcWorldRouteSubsystem.generated.h"

class UZoneGraphSubsystem;

UCLASS()
class ARCAI_API UArcWorldRouteSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UArcWorldRouteSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;

	bool FindRoute(
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FZoneGraphTagFilter& TagFilter,
		FArcWorldRouteFragment& OutRoute) const;

	bool FindNearestLane(
		const FVector& Location,
		float QueryRadius,
		FZoneGraphLaneLocation& OutLaneLocation) const;

private:
	UPROPERTY()
	TObjectPtr<UZoneGraphSubsystem> ZoneGraphSubsystem = nullptr;
};
