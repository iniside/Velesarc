#include "ArcGameplayEffectsDebugger.h"

#include "ArcDebuggerHacks.h"
#include "ArcGameplayEffectContext.h"
#include "imgui.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "Items/ArcItemDefinition.h"

void FArcGameplayEffectsDebugger::Initialize()
{
}

void FArcGameplayEffectsDebugger::Uninitialize()
{
}

void FArcGameplayEffectsDebugger::Draw()
{
	if (!GEngine)
	{
		return;
	}
	
	if (!GEngine->GameViewport)
	{
		return;
	}

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

	if (!PC->PlayerState)
	{
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}

	APlayerState* PS = PC->PlayerState;

	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		return;
	}

	const double WorldTime = AbilitySystem->GetWorld()->GetTimeSeconds();
	auto BoolToText = [](bool InBool)
	{
		return InBool ? TEXT("True") : TEXT("False");
	};

	ImGui::Begin("Gameplay Effects");
	
	if (ImGui::TreeNode("Gameplay Tag Count"))
	{
		const FGameplayTagCountContainer& Tags = AbilitySystem->GetGameplayTagCountContainer();

		if (ImGui::TreeNode("Gameplay Tag Count"))
		{
			const TMap<FGameplayTag, int32> GameplayTagCountMap = DebugHack::GetPrivateGameplayTagCountMap(&Tags);
			for (const auto& TagCountPair : GameplayTagCountMap)
			{
				ImGui::Text(TCHAR_TO_ANSI(*TagCountPair.Key.ToString())); ImGui::SameLine();
				ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT(" Count: %d"), TagCountPair.Value)));
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Explicit Gameplay Tags"))
		{
			const FGameplayTagContainer& GameplayTagCountMap = Tags.GetExplicitGameplayTags();
			for (const auto& TagCountPair : GameplayTagCountMap)
			{
				ImGui::Text(TCHAR_TO_ANSI(*TagCountPair.ToString()));
			}
			
			ImGui::TreePop();
		}
		
		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Minimal Replicated Tags"))
	{
		const FMinimalReplicationTagCountMap& Tags = AbilitySystem->GetMinimalReplicationCountTags();

		for (const auto& TagCountPair : Tags.TagMap)
		{
			ImGui::Text(TCHAR_TO_ANSI(*TagCountPair.Key.ToString())); ImGui::SameLine();
			ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT(" Count: %d"), TagCountPair.Value)));
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Loose Replicated Tags"))
	{
		const FMinimalReplicationTagCountMap& Tags = AbilitySystem->GetReplicatedLooseCountTags();
		for (const auto& TagCountPair : Tags.TagMap)
		{
			ImGui::Text(TCHAR_TO_ANSI(*TagCountPair.Key.ToString())); ImGui::SameLine();
			ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT(" Count: %d"), TagCountPair.Value)));
		}
		ImGui::TreePop();
	}

	const FActiveGameplayEffectsContainer& ActiveGEContainer = AbilitySystem->GetActiveGameplayEffects();
	
	
		for (const FActiveGameplayEffectHandle& ActiveGEHandle : ActiveGEContainer.GetAllActiveEffectHandles())
		{
			if (const FActiveGameplayEffect* ActiveGE = AbilitySystem->GetActiveGameplayEffect(ActiveGEHandle))
			{
				const UGameplayEffect* GESpecDef = ActiveGE->Spec.Def;
		
				const FString EffectName = FString::Printf(TEXT("Effect Name: %s"), *GetNameSafe(GESpecDef));
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*EffectName)))
				{
					if (ImGui::BeginTable("AbilityLayout", 2))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Is Inhibited");
						ImGui::TableSetColumnIndex(1);
						const FString IsInhibited = FString::Printf(TEXT("%s"), BoolToText(ActiveGE->bIsInhibited));
						ImGui::Text(TCHAR_TO_ANSI(*IsInhibited));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Stack Count");
						ImGui::TableSetColumnIndex(1);
						const FString StackCount = FString::Printf(TEXT("%d"), ActiveGE->Spec.GetStackCount());
						ImGui::Text(TCHAR_TO_ANSI(*StackCount));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Duration:");
						ImGui::TableSetColumnIndex(1);
						const FString EffectDuration = FString::Printf(TEXT("%.2f"), ActiveGE->Spec.GetDuration());
						ImGui::Text(TCHAR_TO_ANSI(*EffectDuration));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Period: ");
						ImGui::TableSetColumnIndex(1);
						const FString EffectPeriod = FString::Printf(TEXT("%.2f"), ActiveGE->Spec.GetPeriod());
						ImGui::Text(TCHAR_TO_ANSI(*EffectPeriod));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("End Time");
						ImGui::TableSetColumnIndex(1);
						const FString EndTime = FString::Printf(TEXT(" %.2f"), ActiveGE->GetEndTime());
						ImGui::Text(TCHAR_TO_ANSI(*EndTime));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Level:");
						ImGui::TableSetColumnIndex(1);
						const FString EffectLevel = FString::Printf(TEXT(" %.2f"), ActiveGE->Spec.GetLevel());
						ImGui::Text(TCHAR_TO_ANSI(*EffectLevel));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Time Remaining:");
						ImGui::TableSetColumnIndex(1);
						const FString TimeRemaining = FString::Printf(TEXT("Time Remaining: %.2f"), ActiveGE->GetTimeRemaining(WorldTime));
						ImGui::Text(TCHAR_TO_ANSI(*TimeRemaining));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						if (ImGui::TreeNode("All Granted Tags"))
						{
							FGameplayTagContainer OutTags;
							ActiveGE->Spec.GetAllGrantedTags(OutTags);
							ImGui::Text(TCHAR_TO_ANSI(*OutTags.ToStringSimple()));
		            	
							ImGui::TreePop();
						}
		            	
						if (ImGui::TreeNode("Blocked Ability Tags"))
						{
							FGameplayTagContainer OutTags;
							ActiveGE->Spec.GetAllBlockedAbilityTags(OutTags);
							ImGui::Text(TCHAR_TO_ANSI(*OutTags.ToStringSimple()));
		            	
							ImGui::TreePop();
						}
		            	
						if (ImGui::TreeNode("Asset Tags"))
						{
							FGameplayTagContainer OutTags;
							ActiveGE->Spec.GetAllAssetTags(OutTags);
							ImGui::Text(TCHAR_TO_ANSI(*OutTags.ToStringSimple()));
		            	
							ImGui::TreePop();
						}

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Effect Causer");
						ImGui::TableSetColumnIndex(1);
						AActor* EffectCauser = ActiveGE->Spec.GetEffectContext().GetEffectCauser();
						FString EffectCauserName = FString::Printf(TEXT("%s"), *GetNameSafe(EffectCauser));
						ImGui::Text(TCHAR_TO_ANSI(*EffectCauserName));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Instigator ");
						ImGui::TableSetColumnIndex(1);
						AActor* Instigator = ActiveGE->Spec.GetEffectContext().GetInstigator();
						FString InstigatorName = FString::Printf(TEXT("%s"), *GetNameSafe(Instigator));
						ImGui::Text(TCHAR_TO_ANSI(*InstigatorName));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Original Instigator ");
						ImGui::TableSetColumnIndex(1);
						AActor* OriginalInstigator = ActiveGE->Spec.GetEffectContext().GetOriginalInstigator();
						FString OriginalInstigatorName = FString::Printf(TEXT("%s"), *GetNameSafe(OriginalInstigator));
						ImGui::Text(TCHAR_TO_ANSI(*OriginalInstigatorName));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Source Object ");
						ImGui::TableSetColumnIndex(1);
						UObject* SourceObject = ActiveGE->Spec.GetEffectContext().GetSourceObject();
						FString SourceObjectName = FString::Printf(TEXT("%s"), *GetNameSafe(SourceObject));
						ImGui::Text(TCHAR_TO_ANSI(*SourceObjectName));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Ability ");
						ImGui::TableSetColumnIndex(1);
						const UGameplayAbility* Ability = ActiveGE->Spec.GetEffectContext().GetAbility();
						FString AbilityName = FString::Printf(TEXT("%s"), *GetNameSafe(Ability));
						ImGui::Text(TCHAR_TO_ANSI(*AbilityName));
		
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
					
						const TArray<TWeakObjectPtr<AActor>> Actors = ActiveGE->Spec.GetEffectContext().Get()->GetActors();
						ImGui::Text("Actors: ");
						for (const TWeakObjectPtr<AActor>& Actor : Actors)
						{
							ImGui::Text(TCHAR_TO_ANSI(*GetNameSafe(Actor.Get())));
						}
		
						if (const FHitResult* HitResult = ActiveGE->Spec.GetEffectContext().GetHitResult())
						{
							ImGui::Text("Hit Result: ");
							ImGui::Text(TCHAR_TO_ANSI(*HitResult->ToString()));
						}
		
						if (ActiveGE->Spec.GetEffectContext().Get()->GetScriptStruct()->IsChildOf(FArcGameplayEffectContext::StaticStruct()))
						{
							const FArcGameplayEffectContext* Context = static_cast<const FArcGameplayEffectContext*>(ActiveGE->Spec.GetEffectContext().Get());

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text("Source Item Id:");
							ImGui::TableSetColumnIndex(1);
							FString SourceItemId = FString::Printf(TEXT(" %s"), *Context->GetSourceItemHandle().ToString());
							ImGui::Text(TCHAR_TO_ANSI(*SourceItemId));

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text("Have Valid Source Item Data ");
							ImGui::TableSetColumnIndex(1);
							const FArcItemData* SourceItemData = Context->GetSourceItemPtr();
							FString HaveValidSourceItemData = FString::Printf(TEXT("%s"), BoolToText(SourceItemData != nullptr));
							ImGui::Text(TCHAR_TO_ANSI(*HaveValidSourceItemData));

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text("Source Item: ");
							ImGui::TableSetColumnIndex(1);
							const UArcItemDefinition* SourceItemDef = Context->GetSourceItem();
							FString SourceItemName = FString::Printf(TEXT("%s"), *GetNameSafe(SourceItemDef));
							ImGui::Text(TCHAR_TO_ANSI(*SourceItemName));
						}

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						if (ImGui::TreeNode("Modified Attributes"))
						{
							for (const FGameplayEffectModifiedAttribute& AttributeMods : ActiveGE->Spec.ModifiedAttributes)
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								FString AttributeName = FString::Printf(TEXT("Attribute Name: %s"), *AttributeMods.Attribute.GetName());
								ImGui::Text(TCHAR_TO_ANSI(*AttributeName));
								ImGui::TableSetColumnIndex(1);
								ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Base Value: %.2f"), AttributeMods.TotalMagnitude)));
							}
							ImGui::TreePop();
						}

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						if (ImGui::TreeNode("Modifiers"))
						{
							// Iterate through all modifiers in the spec
							for (int32 Idx = 0; Idx < ActiveGE->Spec.Modifiers.Num(); Idx++)
							{
								const float ModMagnitude = ActiveGE->Spec.GetModifierMagnitude(Idx);
								ImGui::Text("Modifier Magnitude %.2f", ModMagnitude);
							}
					
							ImGui::TreePop();
						}
					
		
						ImGui::EndTable();
					}
					ImGui::TreePop();
				}
			}
		
	}
	
	ImGui::End();
}
