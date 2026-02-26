// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilitiesDebugger.h"

#include "ArcDebuggerHacks.h"
#include "imgui.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemDefinition.h"

namespace
{
	auto BoolToText = [](bool b) -> const TCHAR*
	{
		return b ? TEXT("True") : TEXT("False");
	};

	void TableRow(const char* Label, const FString& Value)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("%s", Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Value));
	}

	void TableRowBool(const char* Label, bool bValue)
	{
		TableRow(Label, FString(BoolToText(bValue)));
	}

	void TableRowFloat(const char* Label, float Value)
	{
		TableRow(Label, FString::Printf(TEXT("%.4f"), Value));
	}

	void TableRowInt(const char* Label, int32 Value)
	{
		TableRow(Label, FString::Printf(TEXT("%d"), Value));
	}

	void TableRowTags(const char* Label, const FGameplayTagContainer& Tags)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		if (ImGui::TreeNode(Label))
		{
			if (Tags.IsEmpty())
			{
				ImGui::TextDisabled("(none)");
			}
			else
			{
				for (const FGameplayTag& Tag : Tags)
				{
					ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
				}
			}
			ImGui::TreePop();
		}
	}
}

void FArcGameplayAbilitiesDebugger::Initialize()
{
}

void FArcGameplayAbilitiesDebugger::Uninitialize()
{
	SelectedASC = nullptr;
}

void FArcGameplayAbilitiesDebugger::Draw()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Gameplay Abilities Debugger", &bShow);

	DrawASCSelector();

	ImGui::Separator();

	UAbilitySystemComponent* ASC = SelectedASC.Get();
	if (ASC)
	{
		DrawAbilitySystemDetails(ASC);
	}
	else
	{
		ImGui::TextDisabled("Select an Ability System Component above.");
	}

	ImGui::End();
}

void FArcGameplayAbilitiesDebugger::DrawASCSelector()
{
	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}

	// Collect all ASCs in the world
	struct FASCEntry
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FString DisplayName;
	};

	TArray<FASCEntry> Entries;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		// Check for IAbilitySystemInterface first
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				FASCEntry Entry;
				Entry.ASC = ASC;

				AActor* OwnerActor = ASC->GetOwnerActor();
				AActor* AvatarActor = ASC->GetAvatarActor();

				FString OwnerName = GetNameSafe(OwnerActor);
				FString AvatarName = GetNameSafe(AvatarActor);
				FString ClassName = ASC->GetClass()->GetName();

				if (OwnerActor == AvatarActor || !AvatarActor)
				{
					Entry.DisplayName = FString::Printf(TEXT("[%s] %s"), *ClassName, *OwnerName);
				}
				else
				{
					Entry.DisplayName = FString::Printf(TEXT("[%s] %s (Avatar: %s)"), *ClassName, *OwnerName, *AvatarName);
				}

				// Deduplicate
				bool bAlreadyAdded = false;
				for (const FASCEntry& Existing : Entries)
				{
					if (Existing.ASC == ASC)
					{
						bAlreadyAdded = true;
						break;
					}
				}
				if (!bAlreadyAdded)
				{
					Entries.Add(MoveTemp(Entry));
				}
			}
		}

		// Also check components directly for actors that don't implement the interface
		TArray<UAbilitySystemComponent*> Components;
		Actor->GetComponents<UAbilitySystemComponent>(Components);
		for (UAbilitySystemComponent* ASC : Components)
		{
			if (!ASC)
			{
				continue;
			}

			bool bAlreadyAdded = false;
			for (const FASCEntry& Existing : Entries)
			{
				if (Existing.ASC == ASC)
				{
					bAlreadyAdded = true;
					break;
				}
			}
			if (!bAlreadyAdded)
			{
				FASCEntry Entry;
				Entry.ASC = ASC;

				AActor* OwnerActor = ASC->GetOwnerActor();
				AActor* AvatarActor = ASC->GetAvatarActor();
				FString OwnerName = GetNameSafe(OwnerActor);
				FString ClassName = ASC->GetClass()->GetName();

				if (AvatarActor && AvatarActor != OwnerActor)
				{
					Entry.DisplayName = FString::Printf(TEXT("[%s] %s (Avatar: %s)"), *ClassName, *OwnerName, *GetNameSafe(AvatarActor));
				}
				else
				{
					Entry.DisplayName = FString::Printf(TEXT("[%s] %s"), *ClassName, *OwnerName);
				}
				Entries.Add(MoveTemp(Entry));
			}
		}
	}

	// Sort entries by name
	Entries.Sort([](const FASCEntry& A, const FASCEntry& B)
	{
		return A.DisplayName < B.DisplayName;
	});

	// Build preview string for the combo
	FString PreviewStr = TEXT("(none)");
	if (SelectedASC.IsValid())
	{
		for (const FASCEntry& Entry : Entries)
		{
			if (Entry.ASC == SelectedASC)
			{
				PreviewStr = Entry.DisplayName;
				break;
			}
		}
	}

	ImGui::Text("Ability System Component (%d in world):", Entries.Num());
	ImGui::SetNextItemWidth(-1);
	if (ImGui::BeginCombo("##ASCSelect", TCHAR_TO_ANSI(*PreviewStr), ImGuiComboFlags_HeightLarge))
	{
		// None option
		if (ImGui::Selectable("(none)", !SelectedASC.IsValid()))
		{
			SelectedASC = nullptr;
		}

		for (const FASCEntry& Entry : Entries)
		{
			const bool bSelected = (Entry.ASC == SelectedASC);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Entry.DisplayName), bSelected))
			{
				SelectedASC = Entry.ASC;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	// Clear selection if ASC was destroyed
	if (SelectedASC.IsStale())
	{
		SelectedASC = nullptr;
	}
}

