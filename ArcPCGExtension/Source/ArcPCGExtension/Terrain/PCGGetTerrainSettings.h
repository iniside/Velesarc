// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Elements/PCGDataFromActor.h"
#include "Terrain/PCGTerrainData.h"

#include "PCGGetTerrainSettings.generated.h"

/**
 * "Get ArcTerrain Data" — produces a single UPCGTerrainData surface from the first selected AArcTerrain.
 * Mirrors UPCGGetLandscapeSettings; the per-cell read runs once at element execute
 * (FPCGGetTerrainElement::ProcessActors) into an immutable FArcTerrainSurfaceCache.
 *
 * The source is the BAKED height TILE on disk, NOT the live GPU composite: the composite is an
 * edit-time representation with editor-only stamp inputs and does not survive cook. There is therefore
 * exactly ONE code path — editor, PIE and cooked behave identically — and no #if WITH_EDITOR in the
 * element. A cell with no tile on disk is flat ground at the terrain's Base Height; only a cell whose
 * tile exists but cannot be read is a hole.
 */
UCLASS()
class ARCPCGEXTENSION_API UPCGGetTerrainSettings : public UPCGDataFromActorSettings
{
	GENERATED_BODY()

public:
	/** Self-resolving actor selection: defaults the ActorSelector to ByClass=AArcTerrain over
	 *  AllWorldActors (overlapping the PCG component). The graph asset therefore discovers the terrain
	 *  from the world at execute time and never bakes a reference to a specific placed actor. */
	UPCGGetTerrainSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetArcTerrainData")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGetTerrainElement", "NodeTitle", "Get ArcTerrain Data"); }
	virtual FText GetNodeTooltipText() const override;
#endif
	//~End UPCGSettings interface

	//~Begin UPCGDataFromActorSettings interface
	virtual EPCGDataType GetDataFilter() const override;
	virtual TSubclassOf<AActor> GetDefaultActorSelectorClass() const override;
	//~End UPCGDataFromActorSettings interface

	/** Sampling options forwarded to the produced UPCGTerrainData. Currently empty (the former
	 *  bGetLayerWeights is gone — see FPCGTerrainDataProps); kept as the node's override surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGTerrainDataProps SamplingProperties;

protected:
	//~Begin UPCGSettings interface
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class FPCGGetTerrainElement : public FPCGDataFromActorElement
{
protected:
	//~Begin FPCGDataFromActorElement interface
	virtual void ProcessActors(FPCGContext* Context, const UPCGDataFromActorSettings* Settings, const TArray<AActor*>& FoundActors, TArray<FPCGTaskId>& OutDynamicDependencies) const override;
	//~End FPCGDataFromActorElement interface
};
