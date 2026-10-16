// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Mass/EntityHandle.h"
#include "ArcTargetingPipelineTypes.h"
#include "GameplayTagContainer.h"

#include "ArcBuilderSubsystem.generated.h"

class UArcBuildingDefinition;
class UArcTargetingPreset;
class AActor;
class UWorld;
class APlayerController;
class APawn;

/** Broadcast whenever the builder tool/placement/requirement state changes (UI refresh rail). */
DECLARE_MULTICAST_DELEGATE(FArcOnBuilderStateChanged);

/** Grid snapping mode applied to placement when no authored SnappingMethod handles the hit. */
UENUM()
enum class EArcBuilderGridMode : uint8
{
	/** No grid: raw impact point, identity rotation. */
	Off,
	/** Snap world X/Y to the grid size; Z stays at the hit height. */
	Global,
	/** Snap all three axes in the frame of the first object hit after placement starts (anchored once). */
	Relative
};

/** Ephemeral tool context registered by the active builder tool; cleared when the tool ends. */
USTRUCT()
struct ARCBUILDER_API FArcBuilderToolContext
{
	GENERATED_BODY()

	/** Resolved buildings offered by the tool (explicit list ∪ tag-filtered primary assets). */
	UPROPERTY()
	TArray<TObjectPtr<UArcBuildingDefinition>> AvailableBuildings;

	/** Tool's building filter tags, kept for consumers that want to re-derive/inspect. */
	UPROPERTY()
	FGameplayTagQuery BuildingFilter;
};

/**
 * Local-player-owned building placement machine.
 *
 * Replaces the old actor-component UArcBuilderComponent. The active builder tool
 * registers its resolved context (RegisterTool) as an ephemeral cache — the UI menu
 * lists the cached buildings (GetAvailableBuildings) — and unregisters when it ends.
 * Registration never gates placement: StartPlacement / CancelPlacement /
 * ConfirmPlacement drive the placement ghost regardless of tool state.
 *
 * Placement tracing runs on ArcTargeting: a continuous StartAsync(bRequeue) request
 * whose completion snaps the ghost (authored SnappingMethod first, then the grid mode
 * fallback — see EArcBuilderGridMode), evaluates the building's requirements + resource
 * availability against the local (client-replicated) Mass state, and swaps the ghost
 * material. ConfirmPlacement sends the server-authoritative FArcPlaceBuildingCommand;
 * the client never spawns the building itself.
 *
 * Lifetime: this subsystem outlives world travel, but the ArcTargeting subsystem is a
 * world subsystem. The active UWorld is cached (RegisterTool / StartPlacement) and
 * OnWorldBeginTearDown cancels placement and unregisters the tool before the
 * targeting subsystem dies.
 */
UCLASS()
class ARCBUILDER_API UArcBuilderSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Tool registration (ephemeral cache; never gates placement) ----

	/** Register the active builder tool's resolved context, replacing any prior registration. */
	void RegisterTool(FArcBuilderToolContext&& Context);

	/** Cancel any active placement (cleanup), clear the cached tool context. */
	void UnregisterTool();

	/** True while a builder tool context is registered. */
	bool IsToolRegistered() const { return bToolRegistered; }

	/** Buildings resolved at registration time (cached — no loads). Empty when no tool is registered. */
	const TArray<TObjectPtr<UArcBuildingDefinition>>& GetAvailableBuildings() const { return ToolContext.AvailableBuildings; }

	// ---- Placement ----

	/** Begin placing Def: spawn the ghost, start the ArcTargeting placement loop. */
	void StartPlacement(UArcBuildingDefinition* Def);

	/** Stop placing: cancel the targeting request, destroy the ghost. */
	void CancelPlacement();

	/** Send the server place command for the current (met) placement. Client does not spawn. */
	void ConfirmPlacement();

	/** True while a ghost is live for a building definition. */
	bool IsPlacing() const;

	/** True when the last placement evaluation met all requirements + resources. */
	bool DidMeetPlacementRequirements() const { return bDidMeetPlaceRequirements; }

	// ---- Grid ----

	/** Set the grid snapping mode. Resets the relative-grid anchor latch. */
	void SetGridMode(EArcBuilderGridMode Mode);
	EArcBuilderGridMode GetGridMode() const { return GridMode; }

	void SetGridSize(int32 NewGridSize) { CurrentGridSize = NewGridSize; }
	int32 GetGridSize() const { return CurrentGridSize; }

	/** Fires on tool register/unregister, placement start/cancel/confirm, and requirement-met changes. */
	FArcOnBuilderStateChanged OnBuilderStateChanged;

private:
	/** ArcTargeting completion — snap the ghost, evaluate requirements/resources, swap material. */
	void HandleTargetingCompleted(FArcTargetingHandle InHandle);

	/** Cancel placement + unregister the tool when the torn-down world is our cached active world. */
	void OnWorldBeginTearDown(UWorld* InWorld);

	void DestroyGhost();
	void ApplyGhostMaterial(bool bMet);

	UWorld* GetOwnerWorld() const;
	APlayerController* GetOwnerPlayerController() const;

	/** Live placement ghost actor (UPROPERTY to keep it alive). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> GhostActor;

	/** Cached tool context; the UPROPERTY keeps the resolved definitions GC-visible. */
	UPROPERTY(Transient)
	FArcBuilderToolContext ToolContext;

	/** World the tool/placement is active in; matched by OnWorldBeginTearDown. */
	TWeakObjectPtr<UWorld> ActiveWorld;
	TWeakObjectPtr<UArcBuildingDefinition> CurrentBuildingDef;

	FArcTargetingHandle TargetingHandle;
	FDelegateHandle WorldTearDownHandle;

	/** Instigator Mass entity (placing pawn), resolved at StartPlacement. */
	FMassEntityHandle PawnEntity;

	FTransform FinalPlacementTransform = FTransform::Identity;

	/** Relative-grid anchor frame, latched once per placement (see bRelativeGridAnchored). */
	FVector RelativeGridOrigin = FVector::ZeroVector;
	FQuat RelativeGridRotation = FQuat::Identity;
	int32 CurrentGridSize = 100;

	EArcBuilderGridMode GridMode = EArcBuilderGridMode::Off;

	/** One-shot latch: set on the first targeting completion after StartPlacement/CancelPlacement/SetGridMode. */
	bool bRelativeGridAnchored = false;

	bool bDidMeetPlaceRequirements = false;
	bool bToolRegistered = false;
};
