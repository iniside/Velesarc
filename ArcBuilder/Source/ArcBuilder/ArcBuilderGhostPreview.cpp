// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderGhostPreview.h"

#include "ArcBuilder.h"
#include "ArcBuildingDefinition.h"

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Visualization/ArcUnifiedMeshVisualizationTrait.h"
#include "MassVisualizationTrait.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

namespace ArcBuilderGhostPreview_Impl
{
	const UMassVisualizationTrait* FindEngineVisTrait(const UArcMassEntityConfigAsset& Config)
	{
		return Cast<UMassVisualizationTrait>(Config.FindTrait(UMassVisualizationTrait::StaticClass()));
	}

	const UArcUnifiedMeshVisualizationTrait* FindUnifiedTrait(const UArcMassEntityConfigAsset& Config)
	{
		return Cast<UArcUnifiedMeshVisualizationTrait>(Config.FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	}

	/** Create + attach + register a movable, collision-free static mesh component. Null mesh → nullptr. */
	UStaticMeshComponent* MakeStaticMeshComp(AActor& Ghost, USceneComponent& Root, UStaticMesh* Mesh, const FTransform& RelativeTransform)
	{
		if (Mesh == nullptr)
		{
			return nullptr;
		}

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(&Ghost);
		Comp->SetStaticMesh(Mesh);
		Comp->SetRelativeTransform(RelativeTransform);
		Comp->SetupAttachment(&Root);
		Comp->RegisterComponent();
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Comp;
	}

	/** Create + attach + register a movable, collision-free skeletal mesh component. Null asset → nullptr. */
	USkeletalMeshComponent* MakeSkeletalMeshComp(AActor& Ghost, USceneComponent& Root, USkinnedAsset* Asset, const FTransform& RelativeTransform)
	{
		if (Asset == nullptr)
		{
			return nullptr;
		}

		USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(&Ghost);
		Comp->SetSkinnedAssetAndUpdate(Asset);
		Comp->SetRelativeTransform(RelativeTransform);
		Comp->SetupAttachment(&Root);
		Comp->RegisterComponent();
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Comp;
	}

	void ApplyMaterials(UMeshComponent& Comp, const TArray<TObjectPtr<UMaterialInterface>>& Materials)
	{
		for (int32 Index = 0; Index < Materials.Num(); ++Index)
		{
			if (Materials[Index])
			{
				Comp.SetMaterial(Index, Materials[Index]);
			}
		}
	}

	void ApplyMaterialsSoft(UMeshComponent& Comp, const TArray<TSoftObjectPtr<UMaterialInterface>>& Materials)
	{
		for (int32 Index = 0; Index < Materials.Num(); ++Index)
		{
			UMaterialInterface* Mat = Materials[Index].LoadSynchronous();
			if (Mat)
			{
				Comp.SetMaterial(Index, Mat);
			}
		}
	}

	int32 AppendUnifiedTrait(AActor& Ghost, USceneComponent& Root, const UArcUnifiedMeshVisualizationTrait& Trait)
	{
		int32 Count = 0;
		const TArray<FArcUnifiedMeshEntry> MeshEntries = Trait.GetMeshEntries();
		for (const FArcUnifiedMeshEntry& Entry : MeshEntries)
		{
			if (!Entry.IsValidEntry())
			{
				continue;
			}

			if (Entry.IsSkinnedEntry())
			{
				USkeletalMeshComponent* Comp = MakeSkeletalMeshComp(Ghost, Root, Entry.SkinnedAsset.LoadSynchronous(), Entry.RelativeTransform);
				if (Comp == nullptr)
				{
					continue;
				}

				ApplyMaterialsSoft(*Comp, Entry.MaterialOverrides);
				++Count;
			}
			else
			{
				UStaticMeshComponent* Comp = MakeStaticMeshComp(Ghost, Root, Entry.StaticMesh.LoadSynchronous(), Entry.RelativeTransform);
				if (Comp == nullptr)
				{
					continue;
				}

				ApplyMaterialsSoft(*Comp, Entry.MaterialOverrides);
				++Count;
			}
		}
		return Count;
	}
}

namespace ArcBuilderGhostPreview
{
	AActor* SpawnGhostActor(UWorld& World, const UArcBuildingDefinition& Def, const FActorSpawnParameters& SpawnParams)
	{
		// Class-based path: explicit ActorClass, or the engine vis trait's HighResTemplateActor.
		if (UClass* PreviewClass = Def.ResolvePreviewActorClass())
		{
			return World.SpawnActor<AActor>(PreviewClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		}

		// Trait-built path: assemble a plain actor from the config's Arc visualization traits.
		const UArcMassEntityConfigAsset* Config = Def.MassEntityConfig;
		if (Config == nullptr)
		{
			UE_LOG(LogArcBuilder, Warning,
				TEXT("SpawnGhostActor: %s has no preview source — set ActorClass or a supported visualization trait."),
				*Def.GetName());
			return nullptr;
		}

		AActor* Ghost = World.SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (Ghost == nullptr)
		{
			return nullptr;
		}

		USceneComponent* Root = NewObject<USceneComponent>(Ghost);
		Ghost->SetRootComponent(Root);
		Root->RegisterComponent();

		int32 ComponentCount = 0;
		if (const UArcUnifiedMeshVisualizationTrait* UnifiedTrait = ArcBuilderGhostPreview_Impl::FindUnifiedTrait(*Config))
		{
			ComponentCount += ArcBuilderGhostPreview_Impl::AppendUnifiedTrait(*Ghost, *Root, *UnifiedTrait);
		}

		if (ComponentCount == 0)
		{
			UE_LOG(LogArcBuilder, Warning,
				TEXT("SpawnGhostActor: %s config %s carries no usable visualization trait — add a supported trait or set ActorClass."),
				*Def.GetName(), *Config->GetName());
			Ghost->Destroy();
			return nullptr;
		}

		return Ghost;
	}

	bool HasSupportedVisualization(const UArcMassEntityConfigAsset& Config)
	{
		if (const UMassVisualizationTrait* EngineTrait = ArcBuilderGhostPreview_Impl::FindEngineVisTrait(Config))
		{
			if (EngineTrait->HighResTemplateActor)
			{
				return true;
			}
		}

		if (const UArcUnifiedMeshVisualizationTrait* UnifiedTrait = ArcBuilderGhostPreview_Impl::FindUnifiedTrait(Config))
		{
			if (UnifiedTrait->IsValid())
			{
				return true;
			}
		}

		return false;
	}
}
