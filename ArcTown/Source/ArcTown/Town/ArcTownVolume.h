// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/ArcTownDebugDrawTypes.h"
#include "GameFramework/Actor.h"
#include "Misc/DataValidation.h"
#include "Simulation/ArcTownSimulationInput.h"
#include "Town/ArcTownSiteSnapshot.h"

#include "ArcTownVolume.generated.h"

class UArcTownBuildingCatalog;
class UArcTownRoadCatalog;
class UArcTownSimulationState;
class UBoxComponent;

/** One partition actor a town owns. */
USTRUCT()
struct ARCTOWN_API FArcTownEmittedActor
{
	GENERATED_BODY()

	/** Resolves an actor that is loaded, including one spawned this session and not yet written to a desc. */
	UPROPERTY()
	TSoftObjectPtr<AActor> Actor;

	/** World Partition pin key; a soft path cannot bring back an unloaded actor. */
	UPROPERTY()
	FGuid ActorGuid;
};

UCLASS(BlueprintType)
class ARCTOWN_API AArcTownVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcTownVolume();

	/** Owns this town's partition actors, stamps and content ids; normal duplication mints a new identity. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid TownId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Town")
	TObjectPtr<UBoxComponent> VolumeBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Town")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	TObjectPtr<UArcTownBuildingCatalog> Catalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	TObjectPtr<UArcTownRoadCatalog> RoadCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.0", Units = "cm"))
	double PadFalloffBand = 150.0;

	/** Composite order of pad stamps against every OTHER terrain stamp (lakes, texture patches, etc); lower composites first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	double PadStampPriority = 105.0;

	/** Composite order of path stamps against every OTHER terrain stamp (lakes, texture patches, etc); lower composites first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paths")
	double PathStampPriority = 102.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paths", meta = (ClampMin = "0.0", Units = "cm"))
	double PathFalloffCm = 600.0;

	/** Height the road surface sits above the sampled ground — the embankment the falloff band shoulders down to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paths", meta = (ClampMin = "0.0", Units = "cm"))
	double PathRaiseCm = 30.0;

	/** Lattice spacing an editor sample pass lays over the terrain under this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site", meta = (ClampMin = "1.0", Units = "cm"))
	double SampleSpacing = 200.0;

	/** Sampled lattice rows kept outside the volume on every side, so a site cell's catchment is real terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site", meta = (ClampMin = "1"))
	int32 SiteMarginCells = 16;

	/** Rivers map reading at or above which a sampled cell counts as the water channel itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RiversChannelThreshold = 0.5f;

	/** Authoritative sampled terrain, replaced only by an editor sample pass. */
	UPROPERTY()
	FArcTownSiteSnapshot SiteSnapshot;

	UPROPERTY(EditAnywhere, Category = "Simulation")
	FArcTownGrowthSettings GrowthSettings;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "1"))
	int32 YearsPerRun = 25;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Simulation")
	TObjectPtr<UArcTownSimulationState> SimulationState;

	/** Resolves every UObject a run reads into values, because the worker touches none of them. */
	bool FreezeSimulationInput(int32 YearsToSimulate, FArcTownSimulationInput& OutInput, FText& OutError) const;

	/** World-space thickness of debug lines. DebugDraw shrinks these with distance, so a town-wide overlay needs metres, not centimetres. 0 draws a perspective-independent one-pixel hairline. Two layers ignore it: a road ribbon carries the street's own width, and a building box is edged by a hairline whatever this says. */
	UPROPERTY(EditAnywhere, Category = "Debug", DisplayName = "Debug Line Thickness",
		meta = (ClampMin = "0.0", Units = "cm"))
	float DebugLineThicknessCm = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Debug", DisplayName = "Debug Surface Lift",
		meta = (ClampMin = "1.0", Units = "cm",
		ToolTip = "Raises every draped overlay above the sampled surface so it does not z-fight the terrain. The draped layers stack in whole multiples of it: the site field sheet at one, the road ribbon at two, and every line, node and box above at three, so no two of them share a plane."))
	double DebugSurfaceLiftCm = 25.0;

	UPROPERTY(EditAnywhere, Category = "Debug", DisplayName = "Debug Overlay Opacity",
		meta = (ClampMin = "1.0", ClampMax = "100.0", Units = "Percent",
		ToolTip = "Opacity of every filled overlay surface: the site field sheet, the road ribbons and the building boxes. Below 100 the fill stops writing depth, so overlapping buildings no longer occlude one another and a dense town reads muddier; 100 restores that occlusion but paints over the editor's transform gizmo. Lines, node points and labels ignore it."))
	float DebugOverlayOpacity = 65.0f;

	UPROPERTY(EditAnywhere, Category = "Debug|Structure", DisplayName = "Draw Volume Bounds")
	bool bDrawVolumeBounds = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Structure", DisplayName = "Draw Site Grid",
		meta = (ToolTip = "Outlines the site rect the simulation grows in and, outside it, the margined grid the sample pass took, so the band Site Margin Cells bought is visible."))
	bool bDrawSiteGrid = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Draw Site Field")
	bool bDrawSiteField = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Site Field",
		meta = (EditCondition = "bDrawSiteField"))
	EArcTownDebugSiteField SiteField = EArcTownDebugSiteField::Buildable;

	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Site Field Stride",
		meta = (EditCondition = "bDrawSiteField", ClampMin = "0",
		ToolTip = "0 auto-fits the sheet to Max Drawn Site Cells; above 0 samples every Nth cell explicitly."))
	int32 SiteFieldStride = 0;

	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Site Field Cell Scale",
		meta = (EditCondition = "bDrawSiteField", ClampMin = "0.05", ClampMax = "1.0",
		ToolTip = "Shrinks each sampled quad about its own centre as a fraction of its stride footprint: 1.0 is a continuous sheet, 0.5 leaves a regular gap grid."))
	float SiteFieldCellScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Debug|Tensor Field", DisplayName = "Draw Tensor Field",
		meta = (ToolTip = "Draws the major direction of the road tensor field the run reads, at the core the bootstrap chose."))
	bool bDrawTensorField = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Tensor Field", DisplayName = "Tensor Field Stride",
		meta = (EditCondition = "bDrawTensorField", ClampMin = "0",
		ToolTip = "0 auto-fits the lattice to Max Drawn Tensor Samples; above 0 samples every Nth cell explicitly."))
	int32 TensorFieldStride = 0;

	UPROPERTY(EditAnywhere, Category = "Debug|Roads", DisplayName = "Draw Roads",
		meta = (ToolTip = "Drapes the road graph the run produced over the sampled ground as a flat ribbon at each street's own width: built streets continuous, planned ones broken into dashes, nodes marked by whether they stopped expanding."))
	bool bDrawRoads = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Roads", DisplayName = "Road Color",
		meta = (EditCondition = "bDrawRoads",
		ToolTip = "Built Planned separates the two build states, Traffic ramps over the drawn traffic range, Hierarchy steps through the promotion tiers."))
	EArcTownDebugRoadColor RoadColor = EArcTownDebugRoadColor::BuiltPlanned;

	UPROPERTY(EditAnywhere, Category = "Debug|Buildings", DisplayName = "Draw Buildings",
		meta = (ToolTip = "Draws each placed building as a filled box standing on the sampled ground, sized in plan from its authored footprint and turned to its placed yaw. The catalog authors no height, so the box takes a stand-in of one to two storeys derived from its shorter footprint axis."))
	bool bDrawBuildings = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Buildings", DisplayName = "Building Color",
		meta = (EditCondition = "bDrawBuildings",
		ToolTip = "Birth Year ramps over the drawn year range, Catalog Entry gives each catalog index its own hue, Frontage marks buildings whose street edge no longer resolves."))
	EArcTownDebugBuildingColor BuildingColor = EArcTownDebugBuildingColor::BirthYear;

	UPROPERTY(EditAnywhere, Category = "Debug|Morphology", DisplayName = "Draw Blocks",
		meta = (ToolTip = "Computes the block and frontage-polygon geometry on demand and drapes it over the sampled ground: block boundary one colour, frontage polygon a second, and the side attributed to a street a third. The blocks come from the road graph; the frontage polygon is inset from each road axis by the widest half width promotion could widen that road to, and runs to the deepest plot the catalog and the morphology settings would give the next simulated year. Draws no plots, because a plot depends on the scores and the placement order of a year that has not run yet."))
	bool bDrawBlocks = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Attractors", DisplayName = "Draw Attractors",
		meta = (ToolTip = "Rings each attractor the freeze admitted at its influence radius, with a stem one sample spacing tall per unit of strength."))
	bool bDrawAttractors = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Labels", DisplayName = "Draw Labels")
	bool bDrawLabels = false;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "How many samples the site field sheet gets, which is what sets its resolution. The default leaves headroom under UE.DebugDraw.MaxTriangles; raising it much further drops primitives and logs a warning. 0 draws at Site Field Stride instead, unbudgeted."))
	int32 MaxDrawnSiteCells = 50000;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "How many samples the tensor field lattice gets, which is what sets its resolution. 0 draws at Tensor Field Stride instead, unbudgeted."))
	int32 MaxDrawnTensorSamples = 4000;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "How many triangles the whole road graph gets, as a hard cap. A road is a ground-hugging ribbon costing two triangles per sampled cell it crosses; past this the overlay stretches that spacing, and since a road never drapes below one station it then drops whole edges to stay under the cap and counts the drops in the header. 0 drapes at one station per sampled cell, unbudgeted. UE.DebugDraw.MaxTriangles sizes the triangle pool of each depth tier separately, so only the layers sharing this one's tier share its pool."))
	int32 MaxDrawnRoadTriangles = 40000;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "How many placed buildings the overlay draws, as a hard cap; the rest are dropped and counted in the header. A building costs twenty-four lines and twelve triangles: the filled box carries twelve edge hairlines of its own under UE.DebugDraw.SolidOutline, and the wireframe box over it adds twelve more as hairlines. Lines are the binding pool, so the default spends 144000 of the 262144 UE.DebugDraw.MaxLines gives each depth tier; overrun there drops whatever the overlay submits after the buildings. 0 draws every building, unbudgeted."))
	int32 MaxDrawnBuildings = 6000;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "How many lines the overlay draws for blocks, as a hard cap applied separately to the block boundaries and to the frontage polygons; the first polygon whose lines no longer fit stops that layer, and it and the rest are dropped and counted in the header. A polygon side is draped one line per sample spacing it covers, so an eight-sided block on a 2000 cm road step at the 200 cm default spacing costs eighty lines. Lines are the binding pool, so the default spends 40000 of the 262144 UE.DebugDraw.MaxLines gives each depth tier, alongside the 144000 the building default already spends there; overrun drops whatever the overlay submits after it. 0 draws every block and every frontage polygon, unbudgeted."))
	int32 MaxDrawnBlockLines = 20000;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "1",
		ToolTip = "How many per-element labels the overlay enqueues each frame, nearest to the camera first while a view is published. The header states the enqueued count against the total whenever it truncates. Fewer reach the screen: the DebugDraw label pool then culls by UE.DebugDraw.Text.MaxDistance, the view frustum and UE.DebugDraw.Label.MaxPerFrame."))
	int32 MaxDrawnLabels = 250;

	/** True when any layer is on. bDrawLabels is a modifier and enables nothing. */
	bool HasDebugOverlayEnabled() const
	{
		return bDrawVolumeBounds
			|| bDrawSiteGrid
			|| bDrawSiteField
			|| bDrawTensorField
			|| bDrawRoads
			|| bDrawBuildings
			|| bDrawBlocks
			|| bDrawAttractors;
	}

#if WITH_EDITORONLY_DATA
	/** Partition actors this town owns, keyed for World Partition pinning by ActorGuid. */
	UPROPERTY()
	TArray<FArcTownEmittedActor> EmittedActors;

	/**
	 * Placed-entity hosts this town owns, its own family keyed by a grid guid derived from TownId.
	 * Soft plus GUID: a hard UPROPERTY actor reference is an ActorDesc reference and would force the
	 * whole family always-loaded, and only the GUID can pin back a host that is streamed out.
	 */
	UPROPERTY()
	TArray<FArcTownEmittedActor> EmittedEntityHosts;
#endif

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	/**
	 * Serviced by ArcTownEditor, which owns the teardown; bound in FArcTownEditorModule::StartupModule so
	 * the dependency stays one-way. Returns the number of partition actors it destroyed. The volume is
	 * passed because TownId is not unique among loaded actors: two placements of one Level Instance
	 * share it, and a search by id could clear the ledger of the wrong one.
	 */
	DECLARE_DELEGATE_RetVal_FourParams(int32, FOnArcTownTeardownRequested, UWorld&, AArcTownVolume*, const FGuid&, const FBox&);
	static FOnArcTownTeardownRequested OnTeardownRequested;

	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
