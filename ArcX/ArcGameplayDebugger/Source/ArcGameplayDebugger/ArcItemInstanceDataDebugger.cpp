﻿#include "ArcItemInstanceDataDebugger.h"

#include "SlateIM.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Kismet/GameplayStatics.h"
#include "imgui.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"

void FArcItemInstanceDataDebugger::DrawGrantedAbilities(const FArcItemData* InItemData, const FArcItemInstance* InInstance)
{
	UWorld* World = GEngine->GameViewport->GetWorld();
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	APlayerState* PS = PC->PlayerState;
	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	
	if (InInstance->GetScriptStruct()->IsChildOf(FArcItemInstance_GrantedAbilities::StaticStruct()))
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("GrantedAbilities"));
		}
		if (SlateIM::BeginTableRowChildren())
		{
			const FArcItemFragment_GrantedAbilities* Fragment = ArcItems::FindFragment<FArcItemFragment_GrantedAbilities>(InItemData);
			if (SlateIM::NextTableCell())
			{
				SlateIM::Text(TEXT("Fragment"));
			}
			if (SlateIM::BeginTableRowChildren())
			{
				SlateIM::BeginVerticalStack();
				const TArray<FArcAbilityEntry>& GrantedAbilities = Fragment->Abilities;
				for (const FArcAbilityEntry& AbilityHandle : GrantedAbilities)
				{
					if (AbilityHandle.GrantedAbility)
					{
						SlateIM::Text(FString::Printf(TEXT("Input Tag: %s"), *AbilityHandle.InputTag.ToString()));
						SlateIM::Text(FString::Printf(TEXT("Add Input Tag: %s"), AbilityHandle.bAddInputTag ? TEXT("True") : TEXT("False")));
						SlateIM::Text(FString::Printf(TEXT("Granted Ability: %s"), *GetNameSafe(AbilityHandle.GrantedAbility)));
					}
				}
				SlateIM::EndVerticalStack();
			}
			SlateIM::EndTableRowChildren();
			
			const FArcItemInstance_GrantedAbilities* Instance = static_cast<const FArcItemInstance_GrantedAbilities*>(InInstance);
			if (SlateIM::NextTableCell())
			{
				SlateIM::Text(TEXT("Instance"));
			}
			if (SlateIM::BeginTableRowChildren())
			{
				SlateIM::BeginVerticalStack();
				const TArray<FGameplayAbilitySpecHandle>& GrantedAbilities = Instance->GetGrantedAbilities();
				for (const FGameplayAbilitySpecHandle& AbilityHandle : GrantedAbilities)
				{
					FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(AbilityHandle);
					if (Spec)
					{
						const UGameplayAbility* Ability = Spec->GetPrimaryInstance();
						if (Ability)
						{
							SlateIM::Text(FString::Printf(TEXT("Handle: %s"), *AbilityHandle.ToString()));
							SlateIM::Text(FString::Printf(TEXT("Granted Ability: %s"), *GetNameSafe(Ability)));
						}
					}
				}
				SlateIM::EndVerticalStack();
			}
			SlateIM::EndTableRowChildren();
		}
		SlateIM::EndTableRowChildren();
	}
}

