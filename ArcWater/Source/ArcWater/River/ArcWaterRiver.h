// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ArcWaterRiver.generated.h"

class UArcWaterRiverComponent;
class UArcWaterRiverSplineModifier;
class UPostProcessComponent;

UCLASS(ClassGroup = (ArcWater), HideCategories = (Rendering, Replication, Collision, Input, Physics, Networking, HLOD, Cooking, DataLayers, WorldPartition, LOD, Actor, Events, Tick, AI, Game, Mobile, RayTracing, TextureStreaming, Navigation, Lighting, Mobility, Advanced))
class ARCWATER_API AArcWaterRiver : public AActor
{
	GENERATED_BODY()

public:
	AArcWaterRiver();
	UArcWaterRiverSplineModifier* GetTerrainModifier() const { return TerrainModifier; }

	//~ Begin AActor interface
	virtual void PostRegisterAllComponents() override;
	//~ End AActor interface

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "River")
	TObjectPtr<UArcWaterRiverComponent> RiverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "River")
	TObjectPtr<UPostProcessComponent> UnderwaterPostProcess;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "River")
	TObjectPtr<UArcWaterRiverSplineModifier> TerrainModifier;
};
