#include "ArcGameplayAbilitiesDebugger.h"

#include "imgui.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemDefinition.h"

void FArcGameplayAbilitiesDebugger::Initialize()
{
}

void FArcGameplayAbilitiesDebugger::Uninitialize()
{
}

void FArcGameplayAbilitiesDebugger::Draw()
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

	const TArray<FGameplayAbilitySpec>& ActivatableAbilities = AbilitySystem->GetActivatableAbilities();

	auto CheckForBlocked = [&](const FGameplayTagContainer& ContainerA, const FGameplayTagContainer& ContainerB)
	{
		// Do we not have any tags in common?  Then we're not blocked
		if (ContainerA.IsEmpty() || ContainerB.IsEmpty() || !ContainerA.HasAny(ContainerB))
		{
			return false;
		}

		return true;
	};

	// Define a common lambda to check for missing required tags
	auto CheckForRequired = [&](const FGameplayTagContainer& TagsToCheck, const FGameplayTagContainer& RequiredTags)
	{
		// Do we have no requirements, or have met all requirements?  Then nothing's missing
		if (RequiredTags.IsEmpty() || TagsToCheck.HasAll(RequiredTags))
		{
			return false;
		}

		return true;
	};

	auto BoolToText = [](bool b)
	{
		return b ? TEXT("True") : TEXT("False");
	};
	
	ImGui::Begin("Gameplay Abilities");
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
	{
		UGameplayAbility* Ability = AbilitySpec.GetPrimaryInstance();
		if (!Ability)
		{
			continue;
		}

		UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Ability);
		
		FString AbilityName = GetNameSafe(Ability);
		if (ArcAbility)
		{
			AbilityName += " (  ";
			AbilityName += GetNameSafe(ArcAbility->GetSourceItemData());
			AbilityName += "  )";
		}
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*AbilityName)))
		{
			const FGameplayAbilityActorInfo* CurrentActorInfo = Ability->GetCurrentActorInfo();
			FGameplayAbilityActivationInfo CurrentActivationInfo = Ability->GetCurrentActivationInfo();

			if (ImGui::BeginTable("AbilityLayout", 2))
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Is Active");
				
				ImGui::TableSetColumnIndex(1);
				const bool bIsActive = Ability->IsActive();
				FString IsActive = FString::Printf(TEXT("%s"), BoolToText(bIsActive));
				ImGui::Text(TCHAR_TO_ANSI(*IsActive));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Is Triggered");
				
				ImGui::TableSetColumnIndex(1);
				const bool bIsTriggered = Ability->IsTriggered();
				FString IsTriggered = FString::Printf(TEXT("%s"), BoolToText(bIsTriggered));
				ImGui::Text(TCHAR_TO_ANSI(*IsTriggered));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Is Predicting Client");
				
				ImGui::TableSetColumnIndex(1);
				const bool bIsPredictingClient = Ability->IsPredictingClient();
				FString IsPredictingClient = FString::Printf(TEXT("%s"), BoolToText(bIsPredictingClient));
				ImGui::Text(TCHAR_TO_ANSI(*IsPredictingClient));
				
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Is For Remote Client");
				
				ImGui::TableSetColumnIndex(1);
				const bool bIsForRemoteClient = Ability->IsForRemoteClient();
				FString IsForRemoteClient = FString::Printf(TEXT("%s"), BoolToText(bIsForRemoteClient));
				ImGui::Text(TCHAR_TO_ANSI(*IsForRemoteClient));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Can Activate Ability");
				
				ImGui::TableSetColumnIndex(1);
				const bool bCanActivateAbility = Ability->CanActivateAbility(AbilitySpec.Handle, CurrentActorInfo);
				FString CanActivateAbility = FString::Printf(TEXT("%s"), BoolToText(bCanActivateAbility));
				ImGui::Text(TCHAR_TO_ANSI(*CanActivateAbility));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Can Be Canceled");
				
				ImGui::TableSetColumnIndex(1);
				const bool bCanBeCanceled = Ability->CanBeCanceled();
				FString CanBeCanceled = FString::Printf(TEXT("%s"), BoolToText(bCanBeCanceled));
				ImGui::Text(TCHAR_TO_ANSI(*CanBeCanceled));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Is Blocking Other Abilities");
				
				ImGui::TableSetColumnIndex(1);
				bool bIsBlockingOtherAbilities = Ability->IsBlockingOtherAbilities();
				FString IsBlockingOtherAbilities = FString::Printf(TEXT("%s"), BoolToText(bIsBlockingOtherAbilities));
				ImGui::Text(TCHAR_TO_ANSI(*IsBlockingOtherAbilities));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Ability Satisfy Tag Requirements");
				
				ImGui::TableSetColumnIndex(1);
				bool bAbilitySatisfyTagRequirements= Ability->DoesAbilitySatisfyTagRequirements(*AbilitySystem);
				FString AbilitySatisfyTagRequirements = FString::Printf(TEXT("%s"), BoolToText(bAbilitySatisfyTagRequirements));
				ImGui::Text(TCHAR_TO_ANSI(*AbilitySatisfyTagRequirements));

				if (ArcAbility)
				{
					const FGameplayTagContainer& OwnedTags = AbilitySystem->GetOwnedGameplayTags();
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::TreeNode("Owned Tags"))
					{
						ImGui::Text(TCHAR_TO_ANSI(*OwnedTags.ToStringSimple()));
						ImGui::TreePop();
					}
				
					const FGameplayTagContainer& AbilityTags = ArcAbility->GetAssetTags();
					const FGameplayTagContainer& AbilityBlockedTags = AbilitySystem->GetBlockedAbilityTags();
					


					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ability System Have Blocking Tags");
				
					ImGui::TableSetColumnIndex(1);
					const bool bAbilitySystemBlocksAbility = CheckForBlocked(AbilityTags, AbilitySystem->GetBlockedAbilityTags());
					FString AbilitySystemBlocksAbility = FString::Printf(TEXT("%s"), BoolToText(bAbilitySystemBlocksAbility));
					ImGui::Text(TCHAR_TO_ANSI(*AbilitySystemBlocksAbility));
					
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::TreeNode("Ability Tags"))
					{
						ImGui::Text(TCHAR_TO_ANSI(*AbilityTags.ToStringSimple()));
						ImGui::TreePop();
					}
					
					if (ImGui::TreeNode("Ability Blocked Tags"))
					{
						ImGui::Text(TCHAR_TO_ANSI(*AbilityBlockedTags.ToStringSimple()));
						ImGui::TreePop();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ability Have Blocked Tags, which ability system contains");
				
					ImGui::TableSetColumnIndex(1);
					const FGameplayTagContainer& ActivationBlockedTags = ArcAbility->GetActivationBlockedTags();
					const bool bActivationBlockedTags = CheckForBlocked(AbilitySystem->GetOwnedGameplayTags(), ArcAbility->GetActivationBlockedTags());
					FString ActivationBlocked = FString::Printf(TEXT(" %s"), BoolToText(bActivationBlockedTags));
					ImGui::Text(TCHAR_TO_ANSI(*ActivationBlocked));

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::TreeNode("Activation Blocked Tags"))
					{
						ImGui::Text(TCHAR_TO_ANSI(*ActivationBlockedTags.ToStringSimple()));
						ImGui::TreePop();
					}
				
					const FGameplayTagContainer& ActivationRequiredTags = ArcAbility->GetActivationRequiredTags();
					

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Ability System, does not have Required Tags");
				
					ImGui::TableSetColumnIndex(1);
					const bool bActivationRequiredTags = CheckForRequired(AbilitySystem->GetOwnedGameplayTags(), ArcAbility->GetActivationRequiredTags());
					FString ActivationRequired = FString::Printf(TEXT("%s"), BoolToText(bActivationRequiredTags));
					ImGui::Text(TCHAR_TO_ANSI(*ActivationRequired));
					
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::TreeNode("Activation Required Tags"))
					{
						ImGui::Text(TCHAR_TO_ANSI(*ActivationRequiredTags.ToStringSimple()));
						ImGui::TreePop();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Items Store:");
				
					ImGui::TableSetColumnIndex(1);
					UArcItemsStoreComponent* SourceItemsStore = ArcAbility->GetItemsStoreComponent(nullptr);
					FString ItemsStoreName = FString::Printf(TEXT("%s"), *GetNameSafe(SourceItemsStore));
					ImGui::Text(TCHAR_TO_ANSI(*ItemsStoreName));

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Source Item:");
				
					ImGui::TableSetColumnIndex(1);
					const UArcItemDefinition* SourceItemDef = ArcAbility->GetSourceItemData();
					FString SourceItemName = FString::Printf(TEXT("%s"), *GetNameSafe(SourceItemDef));
					ImGui::Text(TCHAR_TO_ANSI(*SourceItemName));

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Have Valid Item Data Ptr");
				
					ImGui::TableSetColumnIndex(1);
					const FArcItemData* ItemData =  ArcAbility->GetSourceItemEntryPtr();
					FString ValidItemDataPtr = FString::Printf(TEXT("%s"), BoolToText(ItemData != nullptr));
					ImGui::Text(TCHAR_TO_ANSI(*ValidItemDataPtr));

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Source Item Id: ");
				
					ImGui::TableSetColumnIndex(1);
					const FArcItemId ItemId = ArcAbility->GetSourceItemHandle();
					FString ValidItemId = FString::Printf(TEXT("%s"), *ItemId.ToString());
					ImGui::Text(TCHAR_TO_ANSI(*ValidItemId));

				}
			
				
				ImGui::EndTable();
			}
			





			
			ImGui::TreePop();
		}
	}

	ImGui::End();
}