void FArcItemInstanceDebugger::DrawAbilityEffectsToApply(const FArcItemData* InItemData, const FArcItemInstance* InInstance)
{
	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}
	
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}
	
	APlayerState* PS = PC->PlayerState;
	if (!PS)
	{
		return;
	}

	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		return;
	}
	
	if (InInstance->GetScriptStruct()->IsChildOf(FArcItemInstance_EffectToApply::StaticStruct()))
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("AbilityEffectsToApply"));
		}
		if (SlateIM::BeginTableRowChildren())
		{
			const FArcItemFragment_AbilityEffectsToApply* Fragment = ArcItems::FindFragment<FArcItemFragment_AbilityEffectsToApply>(InItemData);
			if (SlateIM::NextTableCell())
			{
				SlateIM::Text(TEXT("Fragment"));
			}
			if (SlateIM::BeginTableRowChildren())
			{
				const TMap<FGameplayTag, FArcMapEffectItem>& GrantedAbilities = Fragment->Effects;
				for (const TPair<FGameplayTag, FArcMapEffectItem>& AbilityHandle : GrantedAbilities)
				{
					if (SlateIM::NextTableCell())
					{
						SlateIM::Text(AbilityHandle.Key.ToString());
					}
					if (SlateIM::BeginTableRowChildren())
					{
						SlateIM::BeginVerticalStack();
						SlateIM::Text(FString::Printf(TEXT("SourceRequiredTags: %s"), *AbilityHandle.Value.SourceRequiredTags.ToString()));
						SlateIM::Text(FString::Printf(TEXT("SourceIgnoreTags: %s"), *AbilityHandle.Value.SourceIgnoreTags.ToString()));
						SlateIM::Text(FString::Printf(TEXT("TargetRequiredTags: %s"), *AbilityHandle.Value.TargetRequiredTags.ToString()));
						SlateIM::Text(FString::Printf(TEXT("TargetIgnoreTags: %s"), *AbilityHandle.Value.TargetIgnoreTags.ToString()));
						SlateIM::EndVerticalStack();
						
						if (SlateIM::NextTableCell())
						{
							SlateIM::Text(TEXT("Effects"));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							SlateIM::BeginVerticalStack();
							for (const TSubclassOf<UGameplayEffect>& Effect : AbilityHandle.Value.Effects)
							{
								SlateIM::Text(FString::Printf(TEXT("Effect: %s"), *GetNameSafe(Effect)));
							}
							SlateIM::EndVerticalStack();
						}
						SlateIM::EndTableRowChildren();
					}
					SlateIM::EndTableRowChildren();
				}
			}
			SlateIM::EndTableRowChildren();
			
			const FArcItemInstance_EffectToApply* Instance = static_cast<const FArcItemInstance_EffectToApply*>(InInstance);
			if (SlateIM::NextTableCell())
			{
				SlateIM::Text(TEXT("Instance"));
			}
			if (SlateIM::BeginTableRowChildren())
			{
				const TArray<FArcEffectSpecItem>& EffectsToApply = Instance->GetAllEffectSpecHandles();
				
				for (const FArcEffectSpecItem& AbilityHandle : EffectsToApply)
				{
					SlateIM::BeginVerticalStack();
					SlateIM::Text(FString::Printf(TEXT("SourceRequiredTags: %s"), *AbilityHandle.SourceRequiredTags.ToStringSimple()));
					SlateIM::Text(FString::Printf(TEXT("SourceIgnoreTags: %s"), *AbilityHandle.SourceIgnoreTags.ToStringSimple()));
					SlateIM::Text(FString::Printf(TEXT("TargetRequiredTags: %s"), *AbilityHandle.TargetRequiredTags.ToStringSimple()));
					SlateIM::Text(FString::Printf(TEXT("TargetIgnoreTags: %s"), *AbilityHandle.TargetIgnoreTags.ToStringSimple()));
					SlateIM::EndVerticalStack();
					
					for (const FGameplayEffectSpecHandle& Spec : AbilityHandle.Specs)
					{
						if (SlateIM::NextTableCell())
						{
							SlateIM::Text(FString::Printf(TEXT("Effect: %s"), *GetNameSafe(Spec.Data->Def)));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							SlateIM::BeginVerticalStack();
							SlateIM::Text(FString::Printf(TEXT("Level: %f"), Spec.Data->GetLevel()));
							SlateIM::Text(FString::Printf(TEXT("Duration: %f"), Spec.Data->GetDuration()));
							SlateIM::Text(FString::Printf(TEXT("Period: %f"), Spec.Data->GetPeriod()));
							SlateIM::Text(FString::Printf(TEXT("CapturedSourceTags: %s"), *Spec.Data->CapturedSourceTags.GetAggregatedTags()->ToStringSimple()));
							SlateIM::Text(FString::Printf(TEXT("CapturedTargetTags: %s"), *Spec.Data->CapturedTargetTags.GetAggregatedTags()->ToStringSimple()));
							SlateIM::Text(FString::Printf(TEXT("DynamicGrantedTags: %s"), *Spec.Data->DynamicGrantedTags.ToStringSimple()));;
							SlateIM::Text(FString::Printf(TEXT("CapturedTargetTags: %s"), *Spec.Data->GetDynamicAssetTags().ToStringSimple()));
							SlateIM::EndVerticalStack();
							
							if (SlateIM::NextTableCell())
							{
								SlateIM::Text(TEXT("Modified Attributes"));
							}
							if (SlateIM::BeginTableRowChildren())
							{
								SlateIM::BeginVerticalStack();
								for (const FGameplayEffectModifiedAttribute& Attribute : Spec.Data->ModifiedAttributes)
								{
									SlateIM::Text(FString::Printf(TEXT("Attribute: %s Value: %f"), *Attribute.Attribute.GetName(), Attribute.TotalMagnitude));
								}
								SlateIM::EndVerticalStack();
							}
							SlateIM::EndTableRowChildren();

							if (SlateIM::NextTableCell())
							{
								SlateIM::Text(TEXT("Modifiers: "));
							}
							if (SlateIM::BeginTableRowChildren())
							{
								SlateIM::BeginVerticalStack();
								for (const FModifierSpec& Attribute : Spec.Data->Modifiers)
								{
									SlateIM::Text(FString::Printf(TEXT("Value: %f"), Attribute.GetEvaluatedMagnitude()));
								}
								SlateIM::EndVerticalStack();
							}
							SlateIM::EndTableRowChildren();
						}
						SlateIM::EndTableRowChildren();
					}
				}
			}
			SlateIM::EndTableRowChildren();
		}
		SlateIM::EndTableRowChildren();
	}
}

