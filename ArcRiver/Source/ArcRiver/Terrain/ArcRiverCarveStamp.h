// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "ArcRiverCarveStamp.generated.h"

class UArcTerrainTexturePatchModifier;

/**
 * Optional deepening of the river channels the Rivers mask marks. Place it on the SAME actor and the
 * same attachment parent as the UArcTerrainTexturePatchModifier that carries the mask: the footprint
 * and the transform are taken from that sibling, so the carve lands on the patch's rectangle.
 *
 * CarveDepth defaults to 0 and the component then contributes nothing — the base Gaea heightmap
 * already carries the channels, and this only adds to them.
 */
UCLASS(ClassGroup = (ArcRiver), meta = (BlueprintSpawnableComponent, DisplayName = "ArcRiver Carve Stamp"))
class ARCRIVER_API UArcRiverCarveStamp : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UArcRiverCarveStamp();

	UPROPERTY(EditAnywhere, Category = "Carve",
		meta = (ClampMin = "0.0", UIMax = "5000.0", ForceUnits = "cm"))
	float CarveDepth = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Carve",
		meta = (ClampMin = "0.0", UIMax = "20000.0", ForceUnits = "cm"))
	float BankSoftness = 500.0f;

	/** Texels per side of the carve field. Capped at 2048 because the provider's distance transform
	 *  holds Resolution * Resolution doubles - 32 MB at the cap. */
	UPROPERTY(EditAnywhere, Category = "Carve", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 CarveResolution = 512;

	//~ Begin UActorComponent / UArcTerrainStampComponent interface
	virtual void OnRegister() override;
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UActorComponent / UArcTerrainStampComponent interface

private:
	/** The owner's single texture patch modifier; null when there is none or more than one. */
	const UArcTerrainTexturePatchModifier* FindPatchModifier() const;

	void SyncFootprintFromPatchModifier();
};
