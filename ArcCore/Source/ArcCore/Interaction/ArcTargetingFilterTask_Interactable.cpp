// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetingFilterTask_Interactable.h"

#include "InteractableTargetInterface.h"
#include "Components/PrimitiveComponent.h"
#include "ArcMass/ArcMassEntityLibrary.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "ArcInteractionDisplayData.h"
#include "MassEntitySubsystem.h"
#include "SmartObjectDefinition.h"
#include "Engine/Engine.h"

bool UArcTargetingFilterTask_Interactable::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const
{
	if (AActor* Actor = TargetData.HitResult.GetActor())
	{
		if (Actor->Implements<UInteractionTarget>())
		{
			return false;
		}

		if (UPrimitiveComponent* HitComp = TargetData.HitResult.GetComponent())
		{
			if (HitComp->Implements<UInteractionTarget>())
			{
				return false;
			}
		}

		// Search actor's components for any IInteractionTarget implementor
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->Implements<UInteractionTarget>())
			{
				return false;
			}
		}
	}

	// Fallback: check if the hit resolves to a Mass entity with a SmartObject definition
	EArcMassResult MassResult = EArcMassResult::NotValid;
	FMassEntityHandle EntityHandle = UArcMassEntityLibrary::ResolveHitToEntity(TargetData.HitResult, MassResult);
	if (MassResult == EArcMassResult::Valid && EntityHandle.IsValid())
	{
		UWorld* World = GEngine->GetCurrentPlayWorld();
		if (World)
		{
			UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
			if (EntitySubsystem)
			{
				const FMassEntityManager& EM = EntitySubsystem->GetEntityManager();
				const FArcSmartObjectDefinitionSharedFragment* SODefFragment = EM.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(EntityHandle);
				if (SODefFragment && SODefFragment->SmartObjectDefinition)
				{
					return false;
					// Check that at least one slot has interaction display data
					//for (const FSmartObjectSlotDefinition& Slot : SODefFragment->SmartObjectDefinition->GetSlots())
					//{
					//	if (Slot.GetDefinitionDataPtr<FArcInteractionDisplayData>() != nullptr)
					//	{
					//		return false;
					//	}
					//}
				}
			}
		}
	}

	return true;
}