void FArcItemInstanceDataDebugger::DrawAbilityEffectsToApply(const FArcItemData* InItemData, const FArcItemInstance* InInstance)
{
	UWorld* World = GEngine->GameViewport->GetWorld();
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	APlayerState* PS = PC->PlayerState;
	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	
	if (InInstance->GetScriptStruct()->IsChildOf(FArcItemInstance_EffectToApply::StaticStruct()))
	{
		const FArcItemFragment_AbilityEffectsToApply* Fragment = ArcItems::FindFragment<FArcItemFragment_AbilityEffectsToApply>(InItemData);
	
		if (ImGui::TreeNode("Fragment"))
		{
			const TMap<FGameplayTag, FArcMapEffectItem>& GrantedAbilities = Fragment->Effects;
			for (const TPair<FGameplayTag, FArcMapEffectItem>& AbilityHandle : GrantedAbilities)
			{
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*AbilityHandle.Key.ToString())))
				{
					ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("SourceRequiredTags: %s"), *AbilityHandle.Value.SourceRequiredTags.ToString())));
					ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("SourceIgnoreTags: %s"), *AbilityHandle.Value.SourceIgnoreTags.ToString())));
					ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("TargetRequiredTags: %s"), *AbilityHandle.Value.TargetRequiredTags.ToString())));
					ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("TargetIgnoreTags: %s"), *AbilityHandle.Value.TargetIgnoreTags.ToString())));
					
					ImGui::Text("Effects: ");
					
					for (const TSubclassOf<UGameplayEffect>& Effect : AbilityHandle.Value.Effects)
					{
						ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Effect: %s"), *GetNameSafe(Effect))));
					}
					
					
					ImGui::TreePop();
				}
			}
			
			ImGui::TreePop();
		}
				
		const FArcItemInstance_EffectToApply* Instance = static_cast<const FArcItemInstance_EffectToApply*>(InInstance);
		if (ImGui::TreeNode("Instance"))
		{
			const TArray<FArcEffectSpecItem>& EffectsToApply = Instance->GetAllEffectSpecHandles();
			
			for (const FArcEffectSpecItem& AbilityHandle : EffectsToApply)
			{
				ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("SourceRequiredTags: %s"), *AbilityHandle.SourceRequiredTags.ToStringSimple())));
				ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("SourceIgnoreTags: %s"), *AbilityHandle.SourceIgnoreTags.ToStringSimple())));
				ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("TargetRequiredTags: %s"), *AbilityHandle.TargetRequiredTags.ToStringSimple())));
				ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("TargetIgnoreTags: %s"), *AbilityHandle.TargetIgnoreTags.ToStringSimple())));
				
				for (const FGameplayEffectSpecHandle& Spec : AbilityHandle.Specs)
				{
					if (ImGui::TreeNode(TCHAR_TO_ANSI(*FString::Printf(TEXT("Effect: %s"), *GetNameSafe(Spec.Data->Def)))))
					{
						FArcItemInstanceDebugger::DrawGameplayEffectSpec(&Spec);
						ImGui::TreePop();
					}
				}
			}

			ImGui::TreePop();
		}
	}
}

