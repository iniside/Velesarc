// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/PCGPropertyAccessor.h"

#include "PCGSpawnArcIWEntities.generated.h"

class UMassEntityConfigAsset;
class UTransformProviderData;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSpawnArcIWEntitiesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SpawnArcIWEntities")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSpawnArcIWEntities", "NodeTitle", "Spawn ArcIW Entities"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bSpawnByAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bSpawnByAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector SpawnAttributeSelector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bSpawnByAttribute", EditConditionHides, OnlyPlaceable, DisallowCreateNew, PCG_Overridable))
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName GridName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TObjectPtr<UMassEntityConfigAsset> AdditionalEntityConfig;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMuteOnEmptyClass = false;

	/** Transform provider override for skinned meshes. Applied to all skinned mesh entries
	 *  unless overridden by a per-point TransformProvider attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TObjectPtr<UTransformProviderData> TransformProvider;
};

class FPCGSpawnArcIWEntitiesElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* InContext) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
