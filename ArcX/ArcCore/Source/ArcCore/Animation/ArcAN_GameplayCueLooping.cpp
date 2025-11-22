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



#include "ArcAN_GameplayCueLooping.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueManager.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Items/ArcItemsHelpers.h"

namespace Arcx
{
	typedef void (*GameplayCueFunc)(AActor* Target, const FGameplayTag GameplayCueTag, const FGameplayCueParameters& Parameters);


	static void ProcessGameplayCue(GameplayCueFunc Func, USkeletalMeshComponent* MeshComp, FGameplayTag GameplayCue, UAnimSequenceBase* Animation)
	{
		if (MeshComp && GameplayCue.IsValid())
		{
			AActor* OwnerActor = MeshComp->GetOwner();

#if WITH_EDITOR
			if (GIsEditor && (OwnerActor == nullptr))
			{
				// Make preview work in anim preview window
				UGameplayCueManager::PreviewComponent = MeshComp;
				UGameplayCueManager::PreviewWorld = MeshComp->GetWorld();

				if (UGameplayCueManager* GCM = UAbilitySystemGlobals::Get().GetGameplayCueManager())
				{
					GCM->LoadNotifyForEditorPreview(GameplayCue);
				}
			}
#endif // #if WITH_EDITOR

			FGameplayCueParameters Parameters;
			Parameters.Instigator = OwnerActor;

			// Try to get the ability level. This may not be able to find the ability level
			// in cases where a blend out is happening or two abilities are trying to play animations
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor))
			{
				if (ASC->GetCurrentMontage() == Animation)
				{
					if (const UGameplayAbility* Ability = ASC->GetAnimatingAbility())
					{
						Parameters.AbilityLevel = Ability->GetAbilityLevel();
					}
				}

				// Always use ASC's owner for instigator
				Parameters.Instigator = ASC->GetOwner();
			}
			
			Parameters.SourceObject = MeshComp->GetOwner();
			Parameters.EffectCauser = MeshComp->GetOwner();
			Parameters.TargetAttachComponent = MeshComp;

			(*Func)(OwnerActor, GameplayCue, Parameters);
		}

#if WITH_EDITOR
		if (GIsEditor)
		{
			UGameplayCueManager::PreviewComponent = nullptr;
			UGameplayCueManager::PreviewWorld = nullptr;
		}
#endif // #if WITH_EDITOR
	}

}

void UArcAN_GameplayCueLoopingStart::Notify(USkeletalMeshComponent* MeshComp
	, UAnimSequenceBase* Animation
	, const FAnimNotifyEventReference& EventReference)
{
	FGameplayCueParameters Parameters;
	
	Parameters.SourceObject = MeshComp->GetOwner();
	Parameters.EffectCauser = MeshComp->GetOwner();
	
	
	Parameters.TargetAttachComponent = MeshComp;
	
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(MeshComp->GetOwner()))
	{
		if (UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			Parameters.Instigator = ASC->GetOwnerActor();
			
			UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
			
			if (const TWeakObjectPtr<const UArcItemDefinition>* ItemDef = ASC->MontageToItemDef.Find(Montage))
			{
				const FArcItemFragment_AnimMontageGameplayCue* Cue = ItemDef->Get()->FindFragment<FArcItemFragment_AnimMontageGameplayCue>();
				if (Cue)
				{
					UGameplayCueManager::AddGameplayCue_NonReplicated(MeshComp->GetOwner(), Cue->Cue.GameplayCueTag, Parameters);
					return;
				}
			}
		}
	}
	
	Arcx::ProcessGameplayCue(&UGameplayCueManager::AddGameplayCue_NonReplicated, MeshComp, GameplayCue.GameplayCueTag, Animation);
}

FString UArcAN_GameplayCueLoopingStart::GetNotifyName_Implementation() const
{
	FString DisplayName = TEXT("GameplayCue");

	if (GameplayCue.GameplayCueTag.IsValid())
	{
		DisplayName = GameplayCue.GameplayCueTag.ToString();
		DisplayName += TEXT(" (Looping Start)");
	}

	return DisplayName;
}

void UArcAN_GameplayCueLoopingFinish::Notify(USkeletalMeshComponent* MeshComp
	, UAnimSequenceBase* Animation
	, const FAnimNotifyEventReference& EventReference)
{
	FGameplayCueParameters Parameters;
	Parameters.SourceObject = MeshComp->GetOwner();
	Parameters.EffectCauser = MeshComp->GetOwner();
	Parameters.TargetAttachComponent = MeshComp;

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(MeshComp->GetOwner()))
	{
		if (UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			Parameters.Instigator = ASC->GetOwnerActor();
			
			UAnimMontage* Montage = Cast<UAnimMontage>(Animation);

			if (const TWeakObjectPtr<const UArcItemDefinition>* ItemDef = ASC->MontageToItemDef.Find(Montage))
            {
            	const FArcItemFragment_AnimMontageGameplayCue* Cue = ItemDef->Get()->FindFragment<FArcItemFragment_AnimMontageGameplayCue>();
            	if (Cue)
            	{
            		UGameplayCueManager::RemoveGameplayCue_NonReplicated(MeshComp->GetOwner(), Cue->Cue.GameplayCueTag, Parameters);
            		ASC->MontageToItemDef.Empty();
            		return;
            	}
            }
		}
	}
	
	UGameplayCueManager::RemoveGameplayCue_NonReplicated(MeshComp->GetOwner(), GameplayCue.GameplayCueTag, Parameters);
}