void FArcItemInstanceDebugger::DrawGrantedAbilities(const FArcItemData* InItemData, const FArcItemInstance* InInstance)
{
	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}
	
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	APlayerState* PS = PC->PlayerState;
	if (!PS)
	{
		return;
	}
	
	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		return;
	}
	
	if (InInstance->GetScriptStruct()->IsChildOf(FArcItemInstance_GrantedAbilities::StaticStruct()))
	{
		const FArcItemFragment_GrantedAbilities* Fragment = ArcItems::FindFragment<FArcItemFragment_GrantedAbilities>(InItemData);
		if (ImGui::TreeNode("Fragment"))
		{
			const TArray<FArcAbilityEntry>& GrantedAbilities = Fragment->Abilities;
			for (const FArcAbilityEntry& AbilityHandle : GrantedAbilities)
			{
				if (AbilityHandle.GrantedAbility)
				{
					ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Input Tag: %s"), *AbilityHandle.InputTag.ToString())));
					ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Add Input Tag: %s"), AbilityHandle.bAddInputTag ? TEXT("True") : TEXT("False"))));
					ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Granted Ability: %s"), *GetNameSafe(AbilityHandle.GrantedAbility))));
				}
			}
		
			ImGui::TreePop();
		}
		
		
		
		const FArcItemInstance_GrantedAbilities* Instance = static_cast<const FArcItemInstance_GrantedAbilities*>(InInstance);
		if (ImGui::TreeNode("Instance"))
		{
			
			const TArray<FGameplayAbilitySpecHandle>& GrantedAbilities = Instance->GetGrantedAbilities();
			for (const FGameplayAbilitySpecHandle& AbilityHandle : GrantedAbilities)
			{
				FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(AbilityHandle);
				if (Spec)
				{
					const UGameplayAbility* Ability = Spec->GetPrimaryInstance();
					if (Ability)
					{
						ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Handle: %s"), *AbilityHandle.ToString())));
						ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Granted Ability: %s"), *GetNameSafe(Ability))));
					}
				}
			}
	
			ImGui::TreePop();
		}
	}
}

void FArcItemInstanceDebugger::DrawGameplayEffectSpec(const FGameplayEffectSpecHandle* Spec)
{
	ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Level: %f"), Spec->Data->GetLevel())));
	ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Duration: %f"), Spec->Data->GetDuration())));
	ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Period: %f"), Spec->Data->GetPeriod())));
	ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("CapturedSourceTags: %s"), *Spec->Data->CapturedSourceTags.GetAggregatedTags()->ToStringSimple())));
	ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("CapturedTargetTags: %s"), *Spec->Data->CapturedTargetTags.GetAggregatedTags()->ToStringSimple())));
	ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("DynamicGrantedTags: %s"), *Spec->Data->DynamicGrantedTags.ToStringSimple())));
	ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("CapturedTargetTags: %s"), *Spec->Data->GetDynamicAssetTags().ToStringSimple())));
						
	ImGui::Text("Modified Attributes: ");
						
	for (const FGameplayEffectModifiedAttribute& Attribute : Spec->Data->ModifiedAttributes)
	{
		ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Attribute: %s Value: %f"), *Attribute.Attribute.GetName(), Attribute.TotalMagnitude)));
	}
						
	ImGui::Text("Modifiers: ");
						
	for (const FModifierSpec& Attribute : Spec->Data->Modifiers)
	{
		ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Value: %f"), Attribute.GetEvaluatedMagnitude())));
	}
}

void FArcItemInstanceDebugger::DrawGameplayAbilitySpec(UArcCoreAbilitySystemComponent* InASC, const FGameplayAbilitySpec* Spec)
{
	if (!InASC || !Spec)
	{
		return;
	}

	UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());
	
	FString AbilityName = GetNameSafe(Spec->GetPrimaryInstance());
	//ImGui::Text(TCHAR_TO_ANSI(*AbilityName));
	if (ImGui::TreeNode(TCHAR_TO_ANSI(*AbilityName)))
	{
		ImGui::Text("DynamicTags: %s", TCHAR_TO_ANSI(*Spec->GetDynamicSpecSourceTags().ToString()));
		if (ArcAbility)
		{
			double StartTime = InASC->GetAbilityActivationStartTime(Spec->Handle);
			const float ActivationTime = ArcAbility->GetActivationTime();
			double CurrentTime = StartTime > -1 ? FPlatformTime::Seconds() - StartTime : 0.0;
			FString ActivationTimeStr = FString::Printf(TEXT("StartTime: %.2f, ActivationTime: %.2f CurrentTime %.2f"), StartTime, ActivationTime, CurrentTime);
			ImGui::Text("ActivationTime: %s", TCHAR_TO_ANSI(*ActivationTimeStr));
		}

		ImGui::TreePop();
	}
}
