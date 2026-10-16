// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcRPGHitZoneConfig.generated.h"

USTRUCT(BlueprintType)
struct ARCRPGMECHANICS_API FArcRPGHitZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Zone", meta = (Categories = "ArcMech.Hit.Zone", ToolTip = "Must be a child of ArcMech.Hit.Zone. Armor pieces carry this tag in their Item Tags."))
	FGameplayTag Zone;

	UPROPERTY(EditAnywhere, Category = "Zone", meta = (ClampMin = "0"))
	float Weight = 0.f;
};

UCLASS(BlueprintType, meta = (ToolTip = "Hit zones shared by every RPG Damage Execution that references it, rolled once per applied hit by weight. An execution without one rolls no zone, so no armor piece is struck."))
class ARCRPGMECHANICS_API UArcRPGHitZoneConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Hit Zones", meta = (ToolTip = "Zones rolled once per applied hit by weight. Periodic ticks roll no zone."))
	TArray<FArcRPGHitZone> Zones;

	FGameplayTag RollZone() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