FString UArcAN_GameplayCueLoopingFinish::GetNotifyName_Implementation() const
{
	FString DisplayName = TEXT("GameplayCue");

	if (GameplayCue.GameplayCueTag.IsValid())
	{
		DisplayName = GameplayCue.GameplayCueTag.ToString();
		DisplayName += TEXT(" (Looping Finish)");
	}

	return DisplayName;
}

void UArcANS_GameplayCueState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration
	, const FAnimNotifyEventReference& EventReference)
{
	FGameplayCueParameters Parameters;
	Parameters.SourceObject = MeshComp->GetOwner();
	Parameters.EffectCauser = MeshComp->GetOwner();
	Parameters.TargetAttachComponent = MeshComp;
	
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(MeshComp->GetOwner()))
	{
		if (UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
			
			Parameters.Instigator = ASC->GetOwnerActor();
			
			if (const TWeakObjectPtr<const UArcItemDefinition>* ItemDef = ASC->MontageToItemDef.Find(Montage))
			{
				const FArcItemFragment_AnimMontageGameplayCue* Cue = ItemDef->Get()->FindFragment<FArcItemFragment_AnimMontageGameplayCue>();
				if (Cue)
				{
					UGameplayCueManager::AddGameplayCue_NonReplicated(MeshComp->GetOwner(), Cue->Cue.GameplayCueTag, Parameters);
					return;
				}
			}
		}
	}
	
	Arcx::ProcessGameplayCue(&UGameplayCueManager::AddGameplayCue_NonReplicated, MeshComp, GameplayCue.GameplayCueTag, Animation);
}

void UArcANS_GameplayCueState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime
	, const FAnimNotifyEventReference& EventReference)
{
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(MeshComp->GetOwner()))
	{
		if (UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			
			
			UAnimMontage* Montage = Cast<UAnimMontage>(Animation);

			if (const TWeakObjectPtr<const UArcItemDefinition>* ItemDef = ASC->MontageToItemDef.Find(Montage))
			{
				const FArcItemFragment_AnimMontageGameplayCue* Cue = ItemDef->Get()->FindFragment<FArcItemFragment_AnimMontageGameplayCue>();
				if (Cue)
				{
					FGameplayCueParameters Parameters;
					Parameters.Instigator = ASC->GetOwnerActor();
					
					Parameters.SourceObject = MeshComp->GetOwner();
					Parameters.EffectCauser = MeshComp->GetOwner();
					
					Parameters.TargetAttachComponent = MeshComp;
					
					UGameplayCueManager::ExecuteGameplayCue_NonReplicated(MeshComp->GetOwner(), Cue->Cue.GameplayCueTag, Parameters);
					ASC->MontageToItemDef.Empty();
					return;
				}
			}
		}
	}
}

void UArcANS_GameplayCueState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation
	, const FAnimNotifyEventReference& EventReference)
{
	FGameplayCueParameters Parameters;
	Parameters.SourceObject = MeshComp->GetOwner();
	Parameters.EffectCauser = MeshComp->GetOwner();
	Parameters.TargetAttachComponent = MeshComp;

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(MeshComp->GetOwner()))
	{
		if (UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			Parameters.Instigator = ASC->GetOwnerActor();
			
			UAnimMontage* Montage = Cast<UAnimMontage>(Animation);

			if (const TWeakObjectPtr<const UArcItemDefinition>* ItemDef = ASC->MontageToItemDef.Find(Montage))
			{
				const FArcItemFragment_AnimMontageGameplayCue* Cue = ItemDef->Get()->FindFragment<FArcItemFragment_AnimMontageGameplayCue>();
				if (Cue)
				{
					UGameplayCueManager::RemoveGameplayCue_NonReplicated(MeshComp->GetOwner(), Cue->Cue.GameplayCueTag, Parameters);
					ASC->MontageToItemDef.Empty();
					return;
				}
			}
		}
	}
	
	UGameplayCueManager::RemoveGameplayCue_NonReplicated(MeshComp->GetOwner(), GameplayCue.GameplayCueTag, Parameters);
}

void UArcANS_GameplayCueState::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyBegin(BranchingPointPayload);
}

void UArcANS_GameplayCueState::BranchingPointNotifyTick(FBranchingPointNotifyPayload& BranchingPointPayload, float FrameDeltaTime)
{
	Super::BranchingPointNotifyTick(BranchingPointPayload, FrameDeltaTime);
}

void UArcANS_GameplayCueState::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyEnd(BranchingPointPayload);
}
