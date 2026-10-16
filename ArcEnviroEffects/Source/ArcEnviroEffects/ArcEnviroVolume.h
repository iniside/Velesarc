// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcEnviroVolume.generated.h"

class UBoxComponent;

UCLASS(meta = (ToolTip = "Box that overrides the effective Rain, Cold or Heat level for entities inside it. The highest Priority wins per axis; at equal priority the lower level wins."))
class ARCENVIROEFFECTS_API AArcEnviroVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcEnviroVolume();

	bool ContainsLocation(const FVector& Location) const;
	int32 GetPriority() const { return Priority; }
	TOptional<int32> GetRainOverride() const;
	TOptional<int32> GetColdOverride() const;
	TOptional<int32> GetHeatOverride() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Enviro", meta = (InlineEditConditionToggle))
	bool bOverrideRain = false;

	UPROPERTY(EditAnywhere, Category = "Enviro", meta = (EditCondition = "bOverrideRain", ClampMin = "0", ClampMax = "3"))
	int32 RainLevel = 0;

	UPROPERTY(EditAnywhere, Category = "Enviro", meta = (InlineEditConditionToggle))
	bool bOverrideCold = false;

	UPROPERTY(EditAnywhere, Category = "Enviro", meta = (EditCondition = "bOverrideCold", ClampMin = "0", ClampMax = "3"))
	int32 ColdLevel = 0;

	UPROPERTY(EditAnywhere, Category = "Enviro", meta = (InlineEditConditionToggle))
	bool bOverrideHeat = false;

	UPROPERTY(EditAnywhere, Category = "Enviro", meta = (EditCondition = "bOverrideHeat", ClampMin = "0", ClampMax = "3"))
	int32 HeatLevel = 0;

	UPROPERTY(EditAnywhere, Category = "Enviro")
	int32 Priority = 0;

	UPROPERTY(VisibleAnywhere, Category = "Enviro")
	TObjectPtr<UBoxComponent> BoxComponent;
};
