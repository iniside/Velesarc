// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/AssetFactoryInterface.h"
#include "ArcPlacedEntityPlacementFactory.generated.h"

/**
 * Placement factory that enables dragging UMassEntityConfigAsset from the Content Browser
 * into the viewport, adding instances to AArcPlacedEntityPartitionActor.
 */
UCLASS()
class UArcPlacedEntityPlacementFactory : public UObject, public IAssetFactoryInterface
{
	GENERATED_BODY()

public:
	//~ Begin IAssetFactoryInterface
	virtual bool CanPlaceElementsFromAssetData(const FAssetData& InAssetData) override;
	virtual bool PrePlaceAsset(FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	virtual TArray<FTypedElementHandle> PlaceAsset(const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	virtual void PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	virtual FAssetData GetAssetDataFromElementHandle(const FTypedElementHandle& InHandle) override;
	virtual void BeginPlacement(const FPlacementOptions& InPlacementOptions) override;
	virtual void EndPlacement(TArrayView<const FTypedElementHandle> InPlacedElements, const FPlacementOptions& InPlacementOptions) override;
	virtual UInstancedPlacemenClientSettings* FactorySettingsObjectForPlacement(const FAssetData& InAssetData, const FPlacementOptions& InPlacementOptions) override;
	//~ End IAssetFactoryInterface
};
