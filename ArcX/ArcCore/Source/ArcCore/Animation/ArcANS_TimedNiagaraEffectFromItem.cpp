/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */



#include "ArcANS_TimedNiagaraEffectFromItem.h"

#include "ArcCoreUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_NiagaraSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Player/ArcCoreCharacter.h"

UArcANS_TimedNiagaraEffectFromItem::UArcANS_TimedNiagaraEffectFromItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	PreviewTemplate = nullptr;
#endif
	
	LocationOffset.Set(0.0f, 0.0f, 0.0f);
	RotationOffset = FRotator(0.0f, 0.0f, 0.0f);
}

UFXSystemComponent* UArcANS_TimedNiagaraEffectFromItem::SpawnEffect(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const
{
	// Only spawn if we've got valid params
#if WITH_EDITOR
	if (MeshComp && PreviewTemplate)
	{
		UWorld* World = MeshComp->GetWorld();
		if (World && (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview))
		{
			FFXSystemSpawnParameters SpawnParams;
			SpawnParams.SystemTemplate		= PreviewTemplate;
			SpawnParams.AttachToComponent	= MeshComp;
			SpawnParams.AttachPointName		= SocketName;
			SpawnParams.Location			= LocationOffset;
			SpawnParams.Rotation			= RotationOffset;
			SpawnParams.Scale				= Scale;
			SpawnParams.LocationType		= LocationType;
			SpawnParams.bAutoDestroy		= !bDestroyAtEnd;

			if (UNiagaraComponent* NewComponent = UNiagaraFunctionLibrary::SpawnSystemAttachedWithParams(SpawnParams))
			{
				if (bApplyRateScaleAsTimeDilation)
				{
					NewComponent->SetCustomTimeDilation(Animation->RateScale);
				}
		
				FName NotifyName = GetFName();
				NotifyName.SetNumber(GetUniqueID());

				NewComponent->ComponentTags.AddUnique(NotifyName);
				return NewComponent;
			}
			return nullptr;
		}
	}
#endif
	
	AArcCoreCharacter* ArcChar =  Cast<AArcCoreCharacter>(MeshComp->GetOwner());
	if (!ArcChar)
	{
		return nullptr;
	}

	UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation);
	if (!AnimMontage)
	{
		return nullptr;
	}
	
	UArcCoreAbilitySystemComponent* ArcASC = ArcChar->GetArcAbilitySystemComponent();	
	const FArcItemData* ItemData = ArcASC->GetItemFromMontage(AnimMontage);
	if (!ItemData)
	{
		return nullptr;
	}

	UNiagaraSystem* System = nullptr;
	FName SocketFromFragment = NAME_None;
	if (NiagaraSystemTag.IsValid())
	{
		const FArcItemFragment_NiagaraSystemMap* Fragment = ArcItems::FindFragment<FArcItemFragment_NiagaraSystemMap>(ItemData);
		if (!Fragment)
		{
			return nullptr;
		}

		if (const TSoftObjectPtr<UNiagaraSystem>* SystemPtr = Fragment->Systems.Find(NiagaraSystemTag))
		{
			System = SystemPtr->Get();	
		}
	}
	else
	{
		const FArcItemFragment_NiagaraSystem* Fragment = ArcItems::FindFragment<FArcItemFragment_NiagaraSystem>(ItemData);
		if (!Fragment)
		{
			return nullptr;
		}

		System = Fragment->System.Get();
		SocketFromFragment = Fragment->SocketName;
	}

	FFXSystemSpawnParameters SpawnParams;
	SpawnParams.SystemTemplate		= System;
	SpawnParams.AttachToComponent	= MeshComp;
	SpawnParams.AttachPointName		= SocketName;
	SpawnParams.Location			= LocationOffset;
	SpawnParams.Rotation			= RotationOffset;
	SpawnParams.Scale				= Scale;
	SpawnParams.LocationType		= LocationType;
	SpawnParams.bAutoDestroy		= !bDestroyAtEnd;

	if (!SocketFromFragment.IsNone())
	{
		SpawnParams.AttachPointName = SocketFromFragment;
	}
	
	if (ItemSlotId.IsValid() && ArcChar->GetPlayerState())
	{
		UArcItemAttachmentComponent* AttachmentComponent = Arcx::Utils::GetComponent<UArcItemAttachmentComponent>(ArcChar->GetPlayerState(), UArcItemAttachmentComponent::StaticClass());
		if (AttachmentComponent)
		{
			USceneComponent* Attachment = AttachmentComponent->FindFirstAttachedObject<USceneComponent>(ItemData->GetItemDefinition());
			if (Attachment)
			{
				SpawnParams.AttachToComponent = Attachment;
			}
		}
	}
	
	if (UNiagaraComponent* NewComponent = UNiagaraFunctionLibrary::SpawnSystemAttachedWithParams(SpawnParams))
	{
		if (bApplyRateScaleAsTimeDilation)
		{
			NewComponent->SetCustomTimeDilation(Animation->RateScale);
		}
		
		FName NotifyName = GetFName();
		NotifyName.SetNumber(GetUniqueID());

		NewComponent->ComponentTags.AddUnique(NotifyName);

		return NewComponent;
	}

	return nullptr;
}

