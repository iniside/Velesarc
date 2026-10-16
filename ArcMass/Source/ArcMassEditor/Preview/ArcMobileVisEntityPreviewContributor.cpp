// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcMobileVisEntityPreviewContributor.h"

#include "ArcMass/MobileVisualization/ArcMobileVisualizationTrait.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "PreviewScene.h"

namespace ArcMobileVisEntityPreviewContributor
{

bool IsRenderableActorClass(const UClass* ActorClass)
{
	return ActorClass != nullptr && !ActorClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated);
}

FBox GetVisibleActorBounds(const AActor& Actor)
{
	FBox Bounds(ForceInit);
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor.GetComponents(PrimitiveComponents);

	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent != nullptr && PrimitiveComponent->IsVisible() && !PrimitiveComponent->bHiddenInGame)
		{
			Bounds += PrimitiveComponent->Bounds.GetBox();
		}
	}

	return Bounds;
}

void ApplyMaterialOverrides(UPrimitiveComponent& Component, const TArray<TObjectPtr<UMaterialInterface>>& MaterialOverrides)
{
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialOverrides.Num(); ++MaterialIndex)
	{
		Component.SetMaterial(MaterialIndex, MaterialOverrides[MaterialIndex]);
	}
}

}

bool FArcMobileVisEntityPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcMobileVisEntityTrait* MobileVisTrait = Cast<UArcMobileVisEntityTrait>(Trait);
	if (MobileVisTrait == nullptr)
	{
		return false;
	}

	const FArcMobileVisConfigFragment& Config = MobileVisTrait->VisualizationConfig;
	return ArcMobileVisEntityPreviewContributor::IsRenderableActorClass(Config.ActorClass.Get())
		|| (Config.bUseSkinnedMesh && Config.SkinnedAsset != nullptr)
		|| Config.StaticMesh != nullptr;
}

void FArcMobileVisEntityPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig)
{
	Clear(Scene);
	OwningConfig = InOwningConfig;

	const UArcMobileVisEntityTrait* MobileVisTrait = CastChecked<UArcMobileVisEntityTrait>(Trait);
	const FArcMobileVisConfigFragment& Config = MobileVisTrait->VisualizationConfig;
	UClass* ActorClass = Config.ActorClass.Get();

	if (ArcMobileVisEntityPreviewContributor::IsRenderableActorClass(ActorClass))
	{
		UWorld* PreviewWorld = Scene.GetWorld();
		if (PreviewWorld == nullptr)
		{
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags = RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewRepresentationActor = PreviewWorld->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);
		if (PreviewRepresentationActor.IsValid()
			&& ArcMobileVisEntityPreviewContributor::GetVisibleActorBounds(*PreviewRepresentationActor).IsValid)
		{
			return;
		}

		if (PreviewRepresentationActor.IsValid())
		{
			PreviewRepresentationActor->Destroy();
		}
		PreviewRepresentationActor.Reset();
	}

	if (PreviewActor == nullptr)
	{
		return;
	}

	if (Config.bUseSkinnedMesh && Config.SkinnedAsset != nullptr)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(PreviewActor);
		SkeletalMeshComponent->SetSkinnedAssetAndUpdate(Config.SkinnedAsset);
		PreviewComponent = SkeletalMeshComponent;
	}
	else if (Config.StaticMesh != nullptr)
	{
		UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(PreviewActor);
		StaticMeshComponent->SetStaticMesh(Config.StaticMesh);
		PreviewComponent = StaticMeshComponent;
	}

	if (PreviewComponent != nullptr)
	{
		PreviewActor->AddOwnedComponent(PreviewComponent);
		Scene.AddComponent(PreviewComponent, FTransform::Identity);
		PreviewComponent->SetCastShadow(Config.bCastShadows);
		ArcMobileVisEntityPreviewContributor::ApplyMaterialOverrides(*PreviewComponent, Config.MaterialOverrides);
		PreviewComponent->MarkRenderStateDirty();
	}
}

void FArcMobileVisEntityPreviewContributor::Clear(FPreviewScene& Scene)
{
	if (PreviewComponent != nullptr)
	{
		Scene.RemoveComponent(PreviewComponent);
		PreviewComponent = nullptr;
	}

	if (PreviewRepresentationActor.IsValid())
	{
		PreviewRepresentationActor->Destroy();
	}
	PreviewRepresentationActor.Reset();
	OwningConfig = nullptr;
}

FBox FArcMobileVisEntityPreviewContributor::GetBounds() const
{
	if (PreviewRepresentationActor.IsValid())
	{
		return ArcMobileVisEntityPreviewContributor::GetVisibleActorBounds(*PreviewRepresentationActor);
	}

	return PreviewComponent != nullptr ? PreviewComponent->Bounds.GetBox() : FBox(ForceInit);
}
