// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyEditor/Contributors/ArcSmartObjectSlotDrawContributor.h"
#include "EditorVisualization/ArcEditorEntityDrawComponent.h"
#include "Mass/ArcEconomyBuildingTrait.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/ArcAreaTrait.h"
#include "SmartObjectDefinition.h"
#include "ArcGameplayInteractionContext.h"
#include "StateTree.h"
#include "MassEntityConfigAsset.h"

namespace ArcSmartObjectSlotDrawContributor
{
	static const FColor WorkerSlotColor  = FColor::Green;
	static const FColor GathererSlotColor = FColor::Yellow;
	static const FColor OtherSlotColor   = FColor::Yellow;
	static const FColor EmptySlotColor   = FColor::Silver;

	static const float DefaultSlotCircleRadius = 40.f;
	static const float SlotCircleThickness     = 3.f;
	static const float SlotTextOffsetZ         = 60.f;
} // namespace ArcSmartObjectSlotDrawContributor

TSubclassOf<UMassEntityTraitBase> FArcSmartObjectSlotDrawContributor::GetTraitClass() const
{
	return UArcEconomyBuildingTrait::StaticClass();
}

void FArcSmartObjectSlotDrawContributor::CollectShapes(
	UArcEditorEntityDrawComponent& DrawComponent,
	const UMassEntityTraitBase* Trait,
	TConstArrayView<FTransform> Transforms,
	const FArcEditorDrawContext& Context)
{
	const UArcEconomyBuildingTrait* BuildingTrait = Cast<UArcEconomyBuildingTrait>(Trait);
	if (!BuildingTrait)
	{
		return;
	}

	// Find the UArcAreaTrait on the same config as this building trait.
	const UArcAreaTrait* AreaTrait = nullptr;
	for (const FArcEditorDrawContext::FPlacedEntityInfo& Entry : Context.AllPlacedEntities)
	{
		if (!Entry.Config)
		{
			continue;
		}
		const UMassEntityTraitBase* FoundBuilding = Entry.Config->FindTrait(UArcEconomyBuildingTrait::StaticClass());
		if (FoundBuilding == Trait)
		{
			AreaTrait = Cast<UArcAreaTrait>(Entry.Config->FindTrait(UArcAreaTrait::StaticClass()));
			break;
		}
	}

	if (!AreaTrait || !AreaTrait->SmartObjectDefinition)
	{
		return;
	}

	const USmartObjectDefinition* SODef = AreaTrait->SmartObjectDefinition.Get();
	const FArcBuildingEconomyConfig& EconomyConfig = BuildingTrait->EconomyConfig;

	TConstArrayView<FSmartObjectSlotDefinition> Slots = SODef->GetSlots();
	const int32 SlotCount = Slots.Num();

	for (const FTransform& InstanceTransform : Transforms)
	{
		// Show default behavior definition from the SmartObject asset (per building)
		// GetBehaviorDefinition with INDEX_NONE skips per-slot lookup and returns from DefaultBehaviorDefinitions
		{
			const FVector BuildingLocation = InstanceTransform.GetLocation();
			const USmartObjectBehaviorDefinition* DefaultDef = SODef->GetBehaviorDefinition(
				INDEX_NONE, UArcGameplayInteractionSmartObjectBehaviorDefinition::StaticClass());

			if (DefaultDef)
			{
				const FVector TextPos = BuildingLocation + FVector(0.f, 0.f, 150.f);
				const UArcGameplayInteractionSmartObjectBehaviorDefinition* InteractionDef =
					Cast<UArcGameplayInteractionSmartObjectBehaviorDefinition>(DefaultDef);
				if (InteractionDef)
				{
					const UStateTree* ST = InteractionDef->GetStateTree();
					if (ST)
					{
						DrawComponent.AddText(TextPos, FString::Printf(TEXT("Default ST: %s"), *ST->GetFName().ToString()), FColor::Cyan);
					}
					else
					{
						DrawComponent.AddText(TextPos, TEXT("Default: Interaction (no StateTree)"), FColor::Cyan);
					}
				}
				else
				{
					DrawComponent.AddText(TextPos, FString::Printf(TEXT("Default: %s"), *DefaultDef->GetFName().ToString()), FColor::Cyan);
				}
			}
		}

		for (int32 SlotIdx = 0; SlotIdx < SlotCount; ++SlotIdx)
		{
			const FSmartObjectSlotDefinition& SlotDef = Slots[SlotIdx];
			const FTransform SlotWorldTransform = SODef->GetSlotWorldTransform(SlotIdx, InstanceTransform);
			const FVector SlotLocation = SlotWorldTransform.GetLocation();

			FGameplayTagContainer SlotActivityTags;
			SODef->GetSlotActivityTags(SlotIdx, SlotActivityTags);

			FColor SlotColor;
			FString SlotLabel;

			if (SlotActivityTags.IsEmpty())
			{
				SlotColor = ArcSmartObjectSlotDrawContributor::EmptySlotColor;
				SlotLabel = FString::Printf(TEXT("Slot %d"), SlotIdx);
			}
			else if (!EconomyConfig.WorkerActivityTags.IsEmpty()
				&& SlotActivityTags.HasAny(EconomyConfig.WorkerActivityTags))
			{
				SlotColor = ArcSmartObjectSlotDrawContributor::WorkerSlotColor;
				SlotLabel = FString::Printf(TEXT("Worker %d"), SlotIdx);
			}
			else if (!EconomyConfig.GathererActivityTags.IsEmpty()
				&& SlotActivityTags.HasAny(EconomyConfig.GathererActivityTags))
			{
				SlotColor = ArcSmartObjectSlotDrawContributor::GathererSlotColor;
				SlotLabel = FString::Printf(TEXT("Gatherer %d"), SlotIdx);
			}
			else
			{
				SlotColor = ArcSmartObjectSlotDrawContributor::OtherSlotColor;
				SlotLabel = FString::Printf(TEXT("Slot %d"), SlotIdx);
			}

#if WITH_EDITORONLY_DATA
			const float SlotRadius = SlotDef.DEBUG_DrawSize;
			if (!SlotDef.Name.IsNone())
			{
				SlotLabel = FString::Printf(TEXT("%s (%d)"), *SlotDef.Name.ToString(), SlotIdx);
			}
#else
			const float SlotRadius = ArcSmartObjectSlotDrawContributor::DefaultSlotCircleRadius;
#endif

			DrawComponent.AddCircle(
				SlotLocation,
				SlotRadius,
				FVector::YAxisVector,
				FVector::XAxisVector,
				SlotColor,
				ArcSmartObjectSlotDrawContributor::SlotCircleThickness);

			// Build text lines for this slot
			int32 LineIndex = 0;
			const auto AddSlotText = [&](const FString& Text, const FColor& Color)
			{
				const FVector Pos = SlotLocation + FVector(0.f, 0.f, ArcSmartObjectSlotDrawContributor::SlotTextOffsetZ + LineIndex * 25.f);
				DrawComponent.AddText(Pos, Text, Color);
				++LineIndex;
			};

			AddSlotText(SlotLabel, SlotColor);

			// Activity tags
			if (!SlotDef.ActivityTags.IsEmpty())
			{
				AddSlotText(FString::Printf(TEXT("Activity: %s"), *SlotDef.ActivityTags.ToStringSimple()), SlotColor);
			}

			// Runtime tags
			if (!SlotDef.RuntimeTags.IsEmpty())
			{
				AddSlotText(FString::Printf(TEXT("Runtime: %s"), *SlotDef.RuntimeTags.ToStringSimple()), SlotColor);
			}

			// Behavior definitions
			for (int32 BehaviorIdx = 0; BehaviorIdx < SlotDef.BehaviorDefinitions.Num(); ++BehaviorIdx)
			{
				USmartObjectBehaviorDefinition* BehaviorDef = SlotDef.BehaviorDefinitions[BehaviorIdx];
				if (!BehaviorDef)
				{
					continue;
				}

				const UArcGameplayInteractionSmartObjectBehaviorDefinition* InteractionDef =
					Cast<UArcGameplayInteractionSmartObjectBehaviorDefinition>(BehaviorDef);
				if (InteractionDef)
				{
					const UStateTree* ST = InteractionDef->GetStateTree();
					if (ST)
					{
						AddSlotText(FString::Printf(TEXT("ST: %s"), *ST->GetFName().ToString()), SlotColor);
					}
					else
					{
						AddSlotText(TEXT("Behavior: Interaction (no StateTree)"), SlotColor);
					}
				}
				else
				{
					AddSlotText(FString::Printf(TEXT("Behavior: %s"), *BehaviorDef->GetFName().ToString()), SlotColor);
				}
			}
		}
	}
}
