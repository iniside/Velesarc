// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MassSystem.h"
#include "Sim/ArcItemTagLookup.h"
#include "Sim/ArcSettlementLedgerTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcSettlementLedgerSubsystem.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcSettlementLedgerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	TArray<FArcNeedRow> Needs;
	TArray<FArcHaveRow> Haves;
	TArray<FArcHaveRow> StorageHaves;

	void CollectItemIdsForAssetTags(
		const FGameplayTagContainer& RequiredTags,
		const FGameplayTagContainer& DenyTags,
		TArray<FPrimaryAssetId>& OutIds);

private:
	TMap<ArcWorldSim::FArcAssetTagFilterKey, TArray<FPrimaryAssetId>> ItemIdsByAssetTags;
};

USTRUCT()
struct FMassSystem_ArcSettlementLedgerSubsystem : public FMassSystem
{
	GENERATED_BODY()
};

UE_DECLARE_MASS_SUBSYSTEM_WRAPPER(UArcSettlementLedgerSubsystem, FMassSystem_ArcSettlementLedgerSubsystem, /*GameThreadOnly=*/true, /*ThreadSafeWrite=*/false);
