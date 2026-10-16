// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcSettlementLedgerSubsystem.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Sim/ArcItemTagLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSettlementLedgerSubsystem)

UE_DEFINE_MASS_SUBSYSTEM_WRAPPER(UArcSettlementLedgerSubsystem, FMassSystem_ArcSettlementLedgerSubsystem);

void UArcSettlementLedgerSubsystem::CollectItemIdsForAssetTags(
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& DenyTags,
	TArray<FPrimaryAssetId>& OutIds)
{
	ArcWorldSim::FArcAssetTagFilterKey Key;
	Key.RequiredTags = RequiredTags;
	Key.DenyTags = DenyTags;

	if (const TArray<FPrimaryAssetId>* Cached = ItemIdsByAssetTags.Find(Key))
	{
		OutIds = *Cached;
		return;
	}

	ArcWorldSim::CollectItemIdsForAssetTags(RequiredTags, DenyTags, OutIds);

	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (AssetRegistry == nullptr || AssetRegistry->IsGathering())
	{
		return;
	}

	ItemIdsByAssetTags.Add(Key, OutIds);
}
