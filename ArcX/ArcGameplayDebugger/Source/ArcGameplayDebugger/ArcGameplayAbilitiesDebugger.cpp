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
	
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
	{
		UGameplayAbility* Ability = AbilitySpec.GetPrimaryInstance();
		if (!Ability)
		{
			continue;
		}

		auto BoolToText = [](bool b)
		{
			return b ? TEXT("True") : TEXT("False");
		};
		
		FString AbilityName = GetNameSafe(Ability);
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*AbilityName)))
		{
			const FGameplayAbilityActorInfo* CurrentActorInfo = Ability->GetCurrentActorInfo();
			FGameplayAbilityActivationInfo CurrentActivationInfo = Ability->GetCurrentActivationInfo();

			const bool bIsActive = Ability->IsActive();
			FString IsActive = FString::Printf(TEXT("Is Active %s"), BoolToText(bIsActive));
			ImGui::Text(TCHAR_TO_ANSI(*IsActive));

			const bool bIsTriggered = Ability->IsTriggered();
			FString IsTriggered = FString::Printf(TEXT("Is Triggered %s"), BoolToText(bIsTriggered));
			ImGui::Text(TCHAR_TO_ANSI(*IsTriggered));

			const bool bIsPredictingClient = Ability->IsPredictingClient();
			FString IsPredictingClient = FString::Printf(TEXT("Is Predicting Client %s"), BoolToText(bIsPredictingClient));
			ImGui::Text(TCHAR_TO_ANSI(*IsPredictingClient));

			const bool bIsForRemoteClient = Ability->IsForRemoteClient();
			FString IsForRemoteClient = FString::Printf(TEXT("Is For Remote Client %s"), BoolToText(bIsForRemoteClient));
			ImGui::Text(TCHAR_TO_ANSI(*IsForRemoteClient));

			const bool bCanActivateAbility = Ability->CanActivateAbility(AbilitySpec.Handle, CurrentActorInfo);
			FString CanActivateAbility = FString::Printf(TEXT("Can Activate Ability %s"), BoolToText(bCanActivateAbility));
			ImGui::Text(TCHAR_TO_ANSI(*CanActivateAbility));

			const bool bCanBeCanceled = Ability->CanBeCanceled();
			FString CanBeCanceled = FString::Printf(TEXT("Can Be Canceled %s"), BoolToText(bCanBeCanceled));
			ImGui::Text(TCHAR_TO_ANSI(*CanBeCanceled));

			bool bIsBlockingOtherAbilities = Ability->IsBlockingOtherAbilities();
			FString IsBlockingOtherAbilities = FString::Printf(TEXT("Is Blocking Other Abilities %s"), BoolToText(bIsBlockingOtherAbilities));
			ImGui::Text(TCHAR_TO_ANSI(*IsBlockingOtherAbilities));

			bool bAbilitySatisfyTagRequirements= Ability->DoesAbilitySatisfyTagRequirements(*AbilitySystem);
			FString AbilitySatisfyTagRequirements = FString::Printf(TEXT("Ability Satisfy Tag Requirements %s"), BoolToText(bAbilitySatisfyTagRequirements));
			ImGui::Text(TCHAR_TO_ANSI(*AbilitySatisfyTagRequirements));

			if (UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Ability))
			{
				const FGameplayTagContainer& OwnedTags = AbilitySystem->GetOwnedGameplayTags();
				if (ImGui::TreeNode("Owned Tags"))
				{
					ImGui::Text(TCHAR_TO_ANSI(*OwnedTags.ToStringSimple()));
					ImGui::TreePop();
				}

				const FGameplayTagContainer& AbilityTags = ArcAbility->GetAssetTags();
				const FGameplayTagContainer& AbilityBlockedTags = AbilitySystem->GetBlockedAbilityTags();
				
				const bool bAbilitySystemBlocksAbility = CheckForBlocked(AbilityTags, AbilitySystem->GetBlockedAbilityTags());
				FString AbilitySystemBlocksAbility = FString::Printf(TEXT("Ability System Have Blocking Tags %s"), BoolToText(bAbilitySystemBlocksAbility));
				ImGui::Text(TCHAR_TO_ANSI(*AbilitySystemBlocksAbility));

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

				const FGameplayTagContainer& ActivationBlockedTags = ArcAbility->GetActivationBlockedTags();
				const bool bActivationBlockedTags = CheckForBlocked(AbilitySystem->GetOwnedGameplayTags(), ArcAbility->GetActivationBlockedTags());
				FString ActivationBlocked = FString::Printf(TEXT("Ability Have Blocked Tags, which ability system contains %s"), BoolToText(bActivationBlockedTags));
				ImGui::Text(TCHAR_TO_ANSI(*ActivationBlocked));

				if (ImGui::TreeNode("Activation Blocked Tags"))
				{
					ImGui::Text(TCHAR_TO_ANSI(*ActivationBlockedTags.ToStringSimple()));
					ImGui::TreePop();
				}

				const FGameplayTagContainer& ActivationRequiredTags = ArcAbility->GetActivationRequiredTags();
				const bool bActivationRequiredTags = CheckForRequired(AbilitySystem->GetOwnedGameplayTags(), ArcAbility->GetActivationRequiredTags());
				FString ActivationRequired = FString::Printf(TEXT("Ability System, does not have Required Tags %s"), BoolToText(bActivationRequiredTags));
				ImGui::Text(TCHAR_TO_ANSI(*ActivationRequired));

				if (ImGui::TreeNode("Activation Required Tags"))
				{
					ImGui::Text(TCHAR_TO_ANSI(*ActivationRequiredTags.ToStringSimple()));
					ImGui::TreePop();
				}

				UArcItemsStoreComponent* SourceItemsStore = ArcAbility->GetItemsStoreComponent(nullptr);
				FString ItemsStoreName = FString::Printf(TEXT("Items Store: %s"), *GetNameSafe(SourceItemsStore));
				ImGui::Text(TCHAR_TO_ANSI(*ItemsStoreName));

				const UArcItemDefinition* SourceItemDef = ArcAbility->GetSourceItemData();
				FString SourceItemName = FString::Printf(TEXT("Source Item: %s"), *GetNameSafe(SourceItemDef));
				ImGui::Text(TCHAR_TO_ANSI(*SourceItemName));

				const FArcItemData* ItemData =  ArcAbility->GetSourceItemEntryPtr();
				FString ValidItemDataPtr = FString::Printf(TEXT("Have Valid Item Data Ptr %s"), BoolToText(ItemData != nullptr));
				ImGui::Text(TCHAR_TO_ANSI(*ValidItemDataPtr));

				const FArcItemId ItemId = ArcAbility->GetSourceItemHandle();
				FString ValidItemId = FString::Printf(TEXT("Source Item Id: %s"), *ItemId.ToString());
				ImGui::Text(TCHAR_TO_ANSI(*ValidItemId));
			}
			
			ImGui::TreePop();
		}
	}
}