UFXSystemComponent* UArcANS_TimedNiagaraEffectFromItem::GetSpawnedEffect(UMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (MeshComp)
	{
		if (ItemSlotId.IsValid())
		{
			AArcCoreCharacter* ArcChar =  Cast<AArcCoreCharacter>(MeshComp->GetOwner());
			if (!ArcChar)
			{
				return nullptr;
			}

			UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation);
			if (!AnimMontage)
			{
				return nullptr;
			}
	
			UArcCoreAbilitySystemComponent* ArcASC = ArcChar->GetArcAbilitySystemComponent();	
			const FArcItemData* ItemData = ArcASC->GetItemFromMontage(AnimMontage);
			if (!ItemData)
			{
				return nullptr;
			}

			UArcItemAttachmentComponent* AttachmentComponent = Arcx::Utils::GetComponent<UArcItemAttachmentComponent>(ArcChar->GetPlayerState(), UArcItemAttachmentComponent::StaticClass());
			if (AttachmentComponent)
			{
				USceneComponent* Attachment = AttachmentComponent->FindFirstAttachedObject<USceneComponent>(ItemData->GetItemDefinition());
				if (Attachment)
				{
					TArray<USceneComponent*> Children;
					Attachment->GetChildrenComponents(false, Children);
					for (USceneComponent* Component : Children)
					{
						if (Component && Component->ComponentHasTag(GetSpawnedComponentTag()))
						{
							if (UFXSystemComponent* FXComponent = CastChecked<UFXSystemComponent>(Component))
							{
								return FXComponent;
							}
						}
					}
				}
			}
		}
		else
		{
			TArray<USceneComponent*> Children;
			MeshComp->GetChildrenComponents(false, Children);
			if (Children.Num())
			{
				for (USceneComponent* Component : Children)
				{
					if (Component && Component->ComponentHasTag(GetSpawnedComponentTag()))
					{
						if (UFXSystemComponent* FXComponent = CastChecked<UFXSystemComponent>(Component))
						{
							return FXComponent;
						}
					}
				}
			}	
		}
	}

	return nullptr;
}

void UArcANS_TimedNiagaraEffectFromItem::NotifyBegin(USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (UFXSystemComponent* Component = SpawnEffect(MeshComp, Animation))
	{
		// tag the component with the AnimNotify that is triggering the animation so that we can properly clean it up
		Component->ComponentTags.AddUnique(GetSpawnedComponentTag());
	}

	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
}

void UArcANS_TimedNiagaraEffectFromItem::NotifyEnd(USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UFXSystemComponent* FXComponent = GetSpawnedEffect(MeshComp, Animation))
	{
		// untag the component
		FXComponent->ComponentTags.Remove(GetSpawnedComponentTag());

		// Either destroy the component or deactivate it to have it's active FXSystems finish.
		// The component will auto destroy once all FXSystem are gone.
		if (bDestroyAtEnd)
		{
			FXComponent->DestroyComponent();
		}
		else
		{
			FXComponent->Deactivate();
		}
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

bool UArcANS_TimedNiagaraEffectFromItem::ValidateParameters(USkeletalMeshComponent* MeshComp) const
{
	bool bValid = true;

	//if (!Template)
	//{
	//	bValid = false;
	//}
	//else if (!MeshComp->DoesSocketExist(SocketName) && MeshComp->GetBoneIndex(SocketName) == INDEX_NONE)
	//{
	//	bValid = false;
	//}

	return bValid;
}

FName UArcANS_TimedNiagaraEffectFromItem::GetSpawnedComponentTag() const
{
	// we generate a unique tag to associate with our spawned components so that we can clean things up upon completion
	FName NotifyName = GetFName();
	NotifyName.SetNumber(GetUniqueID());

	return NotifyName;
}

FString UArcANS_TimedNiagaraEffectFromItem::GetNotifyName_Implementation() const
{
	//if (Template)
	//{
	//	return Template->GetName();
	//}

	return UAnimNotifyState::GetNotifyName_Implementation();
}