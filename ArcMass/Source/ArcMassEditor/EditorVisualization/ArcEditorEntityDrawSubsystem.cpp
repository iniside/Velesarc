// Copyright Lukasz Baran. All Rights Reserved.

#include "EditorVisualization/ArcEditorEntityDrawSubsystem.h"
#include "EditorVisualization/ArcEditorEntityDrawComponent.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "MassEntityConfigAsset.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"

void UArcEditorEntityDrawSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcEditorEntityDrawSubsystem::Deinitialize()
{
	Deactivate();
	Contributors.Empty();
	Super::Deinitialize();
}

void UArcEditorEntityDrawSubsystem::RegisterContributor(TSharedPtr<IArcEditorEntityDrawContributor> Contributor)
{
	if (!Contributor)
	{
		return;
	}

	Contributors.Add(Contributor);

	const FName Category = Contributor->GetCategory();
	if (!CategoryToggleState.Contains(Category))
	{
		CategoryToggleState.Add(Category, Contributor->IsEnabledByDefault());
	}

	if (bActive)
	{
		RefreshShapes();
	}
}

void UArcEditorEntityDrawSubsystem::UnregisterContributor(TSharedPtr<IArcEditorEntityDrawContributor> Contributor)
{
	Contributors.Remove(Contributor);

	if (bActive)
	{
		RefreshShapes();
	}
}

void UArcEditorEntityDrawSubsystem::SetCategoryEnabled(FName Category, bool bEnabled)
{
	CategoryToggleState.FindOrAdd(Category) = bEnabled;

	if (bActive)
	{
		RefreshShapes();
	}
}

bool UArcEditorEntityDrawSubsystem::IsCategoryEnabled(FName Category) const
{
	const bool* bEnabled = CategoryToggleState.Find(Category);
	return bEnabled ? *bEnabled : true;
}

TArray<FName> UArcEditorEntityDrawSubsystem::GetAllCategories() const
{
	TArray<FName> Categories;
	CategoryToggleState.GetKeys(Categories);
	return Categories;
}

void UArcEditorEntityDrawSubsystem::Activate(UWorld* World)
{
	if (bActive)
	{
		return;
	}

	ActiveWorld = World;
	bActive = true;
	SpawnDrawActor();
	RefreshShapes();
}

void UArcEditorEntityDrawSubsystem::Deactivate()
{
	if (!bActive)
	{
		return;
	}

	bActive = false;
	DestroyDrawActor();
	ActiveWorld = nullptr;
}

void UArcEditorEntityDrawSubsystem::SpawnDrawActor()
{
	if (!ActiveWorld)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags = RF_Transient;
	DrawActor = ActiveWorld->SpawnActor<AActor>(Params);
	if (DrawActor)
	{
#if WITH_EDITOR
		DrawActor->SetActorLabel(TEXT("EditorEntityDraw"));
#endif
		USceneComponent* Root = NewObject<USceneComponent>(DrawActor, TEXT("Root"));
		Root->RegisterComponent();
		DrawActor->SetRootComponent(Root);

		DrawComponent = NewObject<UArcEditorEntityDrawComponent>(DrawActor);
		DrawComponent->SetupAttachment(Root);
		DrawComponent->RegisterComponent();
		DrawActor->AddInstanceComponent(DrawComponent);
	}
}

void UArcEditorEntityDrawSubsystem::DestroyDrawActor()
{
	if (DrawActor)
	{
		DrawActor->Destroy();
		DrawActor = nullptr;
		DrawComponent = nullptr;
	}
}

void UArcEditorEntityDrawSubsystem::BuildContext(FArcEditorDrawContext& OutContext) const
{
	OutContext.World = ActiveWorld;
	OutContext.AllPlacedEntities.Reset();

	if (!ActiveWorld)
	{
		return;
	}

	for (TActorIterator<AArcPlacedEntityPartitionActor> It(ActiveWorld); It; ++It)
	{
		AArcPlacedEntityPartitionActor* PartitionActor = *It;
		for (const FArcPlacedEntityEntry& Entry : PartitionActor->Entries)
		{
			if (!Entry.EntityConfig || Entry.InstanceTransforms.IsEmpty())
			{
				continue;
			}

			FArcEditorDrawContext::FPlacedEntityInfo& Info = OutContext.AllPlacedEntities.AddDefaulted_GetRef();
			Info.Config = Entry.EntityConfig;

			const FTransform ActorTransform = PartitionActor->GetActorTransform();
			Info.Transforms.Reserve(Entry.InstanceTransforms.Num());
			for (const FTransform& LocalTransform : Entry.InstanceTransforms)
			{
				Info.Transforms.Add(LocalTransform * ActorTransform);
			}
		}
	}
}

void UArcEditorEntityDrawSubsystem::RefreshShapes()
{
	if (!bActive || !DrawComponent)
	{
		return;
	}

	DrawComponent->ClearShapes();

	FArcEditorDrawContext Context;
	BuildContext(Context);

	for (const TSharedPtr<IArcEditorEntityDrawContributor>& Contributor : Contributors)
	{
		const FName Category = Contributor->GetCategory();
		if (!IsCategoryEnabled(Category))
		{
			continue;
		}

		const TSubclassOf<UMassEntityTraitBase> TraitClass = Contributor->GetTraitClass();

		if (!TraitClass)
		{
			Contributor->CollectShapes(*DrawComponent, nullptr, {}, Context);
			continue;
		}

		for (const FArcEditorDrawContext::FPlacedEntityInfo& Info : Context.AllPlacedEntities)
		{
			const UMassEntityTraitBase* Trait = Info.Config->FindTrait(TraitClass);
			if (Trait)
			{
				Contributor->CollectShapes(*DrawComponent, Trait, Info.Transforms, Context);
			}
		}
	}

	DrawComponent->FinishCollecting();
}
