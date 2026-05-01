// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Fragments/ArcMassReplicationFilter.h"
#include "ArcMassReplicationConfigFragment.generated.h"

class UMassEntityConfigAsset;

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcMassEntityReplicationConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Replication")
	TArray<FArcMassReplicatedFragmentEntry> ReplicatedFragmentEntries;

	UPROPERTY(EditAnywhere, Category = "Replication")
	float CullDistance = 15000.f;

	UPROPERTY(EditAnywhere, Category = "Replication")
	float CellSize = 10000.f;

	UPROPERTY()
	TObjectPtr<UMassEntityConfigAsset> EntityConfigAsset = nullptr;
};
