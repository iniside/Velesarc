// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassReplicationFilter.generated.h"

UENUM(BlueprintType)
enum class EArcMassReplicationFilter : uint8
{
	Spatial,
	OwnerOnly
};

USTRUCT(BlueprintType)
struct ARCMASSREPLICATIONRUNTIME_API FArcMassReplicatedFragmentEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Replication")
	TObjectPtr<UScriptStruct> FragmentType = nullptr;

	UPROPERTY(EditAnywhere, Category = "Replication")
	EArcMassReplicationFilter Filter = EArcMassReplicationFilter::Spatial;
};
