// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainStampComponent.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainModule.h"
#include "ArcTerrainSubsystem.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#if WITH_EDITOR
#include "PrimitiveDrawingUtils.h"
#endif

UArcTerrainStampComponent::UArcTerrainStampComponent()
	: LastNotifiedBounds(ForceInit)
{
	PrimaryComponentTick.bCanEverTick = false;

	// Route EVERY world-transform change (editor gizmo drag, runtime SetActorLocation) through
	// NotifyStampChanged -> OnStampChanged(OldBounds, NewBounds) so the VACATED footprint is dirtied
	// and recomposites back to flat. PostEditComponentMove alone is insufficient: an actor-gizmo drag
	// can rerun construction scripts and recreate this component each delta, so the move-notify never
	// reaches the OnStampChanged branch and the old footprint stays refined until edit-pin eviction.
	// PropagateTransformUpdate fires OnUpdateTransform on the LIVE (still-registered) component with the
	// correct previous LastNotifiedBounds BEFORE any such recreation, so the old-dirty is never lost.
	bWantsOnUpdateTransform = true;

	// Pure data component: no scene proxy, no physics, no overlap. The visualizer (Step 8) draws it.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
}

void UArcTerrainStampComponent::Serialize(FArchive& Ar)
{
	const bool bDuplicateForPIE = Ar.HasAnyPortFlags(PPF_DuplicateForPIE);
	const double StartSeconds = bDuplicateForPIE ? FPlatformTime::Seconds() : 0.0;
	const int64 StartOffset = bDuplicateForPIE ? Ar.Tell() : INDEX_NONE;

	if (bDuplicateForPIE)
	{
		const FString TraceName = FString::Printf(TEXT("ArcTerrain PIE Duplicate %s %s"), *GetClass()->GetPathName(), *GetPathName());
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*TraceName);
		Super::Serialize(Ar);
	}
	else
	{
		Super::Serialize(Ar);
	}

	if (!bDuplicateForPIE)
	{
		return;
	}

	const double ElapsedMilliseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	if (ElapsedMilliseconds < 1.0)
	{
		return;
	}

	const int64 EndOffset = Ar.Tell();
	const bool bHasByteDelta = StartOffset != INDEX_NONE && EndOffset != INDEX_NONE;
	const int64 ByteDelta = bHasByteDelta ? EndOffset - StartOffset : 0;
	const TCHAR* Direction = Ar.IsSaving() ? TEXT("Save") : Ar.IsLoading() ? TEXT("Load") : TEXT("Unknown");
	UE_LOGF(LogArcTerrain, Log,"PIE duplicate Serialize Class=%ls Object=%ls Direction=%ls ElapsedMs=%.3f%ls",
		*GetClass()->GetPathName(), *GetPathName(), Direction, ElapsedMilliseconds,
		bHasByteDelta ? *FString::Printf(TEXT(" ByteDelta=%lld"), ByteDelta) : TEXT(""));
}

FBox UArcTerrainStampComponent::ComputeBounds() const
{
	const AArcTerrain* Terrain = AffectedTerrain.Get();
	if (Terrain == nullptr)
	{
		return FBox(ForceInit);
	}

	// Component transform relative to the terrain frame: stamp-local -> terrain-local.
	const FTransform StampToTerrain = GetComponentTransform().GetRelativeTransform(Terrain->GetTerrainTransform());

	const FVector2D Half = CoverageXY * 0.5;
	FBox FootprintBounds(ForceInit);
	FootprintBounds += StampToTerrain.TransformPosition(FVector(-Half.X, -Half.Y, 0.0));
	FootprintBounds += StampToTerrain.TransformPosition(FVector(Half.X, -Half.Y, 0.0));
	FootprintBounds += StampToTerrain.TransformPosition(FVector(-Half.X, Half.Y, 0.0));
	FootprintBounds += StampToTerrain.TransformPosition(FVector(Half.X, Half.Y, 0.0));
	return FootprintBounds;
}

void UArcTerrainStampComponent::NotifyStampChanged()
{
	if (!bRegisteredWithSubsystem)
	{
		// E.g. AffectedTerrain was assigned after OnRegister ran with a null terrain: registration
		// itself dirties the (initial) bounds, so nothing further to route.
		RegisterWithSubsystem();
		return;
	}

	const FBox NewBounds = ComputeBounds();
	if (UArcTerrainSubsystem* Subsystem = GetTerrainSubsystem())
	{
		Subsystem->OnStampChanged(*this, LastNotifiedBounds, NewBounds);
	}
	LastNotifiedBounds = NewBounds;
}

void UArcTerrainStampComponent::NotifyAffectedTerrainChanged()
{
	// Re-home: dirty the old terrain's cells, then register against the new terrain.
	UnregisterFromSubsystem();
	RegisterWithSubsystem();
}

TArray<FName> UArcTerrainStampComponent::GetTerrainChannels() const
{
	if (const AArcTerrain* Terrain = AffectedTerrain.Get())
	{
		return Terrain->GetTerrainChannels();
	}
	return TArray<FName>();
}

void UArcTerrainStampComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Editor-authoring fixup ONLY (never runtime/PIE, where the flag is already baked into the WP grid):
	// a spatially-loaded stamp-owner actor on a partitioned map would be streamed out by pivot/bounds,
	// so its stamp data would be absent for cells it still affects and the terrain would composite gaps.
	// Force the owner always-loaded (persisted via Modify(), baked at cook) so the stamp stays resident.
	// GetIsSpatiallyLoaded() returns false once flipped, so this fires at most once per owner actor.
	if (const UWorld* World = GetWorld())
	{
		if (World->WorldType == EWorldType::Editor && World->GetWorldPartition() != nullptr)
		{
			if (AActor* Owner = GetOwner())
			{
				if (Owner->GetIsSpatiallyLoaded() && Owner->CanChangeIsSpatiallyLoadedFlag())
				{
					Owner->Modify();
					Owner->SetIsSpatiallyLoaded(false);
					UE_LOGF(LogArcTerrain, Log,
						"ArcTerrain: forced stamp-owner actor '%ls' to always-loaded (was spatially "
						"loaded) on partitioned world '%ls' — stamp data must stay resident for every "
						"cell it affects; World Partition would otherwise silently stream it out.",
						*Owner->GetName(), *World->GetName());
				}
			}
		}
	}
#endif // WITH_EDITOR

	RegisterWithSubsystem();
}

void UArcTerrainStampComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

void UArcTerrainStampComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	// The world transform just changed (gizmo/SetActorLocation). At this point GetComponentTransform()
	// is the NEW pose while LastNotifiedBounds still holds the OLD footprint, so this dirties Old ∪ New.
	// Fires on the live registered component before any construction-script recreation, so the vacated
	// cells are always scheduled to recomposite (no reliance on arc.Terrain.EditPinSeconds eviction).
	NotifyStampChanged();
}

void UArcTerrainStampComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	UnregisterFromSubsystem();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

#if WITH_EDITOR
void UArcTerrainStampComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Interactive slider drags recomposite only on release (pattern: MeshPartition
	// UBooleanModifier::PostEditChangeProperty). Component MOVE drags stay live via
	// PostEditComponentMove — that is the < 100 ms acceptance path.
	if (PropertyChangedEvent.Property != nullptr
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		if (PropertyChangedEvent.Property->GetFName() ==
			GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, AffectedTerrain))
		{
			NotifyAffectedTerrainChanged();
		}
		else
		{
			NotifyStampChanged();
		}
	}
}

void UArcTerrainStampComponent::PostEditComponentMove(bool bFinished)
{
	Super::PostEditComponentMove(bFinished);

	// Always notify (also mid-drag): a cell recomposite is cheap and live drag is the point.
	NotifyStampChanged();
}

void UArcTerrainStampComponent::PostEditUndo()
{
	Super::PostEditUndo();

	// Undo can change anything, including AffectedTerrain and this component's live/dead state —
	// re-home conservatively.
	if (bRegisteredWithSubsystem)
	{
		UnregisterFromSubsystem();
	}
	if (IsRegistered() && IsValidChecked(this))
	{
		RegisterWithSubsystem();
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UArcTerrainStampComponent::DrawStampVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const
{
	const AArcTerrain* Terrain = AffectedTerrain.Get();
	if (Terrain == nullptr)
	{
		return;
	}

	// ComputeBounds() is terrain-local (axis-aligned in the terrain frame); the terrain transform maps
	// it back to world for drawing.
	const FBox TerrainLocalBounds = ComputeBounds();
	if (!TerrainLocalBounds.IsValid)
	{
		return;
	}
	const FMatrix TerrainToWorld = Terrain->GetTerrainTransform().ToMatrixWithScale();
	DrawWireBox(PDI, TerrainToWorld, TerrainLocalBounds, FLinearColor::Green, SDPG_World);

	// Inscribed circle in the stamp's own plane (component-local XY); auto-radius since there is no
	// explicit CircleRadius on the base.
	const FMatrix LocalToWorld = GetComponentTransform().ToMatrixWithScale();
	const FVector2D Half = CoverageXY * 0.5;
	const FVector Center = LocalToWorld.TransformPosition(FVector::ZeroVector);
	const FVector AxisX = LocalToWorld.GetScaledAxis(EAxis::X) * Half.X;
	const FVector AxisY = LocalToWorld.GetScaledAxis(EAxis::Y) * Half.Y;
	DrawCircle(PDI, Center, AxisX, AxisY, FLinearColor::Yellow, 1.0, 48, SDPG_World);
}
#endif // WITH_EDITOR

UArcTerrainSubsystem* UArcTerrainStampComponent::GetTerrainSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
}

void UArcTerrainStampComponent::RegisterWithSubsystem()
{
	if (bRegisteredWithSubsystem)
	{
		return;
	}

	AArcTerrain* Terrain = AffectedTerrain.Get();
	UArcTerrainSubsystem* Subsystem = GetTerrainSubsystem();
	if (Terrain == nullptr || Subsystem == nullptr)
	{
		return;
	}

	LastNotifiedBounds = ComputeBounds();
	Subsystem->RegisterStamp(*this, *Terrain);
	ResolvedTerrain = Terrain;
	bRegisteredWithSubsystem = true;
}

void UArcTerrainStampComponent::UnregisterFromSubsystem()
{
	if (!bRegisteredWithSubsystem)
	{
		return;
	}

	if (UArcTerrainSubsystem* Subsystem = GetTerrainSubsystem())
	{
		Subsystem->UnregisterStamp(*this, LastNotifiedBounds);
	}
	ResolvedTerrain = nullptr;
	bRegisteredWithSubsystem = false;
	LastNotifiedBounds = FBox(ForceInit);
}
