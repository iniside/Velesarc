// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ArcTerrainChannelSet.generated.h"

UCLASS(BlueprintType)
class ARCTERRAIN_API UArcTerrainChannelSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Channels")
	TArray<FName> Channels;

	/** Stable index of a channel by name, or INDEX_NONE if absent. */
	int32 FindChannelIndex(FName InChannel) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

namespace ArcTerrain
{
	constexpr int32 GBaseChannelIndex = 0;

	/** The lane carrying the base remainder, INDEX_NONE when a set of NumChannels cannot address it. The
	 *  normalize shader bakes the lane in at compile time, so an unaddressable base has to be refused by
	 *  every rail rather than degraded per rail. */
	constexpr int32 ResolveBaseChannelIndex(int32 NumChannels)
	{
		return (GBaseChannelIndex >= 0 && GBaseChannelIndex < NumChannels) ? GBaseChannelIndex : INDEX_NONE;
	}

	/** Case-normalized so it agrees with FName's case-insensitive equality once persisted on disk.
	 *  May legitimately return 0 — never treat 0 as an "absent plane" sentinel. */
	ARCTERRAIN_API uint32 ChannelNameHash(FName Channel);
}