void FArcGameplayAbilitiesDebugger::DrawAbilitySystemDetails(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(InASC);
	const double WorldTime = InASC->GetWorld() ? InASC->GetWorld()->GetTimeSeconds() : 0.0;

	// ============ Overview Section ============
	if (ImGui::CollapsingHeader("Overview", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("OverviewTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 250.0f);
			ImGui::TableSetupColumn("Value");

			TableRow("Class", InASC->GetClass()->GetName());
			TableRow("Owner Actor", GetNameSafe(InASC->GetOwnerActor()));
			TableRow("Avatar Actor", GetNameSafe(InASC->GetAvatarActor()));
			TableRowBool("Is Net Simulated", InASC->IsNetSimulating());
			TableRowBool("Has Authority", InASC->IsOwnerActorAuthoritative());
			TableRowFloat("Last Activated Time", InASC->GetAbilityLastActivatedTime());

#if !UE_BUILD_SHIPPING
			if (ArcASC)
			{
				TableRowInt("Scope Lock Count", ArcASC->GetDebug_AbilityScopeLockCount());
			}
#endif

			ImGui::EndTable();
		}
	}

	// ============ Activatable Abilities Section ============
	const TArray<FGameplayAbilitySpec>& ActivatableAbilities = InASC->GetActivatableAbilities();

	if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*FString::Printf(TEXT("Activatable Abilities (%d)###Abilities"), ActivatableAbilities.Num())), ImGuiTreeNodeFlags_DefaultOpen))
	{
		// Filter
		static char AbilityFilter[256] = "";
		ImGui::InputText("Filter##AbilityFilter", AbilityFilter, IM_ARRAYSIZE(AbilityFilter));
		const FString FilterStr(AbilityFilter);

		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
		{
			UGameplayAbility* Ability = AbilitySpec.GetPrimaryInstance();
			UGameplayAbility* AbilityCDO = AbilitySpec.Ability;
			const UGameplayAbility* DisplayAbility = Ability ? Ability : AbilityCDO;
			if (!DisplayAbility)
			{
				continue;
			}

			UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Ability);

			FString AbilityName = GetNameSafe(DisplayAbility);
			if (ArcAbility)
			{
				const UArcItemDefinition* SourceItem = ArcAbility->GetSourceItemData();
				if (SourceItem)
				{
					AbilityName += FString::Printf(TEXT(" [%s]"), *GetNameSafe(SourceItem));
				}
			}

			// Apply filter
			if (FilterStr.Len() > 0 && !AbilityName.Contains(FilterStr))
			{
				continue;
			}

			// Color active abilities green
			const bool bIsActive = AbilitySpec.IsActive();
			if (bIsActive)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
			}

			FString TreeLabel = FString::Printf(TEXT("%s (Lvl %d)###Ability_%s"), *AbilityName, AbilitySpec.Level, *AbilitySpec.Handle.ToString());
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*TreeLabel)))
			{
				if (bIsActive)
				{
					ImGui::PopStyleColor();
				}

				if (ImGui::BeginTable("AbilityDetails", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 250.0f);
					ImGui::TableSetupColumn("Value");

					// -- Spec Info --
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Spec Info");

					TableRow("Handle", FString::Printf(TEXT("%s"), *AbilitySpec.Handle.ToString()));
					TableRow("Ability Class", GetNameSafe(AbilitySpec.Ability ? AbilitySpec.Ability->GetClass() : nullptr));
					TableRowInt("Level", AbilitySpec.Level);
					TableRowInt("Input ID", AbilitySpec.InputID);
					TableRow("Source Object", GetNameSafe(AbilitySpec.SourceObject.Get()));
					TableRowInt("Active Count", AbilitySpec.ActiveCount);
					TableRowBool("Input Pressed", AbilitySpec.InputPressed);
					TableRowBool("Remove After Activation", AbilitySpec.RemoveAfterActivation);
					TableRowBool("Pending Remove", AbilitySpec.PendingRemove);
					TableRowBool("Activate Once", AbilitySpec.bActivateOnce);

					// Dynamic Tags
					TableRowTags("Dynamic Ability Tags", AbilitySpec.DynamicAbilityTags);

					// SetByCaller magnitudes
					if (!AbilitySpec.SetByCallerTagMagnitudes.IsEmpty())
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						if (ImGui::TreeNode("SetByCaller Magnitudes"))
						{
							for (const auto& Pair : AbilitySpec.SetByCallerTagMagnitudes)
							{
								ImGui::BulletText("%s: %.4f", TCHAR_TO_ANSI(*Pair.Key.ToString()), Pair.Value);
							}
							ImGui::TreePop();
						}
					}

					// Instances
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::TreeNode("Instances"))
					{
						ImGui::Text("Non-Replicated: %d", AbilitySpec.NonReplicatedInstances.Num());
						for (int32 i = 0; i < AbilitySpec.NonReplicatedInstances.Num(); ++i)
						{
							ImGui::BulletText("[%d] %s", i, TCHAR_TO_ANSI(*GetNameSafe(AbilitySpec.NonReplicatedInstances[i])));
						}
						ImGui::Text("Replicated: %d", AbilitySpec.ReplicatedInstances.Num());
						for (int32 i = 0; i < AbilitySpec.ReplicatedInstances.Num(); ++i)
						{
							ImGui::BulletText("[%d] %s", i, TCHAR_TO_ANSI(*GetNameSafe(AbilitySpec.ReplicatedInstances[i])));
						}
						ImGui::TreePop();
					}

					if (Ability)
					{
						// -- Runtime State --
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Runtime State");

						TableRowBool("Is Active", Ability->IsActive());
						TableRowBool("Is Triggered", Ability->IsTriggered());
						TableRowBool("Is Predicting Client", Ability->IsPredictingClient());
						TableRowBool("Is For Remote Client", Ability->IsForRemoteClient());
						TableRowBool("Can Be Canceled", Ability->CanBeCanceled());
						TableRowBool("Is Blocking Other Abilities", Ability->IsBlockingOtherAbilities());

						// -- Activation Checks --
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Activation Checks");

						const FGameplayAbilityActorInfo* CurrentActorInfo = Ability->GetCurrentActorInfo();
						const bool bCanActivate = Ability->CanActivateAbility(AbilitySpec.Handle, CurrentActorInfo);
						TableRowBool("Can Activate Ability", bCanActivate);

						const bool bSatisfiesTags = Ability->DoesAbilitySatisfyTagRequirements(*InASC);
						TableRowBool("Satisfies Tag Requirements", bSatisfiesTags);

						// -- Tag Details --
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Tags");

						TableRowTags("Asset Tags", Ability->GetAssetTags());

						if (ArcAbility)
						{
							const FGameplayTagContainer& OwnedTags = InASC->GetOwnedGameplayTags();
							const FGameplayTagContainer& BlockedAbilityTags = InASC->GetBlockedAbilityTags();

							// Blocked check
							const FGameplayTagContainer& AbilityAssetTags = ArcAbility->GetAssetTags();
							const bool bASCBlocks = !AbilityAssetTags.IsEmpty() && !BlockedAbilityTags.IsEmpty() && AbilityAssetTags.HasAny(BlockedAbilityTags);
							TableRowBool("ASC Has Blocking Tags", bASCBlocks);

							TableRowTags("Activation Required Tags", ArcAbility->GetActivationRequiredTags());
							const bool bMissingRequired = !ArcAbility->GetActivationRequiredTags().IsEmpty()
								&& !OwnedTags.HasAll(ArcAbility->GetActivationRequiredTags());
							TableRowBool("Missing Required Tags", bMissingRequired);

							TableRowTags("Activation Blocked Tags", ArcAbility->GetActivationBlockedTags());
							const bool bHasBlockedTags = !ArcAbility->GetActivationBlockedTags().IsEmpty()
								&& OwnedTags.HasAny(ArcAbility->GetActivationBlockedTags());
							TableRowBool("Has Blocked Tags", bHasBlockedTags);

							TableRowTags("Activation Owned Tags", ArcAbility->GetActivationOwnedTags());
							TableRowTags("Cancel Abilities With Tags", ArcAbility->GetCancelAbilitiesWithTag());
							TableRowTags("Block Abilities With Tags", ArcAbility->GetBlockAbilitiesWithTag());
							TableRowTags("Source Required Tags", ArcAbility->GetSourceRequiredTags());
							TableRowTags("Source Blocked Tags", ArcAbility->GetSourceBlockedTags());
							TableRowTags("Target Required Tags", ArcAbility->GetTargetRequiredTags());
							TableRowTags("Target Blocked Tags", ArcAbility->GetTargetBlockedTags());
							TableRowTags("Custom Cooldown Tags", ArcAbility->GetCustomCooldownTags());

							// -- Cooldown & Activation Time --
#if !UE_BUILD_SHIPPING
							if (ArcASC)
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Cooldown & Timing");

								const bool bOnCooldown = ArcASC->IsAbilityOnCooldown(AbilitySpec.Handle);
								TableRowBool("Is On Cooldown", bOnCooldown);
								if (bOnCooldown)
								{
									TableRowFloat("Cooldown Remaining", ArcASC->GetRemainingTime(AbilitySpec.Handle));
								}

								const double ActivationStartTime = ArcASC->GetAbilityActivationStartTime(AbilitySpec.Handle);
								if (ActivationStartTime >= 0)
								{
									TableRowFloat("Activation Start Time", static_cast<float>(ActivationStartTime));
									TableRowFloat("Time Since Activation", static_cast<float>(WorldTime - ActivationStartTime));
								}

								TableRowBool("Activate On Trigger", ArcAbility->GetActivateOnTrigger());
							}
#endif

							// -- Item Info --
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Item Info");

							const UArcItemDefinition* SourceItemDef = ArcAbility->GetSourceItemData();
							TableRow("Source Item Def", GetNameSafe(SourceItemDef));

							UArcItemsStoreComponent* SourceItemsStore = ArcAbility->GetItemsStoreComponent(nullptr);
							TableRow("Items Store", GetNameSafe(SourceItemsStore));

							const FArcItemData* ItemData = ArcAbility->GetSourceItemEntryPtr();
							TableRowBool("Has Valid Item Data", ItemData != nullptr);

							const FArcItemId ItemId = ArcAbility->GetSourceItemHandle();
							TableRow("Source Item Id", ItemId.ToString());

							TableRow("Current Item Slot", ArcAbility->GetCurrentItemSlot().ToString());
						}
					}
					else
					{
						// No instanced ability, show CDO info
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextDisabled("(No instanced ability - showing CDO info only)");

						if (AbilityCDO)
						{
							TableRow("CDO Class", AbilityCDO->GetClass()->GetName());
							TableRowTags("Asset Tags", AbilityCDO->GetAssetTags());
						}
					}

					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
			else
			{
				if (bIsActive)
				{
					ImGui::PopStyleColor();
				}
			}
		}
	}

	// ============ ASC Owned Tags ============
	if (ImGui::CollapsingHeader("Owned Gameplay Tags"))
	{
		const FGameplayTagContainer& OwnedTags = InASC->GetOwnedGameplayTags();
		if (OwnedTags.IsEmpty())
		{
			ImGui::TextDisabled("(no tags)");
		}
		else
		{
			ImGui::Text("Count: %d", OwnedTags.Num());

			if (ArcASC)
			{
				const FGameplayTagCountContainer& TagCountContainer = ArcASC->GetGameplayTagCountContainer();
				const TMap<FGameplayTag, int32>& TagCountMap = DebugHack::GetPrivateGameplayTagCountMap(&TagCountContainer);

				if (ImGui::BeginTable("OwnedTagsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable))
				{
					ImGui::TableSetupColumn("Tag");
					ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
					ImGui::TableHeadersRow();

					for (const FGameplayTag& Tag : OwnedTags)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", TCHAR_TO_ANSI(*Tag.ToString()));
						ImGui::TableSetColumnIndex(1);
						if (const int32* Count = TagCountMap.Find(Tag))
						{
							ImGui::Text("%d", *Count);
						}
						else
						{
							ImGui::TextDisabled("?");
						}
					}
					ImGui::EndTable();
				}
			}
			else
			{
				for (const FGameplayTag& Tag : OwnedTags)
				{
					ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
				}
			}
		}
	}

	// ============ Blocked Ability Tags ============
	if (ImGui::CollapsingHeader("Blocked Ability Tags"))
	{
		const FGameplayTagContainer BlockedTags = InASC->GetBlockedAbilityTags();
		if (BlockedTags.IsEmpty())
		{
			ImGui::TextDisabled("(none)");
		}
		else
		{
			for (const FGameplayTag& Tag : BlockedTags)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
			}
		}
	}

	// ============ Attribute Sets ============
	if (ImGui::CollapsingHeader("Attribute Sets"))
	{
		const TArray<UAttributeSet*>& AttributeSets = InASC->GetSpawnedAttributes();
		if (AttributeSets.IsEmpty())
		{
			ImGui::TextDisabled("(no attribute sets)");
		}
		else
		{
			ImGui::Text("Attribute Sets: %d", AttributeSets.Num());
			for (UAttributeSet* AttribSet : AttributeSets)
			{
				if (!AttribSet)
				{
					continue;
				}

				FString SetName = AttribSet->GetClass()->GetName();
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*SetName)))
				{
					if (ImGui::BeginTable("AttrTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
					{
						ImGui::TableSetupColumn("Attribute");
						ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 100.0f);
						ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.0f);
						ImGui::TableHeadersRow();

						for (TFieldIterator<FProperty> It(AttribSet->GetClass()); It; ++It)
						{
							FStructProperty* StructProp = CastField<FStructProperty>(*It);
							if (!StructProp || !StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
							{
								continue;
							}

							const FGameplayAttributeData* Attribute = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttribSet);
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text("%s", TCHAR_TO_ANSI(*StructProp->GetName()));
							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%.2f", Attribute->GetBaseValue());
							ImGui::TableSetColumnIndex(2);
							ImGui::Text("%.2f", Attribute->GetCurrentValue());
						}
						ImGui::EndTable();
					}
					ImGui::TreePop();
				}
			}
		}
	}

	// ============ Active Gameplay Effects (Summary) ============
	if (ImGui::CollapsingHeader("Active Gameplay Effects"))
	{
		const FActiveGameplayEffectsContainer& ActiveGEContainer = InASC->GetActiveGameplayEffects();
		const TArray<FActiveGameplayEffectHandle> AllHandles = ActiveGEContainer.GetAllActiveEffectHandles();

		ImGui::Text("Active Effects: %d", AllHandles.Num());

		for (const FActiveGameplayEffectHandle& Handle : AllHandles)
		{
			const FActiveGameplayEffect* ActiveGE = InASC->GetActiveGameplayEffect(Handle);
			if (!ActiveGE)
			{
				continue;
			}

			const UGameplayEffect* GEDef = ActiveGE->Spec.Def;
			FString EffectLabel = FString::Printf(TEXT("%s (Stacks: %d)###GE_%s"),
				*GetNameSafe(GEDef), ActiveGE->Spec.GetStackCount(), *Handle.ToString());

			if (ActiveGE->bIsInhibited)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			}

			if (ImGui::TreeNode(TCHAR_TO_ANSI(*EffectLabel)))
			{
				if (ActiveGE->bIsInhibited)
				{
					ImGui::PopStyleColor();
				}

				if (ImGui::BeginTable("GEDetails", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 200.0f);
					ImGui::TableSetupColumn("Value");

					TableRowBool("Is Inhibited", ActiveGE->bIsInhibited);
					TableRowInt("Stack Count", ActiveGE->Spec.GetStackCount());
					TableRowFloat("Duration", ActiveGE->Spec.GetDuration());
					TableRowFloat("Period", ActiveGE->Spec.GetPeriod());
					TableRowFloat("Level", ActiveGE->Spec.GetLevel());
					TableRowFloat("End Time", ActiveGE->GetEndTime());
					TableRowFloat("Time Remaining", ActiveGE->GetTimeRemaining(WorldTime));

					// Context
					TableRow("Effect Causer", GetNameSafe(ActiveGE->Spec.GetEffectContext().GetEffectCauser()));
					TableRow("Instigator", GetNameSafe(ActiveGE->Spec.GetEffectContext().GetInstigator()));
					TableRow("Source Object", GetNameSafe(ActiveGE->Spec.GetEffectContext().GetSourceObject()));
					TableRow("Ability", GetNameSafe(ActiveGE->Spec.GetEffectContext().GetAbility()));

					// Tags
					{
						FGameplayTagContainer GrantedTags;
						ActiveGE->Spec.GetAllGrantedTags(GrantedTags);
						TableRowTags("Granted Tags", GrantedTags);
					}

					// Modifiers
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::TreeNode("Modifiers"))
					{
						for (int32 Idx = 0; Idx < ActiveGE->Spec.Modifiers.Num(); Idx++)
						{
							ImGui::Text("[%d] Magnitude: %.4f", Idx, ActiveGE->Spec.GetModifierMagnitude(Idx));
						}
						ImGui::TreePop();
					}

					// Modified Attributes
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::TreeNode("Modified Attributes"))
					{
						for (const FGameplayEffectModifiedAttribute& Mod : ActiveGE->Spec.ModifiedAttributes)
						{
							ImGui::BulletText("%s: %.4f", TCHAR_TO_ANSI(*Mod.Attribute.GetName()), Mod.TotalMagnitude);
						}
						ImGui::TreePop();
					}

					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
			else
			{
				if (ActiveGE->bIsInhibited)
				{
					ImGui::PopStyleColor();
				}
			}
		}
	}

	// ============ Animation Montages ============
	if (ImGui::CollapsingHeader("Animation Montages"))
	{
#if !UE_BUILD_SHIPPING
		if (ArcASC)
		{
			const FGameplayAbilityLocalAnimMontage& LocalMontage = ArcASC->GetDebug_LocalAnimMontageInfo();
			if (ImGui::BeginTable("MontageTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 200.0f);
				ImGui::TableSetupColumn("Value");

				TableRow("Local Anim Montage", GetNameSafe(LocalMontage.AnimMontage));
				TableRowInt("Play Instance ID", LocalMontage.PlayInstanceId);
				TableRow("Animating Ability", GetNameSafe(LocalMontage.AnimatingAbility.Get()));

				ImGui::EndTable();
			}

			// Montage-to-Item mappings
			if (!ArcASC->MontageToItemDef.IsEmpty())
			{
				if (ImGui::TreeNode("Montage -> Item Definition Mappings"))
				{
					for (const auto& Pair : ArcASC->MontageToItemDef)
					{
						ImGui::BulletText("%s -> %s",
							TCHAR_TO_ANSI(*GetNameSafe(Pair.Key.Get())),
							TCHAR_TO_ANSI(*GetNameSafe(Pair.Value.Get())));
					}
					ImGui::TreePop();
				}
			}
		}
		else
#endif
		{
			ImGui::TextDisabled("(Montage details require ArcCoreAbilitySystemComponent)");
		}
	}

	// ============ Triggered Abilities ============
#if !UE_BUILD_SHIPPING
	if (ArcASC && ImGui::CollapsingHeader("Triggered Abilities"))
	{
		const auto& EventTriggered = ArcASC->GetDebug_GameplayEventTriggeredAbilities();
		const auto& TagTriggered = ArcASC->GetDebug_OwnedTagTriggeredAbilities();

		if (ImGui::TreeNode("Event-Triggered Abilities"))
		{
			if (EventTriggered.IsEmpty())
			{
				ImGui::TextDisabled("(none)");
			}
			for (const auto& Pair : EventTriggered)
			{
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*Pair.Key.ToString())))
				{
					for (const FGameplayAbilitySpecHandle& Handle : Pair.Value)
					{
						const FGameplayAbilitySpec* Spec = InASC->FindAbilitySpecFromHandle(Handle);
						ImGui::BulletText("%s (Handle: %s)",
							TCHAR_TO_ANSI(*GetNameSafe(Spec ? Spec->Ability.Get() : nullptr)),
							TCHAR_TO_ANSI(*Handle.ToString()));
					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Tag-Triggered Abilities"))
		{
			if (TagTriggered.IsEmpty())
			{
				ImGui::TextDisabled("(none)");
			}
			for (const auto& Pair : TagTriggered)
			{
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*Pair.Key.ToString())))
				{
					for (const FGameplayAbilitySpecHandle& Handle : Pair.Value)
					{
						const FGameplayAbilitySpec* Spec = InASC->FindAbilitySpecFromHandle(Handle);
						ImGui::BulletText("%s (Handle: %s)",
							TCHAR_TO_ANSI(*GetNameSafe(Spec ? Spec->Ability.Get() : nullptr)),
							TCHAR_TO_ANSI(*Handle.ToString()));
					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	}
#endif

	// ============ Input State ============
#if !UE_BUILD_SHIPPING
	if (ArcASC && ImGui::CollapsingHeader("Input State"))
	{
		const auto& PressedHandles = ArcASC->GetDebug_InputPressedSpecHandles();
		const auto& HeldHandles = ArcASC->GetDebug_InputHeldSpecHandles();

		ImGui::Text("Pressed this frame: %d", PressedHandles.Num());
		for (const FGameplayAbilitySpecHandle& Handle : PressedHandles)
		{
			const FGameplayAbilitySpec* Spec = InASC->FindAbilitySpecFromHandle(Handle);
			ImGui::BulletText("%s", TCHAR_TO_ANSI(*GetNameSafe(Spec ? Spec->Ability.Get() : nullptr)));
		}

		ImGui::Text("Held: %d", HeldHandles.Num());
		for (const FGameplayAbilitySpecHandle& Handle : HeldHandles)
		{
			const FGameplayAbilitySpec* Spec = InASC->FindAbilitySpecFromHandle(Handle);
			ImGui::BulletText("%s", TCHAR_TO_ANSI(*GetNameSafe(Spec ? Spec->Ability.Get() : nullptr)));
		}
	}
#endif

	// ============ Cooldowns ============
#if !UE_BUILD_SHIPPING
	if (ArcASC && ImGui::CollapsingHeader("Cooldowns"))
	{
		const auto& CooldownMap = ArcASC->GetDebug_AbilitiesOnCooldown();
		if (CooldownMap.IsEmpty())
		{
			ImGui::TextDisabled("(no abilities on cooldown)");
		}
		else
		{
			if (ImGui::BeginTable("CooldownTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Ability");
				ImGui::TableSetupColumn("Remaining", ImGuiTableColumnFlags_WidthFixed, 100.0f);
				ImGui::TableHeadersRow();

				for (const auto& Pair : CooldownMap)
				{
					const FGameplayAbilitySpec* Spec = InASC->FindAbilitySpecFromHandle(Pair.Key);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(Spec ? Spec->Ability.Get() : nullptr)));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.2f", ArcASC->GetRemainingTime(Pair.Key));
				}
				ImGui::EndTable();
			}
		}
	}
#endif

	// ============ Spawned Actors ============
#if !UE_BUILD_SHIPPING
	if (ArcASC && ImGui::CollapsingHeader("Spawned Ability Actors"))
	{
		const auto& SpawnedActors = ArcASC->GetDebug_SpawnedActors();
		if (SpawnedActors.IsEmpty())
		{
			ImGui::TextDisabled("(no spawned actors)");
		}
		else
		{
			ImGui::Text("Spawned Actors: %d", SpawnedActors.Num());
			for (const auto& Pair : SpawnedActors)
			{
				AActor* Actor = Pair.Value.Get();
				FString Label = FString::Printf(TEXT("%s (Handle: %d, Valid: %s)"),
					*GetNameSafe(Actor), Pair.Key.Handle, BoolToText(Actor != nullptr));
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Label));
			}
		}
	}
#endif

	// ============ Targeting Presets ============
	if (ArcASC && ImGui::CollapsingHeader("Global Targeting Presets"))
	{
		const auto& Presets = ArcASC->GetTargetingPresets();
		if (Presets.IsEmpty())
		{
			ImGui::TextDisabled("(no targeting presets)");
		}
		else
		{
			ImGui::Text("Presets: %d", Presets.Num());
			for (const auto& Pair : Presets)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Pair.Key.ToString()));
			}
		}
	}
}
