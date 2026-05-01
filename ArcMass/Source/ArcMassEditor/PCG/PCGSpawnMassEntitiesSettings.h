// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGSpawnMassEntitiesSettings.generated.h"

class UMassEntityConfigAsset;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSpawnMassEntitiesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SpawnMassEntities")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSpawnMassEntities", "NodeTitle", "Spawn Mass Entities"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Default MassEntityConfig used when a point has no per-point config override attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TObjectPtr<UMassEntityConfigAsset> DefaultEntityConfig;

	/** Per-point attribute name holding an FSoftObjectPath to a UMassEntityConfigAsset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName EntityConfigAttributeName = TEXT("MassEntityConfig");
};

class FPCGSpawnMassEntitiesElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* InContext) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
