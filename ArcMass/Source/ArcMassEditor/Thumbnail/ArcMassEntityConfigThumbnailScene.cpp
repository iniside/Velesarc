// Copyright Lukasz Baran. All Rights Reserved.

#include "Thumbnail/ArcMassEntityConfigThumbnailScene.h"

#include "EditorVisualization/ArcMassEditorVisualizationRegistry.h"
#include "Preview/ArcMassPreviewContributorInterface.h"

#include "PreviewScene.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

#include "MassEntityConfigAsset.h"

FArcMassEntityConfigThumbnailScene::FArcMassEntityConfigThumbnailScene()
	: FThumbnailPreviewScene()
{
	bForceAllUsedMipsResident = false;

	FArcMassEditorVisualizationRegistry::Get().CreatePreviewContributors(EArcPreviewContext::Thumbnail, Contributors);
}

bool FArcMassEntityConfigThumbnailScene::HasRenderableVisualization(const UMassEntityConfigAsset* Config)
{
	if (!Config)
	{
		return false;
	}

	TArray<TUniquePtr<IArcMassPreviewContributor>> ProbeContributors;
	FArcMassEditorVisualizationRegistry::Get().CreatePreviewContributors(EArcPreviewContext::Thumbnail, ProbeContributors);

	TArray<FMassEditableTraitInstance> Traits = Config->GetConfig().GetCombinedEditableTraits();
	for (const FMassEditableTraitInstance& TraitInstance : Traits)
	{
		if (TraitInstance.Trait == nullptr)
		{
			continue;
		}

		for (const TUniquePtr<IArcMassPreviewContributor>& Contributor : ProbeContributors)
		{
			if (Contributor->CanContribute(TraitInstance.Trait))
			{
				return true;
			}
		}
	}

	return false;
}

void FArcMassEntityConfigThumbnailScene::SetConfig(const UMassEntityConfigAsset* Config)
{
	Clear();

	if (!Config)
	{
		return;
	}

	UWorld* SpawnWorld = GetWorld();
	if (!SpawnWorld)
	{
		return;
	}

	// Owner actor for the contributor-added components. Never moved: contributors
	// add components standalone at their trait transforms, so recentering the actor
	// would not reposition the geometry.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(SpawnWorld->GetCurrentLevel(), AActor::StaticClass(), TEXT("ArcMassThumbnailActor"));
	SpawnParams.ObjectFlags = RF_Transient;
	SpawnParams.bNoFail = true;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* SpawnedActor = SpawnWorld->SpawnActor<AActor>(SpawnParams);
	PreviewActor = SpawnedActor;

	ArcMassPreview::ApplyConfigToContributors(*Config, Contributors, *this, SpawnedActor);

	FBox Combined(ForceInit);
	for (const TUniquePtr<IArcMassPreviewContributor>& Contributor : Contributors)
	{
		FBox ContributorBox = Contributor->GetBounds();
		if (ContributorBox.IsValid)
		{
			Combined += ContributorBox;
		}
	}

	if (Combined.IsValid)
	{
		CachedBounds = FBoxSphereBounds(Combined);
	}
}

void FArcMassEntityConfigThumbnailScene::Clear()
{
	for (TUniquePtr<IArcMassPreviewContributor>& Contributor : Contributors)
	{
		Contributor->Clear(*this);
	}

	if (PreviewActor.IsValid())
	{
		PreviewActor->Destroy();
	}
	PreviewActor.Reset();

	CachedBounds = FBoxSphereBounds(ForceInit);
}

void FArcMassEntityConfigThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	const float HalfFOVRadians = FMath::DegreesToRadians(InFOVDegrees) * 0.5f;

	// Empty/degenerate config (failed load, no mesh traits): stable, non-black frame.
	if (CachedBounds.SphereRadius <= KINDA_SMALL_NUMBER)
	{
		OutOrigin = FVector::ZeroVector;
		OutOrbitPitch = -11.25f;
		OutOrbitYaw = 0.0f;
		OutOrbitZoom = 200.0f;
		return;
	}

	const float HalfMeshSize = CachedBounds.SphereRadius * 1.15f;
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	// NOTE: deliberate deviation from the engine thumbnail templates
	// (FStaticMeshThumbnailScene / FClassActorThumbnailScene), which use
	// OutOrigin = -FVector(0, 0, GetBoundsZOffset(Bounds)). That literal is only
	// correct when the preview actor is recentered so its centroid.XY == 0. Here
	// the contributors add components standalone at their trait transforms
	// (AddComponent with bAttachToRoot=false) and we never recenter the actor, so
	// the geometry sits at CachedBounds.Origin. Pointing the camera at the world
	// origin would push the meshes off-frame (or fully out of view -> black
	// thumbnail). We aim at the real centroid instead. Do NOT "fix" this back to
	// the (0,0,-ZOffset) literal.
	OutOrigin = -CachedBounds.Origin;

	const USceneThumbnailInfo* Info = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	OutOrbitPitch = Info->OrbitPitch;
	OutOrbitYaw = Info->OrbitYaw;
	OutOrbitZoom = TargetDistance + Info->OrbitZoom;
}
