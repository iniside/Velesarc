// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayEffectsDebugger.h"

#include "ArcDebuggerHacks.h"
#include "ArcGameplayEffectContext.h"
#include "imgui.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
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

	void DrawTagRequirements(const char* Label, const FGameplayTagRequirements& Reqs)
	{
		if (ImGui::TreeNode(Label))
		{
			if (!Reqs.RequireTags.IsEmpty())
			{
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Require Tags:");
				for (const FGameplayTag& Tag : Reqs.RequireTags)
				{
					ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
				}
			}
			if (!Reqs.IgnoreTags.IsEmpty())
			{
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Ignore Tags:");
				for (const FGameplayTag& Tag : Reqs.IgnoreTags)
				{
					ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
				}
			}
			if (Reqs.RequireTags.IsEmpty() && Reqs.IgnoreTags.IsEmpty())
			{
				ImGui::TextDisabled("(none)");
			}
			ImGui::TreePop();
		}
	}

	const char* DurationPolicyToString(EGameplayEffectDurationType Type)
	{
		switch (Type)
		{
		case EGameplayEffectDurationType::Instant: return "Instant";
		case EGameplayEffectDurationType::Infinite: return "Infinite";
		case EGameplayEffectDurationType::HasDuration: return "Has Duration";
		default: return "Unknown";
		}
	}

	const char* StackingTypeToString(EGameplayEffectStackingType Type)
	{
		switch (Type)
		{
		case EGameplayEffectStackingType::None: return "None";
		case EGameplayEffectStackingType::AggregateBySource: return "Aggregate By Source";
		case EGameplayEffectStackingType::AggregateByTarget: return "Aggregate By Target";
		default: return "Unknown";
		}
	}
}

void FArcGameplayEffectsDebugger::Initialize()
{
}

void FArcGameplayEffectsDebugger::Uninitialize()
{
	SelectedASC = nullptr;
}

void FArcGameplayEffectsDebugger::Draw()
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

	ImGui::SetNextWindowSize(ImVec2(750, 650), ImGuiCond_FirstUseEver);
	ImGui::Begin("Gameplay Effects Debugger", &bShow);

	DrawASCSelector();

	ImGui::Separator();

	UAbilitySystemComponent* ASC = SelectedASC.Get();
	if (ASC)
	{
		DrawEffectsDetails(ASC);
	}
	else
	{
		ImGui::TextDisabled("Select an Ability System Component above.");
	}

	ImGui::End();
}

void FArcGameplayEffectsDebugger::DrawASCSelector()
{
	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}

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

		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				FASCEntry Entry;
				Entry.ASC = ASC;

				AActor* OwnerActor = ASC->GetOwnerActor();
				AActor* AvatarActor = ASC->GetAvatarActor();
				FString OwnerName = GetNameSafe(OwnerActor);
				FString ClassName = ASC->GetClass()->GetName();

				if (OwnerActor == AvatarActor || !AvatarActor)
				{
					Entry.DisplayName = FString::Printf(TEXT("[%s] %s"), *ClassName, *OwnerName);
				}
				else
				{
					Entry.DisplayName = FString::Printf(TEXT("[%s] %s (Avatar: %s)"), *ClassName, *OwnerName, *GetNameSafe(AvatarActor));
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
					Entries.Add(MoveTemp(Entry));
				}
			}
		}

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
				Entry.DisplayName = FString::Printf(TEXT("[%s] %s"), *ASC->GetClass()->GetName(), *GetNameSafe(OwnerActor));
				Entries.Add(MoveTemp(Entry));
			}
		}
	}

	Entries.Sort([](const FASCEntry& A, const FASCEntry& B) { return A.DisplayName < B.DisplayName; });

	UAbilitySystemComponent* CurrentASC = SelectedASC.Get();
	FString CurrentLabel = TEXT("(None)");
	for (const FASCEntry& Entry : Entries)
	{
		if (Entry.ASC == CurrentASC)
		{
			CurrentLabel = Entry.DisplayName;
			break;
		}
	}

	ImGui::Text("ASC:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1);
	if (ImGui::BeginCombo("##ASCSelector_Effects", TCHAR_TO_ANSI(*CurrentLabel)))
	{
		if (ImGui::Selectable("(None)", !CurrentASC))
		{
			SelectedASC = nullptr;
		}
		for (const FASCEntry& Entry : Entries)
		{
			bool bSelected = (Entry.ASC == CurrentASC);
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
}

void FArcGameplayEffectsDebugger::DrawEffectsDetails(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	const double WorldTime = InASC->GetWorld()->GetTimeSeconds();
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(InASC);

	// ============ Overview ============
	if (ImGui::CollapsingHeader("Overview", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("GEOverview", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 200.0f);
			ImGui::TableSetupColumn("Value");

			TableRow("ASC Class", InASC->GetClass()->GetName());
			TableRow("Owner Actor", GetNameSafe(InASC->GetOwnerActor()));
			TableRow("Avatar Actor", GetNameSafe(InASC->GetAvatarActor()));

			const FActiveGameplayEffectsContainer& ActiveGEContainer = InASC->GetActiveGameplayEffects();
			const TArray<FActiveGameplayEffectHandle> AllHandles = ActiveGEContainer.GetAllActiveEffectHandles();
			TableRowInt("Total Active Effects", AllHandles.Num());

			int32 InhibitedCount = 0;
			for (const FActiveGameplayEffectHandle& Handle : AllHandles)
			{
				if (const FActiveGameplayEffect* AGE = InASC->GetActiveGameplayEffect(Handle))
				{
					if (AGE->bIsInhibited)
					{
						InhibitedCount++;
					}
				}
			}
			TableRowInt("Inhibited Effects", InhibitedCount);

			TableRowInt("Attribute Sets", InASC->GetSpawnedAttributes().Num());

			ImGui::EndTable();
		}
	}

	// ============ Gameplay Tag State ============
	if (ImGui::CollapsingHeader("Gameplay Tag State"))
	{
		if (ArcASC)
		{
			const FGameplayTagCountContainer& Tags = ArcASC->GetGameplayTagCountContainer();

			if (ImGui::TreeNode("Owned Tags (with counts)"))
			{
				const TMap<FGameplayTag, int32> GameplayTagCountMap = DebugHack::GetPrivateGameplayTagCountMap(&Tags);
				if (GameplayTagCountMap.Num() == 0)
				{
					ImGui::TextDisabled("(none)");
				}
				else
				{
					if (ImGui::BeginTable("OwnedTags", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable))
					{
						ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
						ImGui::TableHeadersRow();

						for (const auto& Pair : GameplayTagCountMap)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text("%s", TCHAR_TO_ANSI(*Pair.Key.ToString()));
							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%d", Pair.Value);
						}
						ImGui::EndTable();
					}
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Explicit Tags"))
			{
				const FGameplayTagContainer& ExplicitTags = Tags.GetExplicitGameplayTags();
				if (ExplicitTags.IsEmpty())
				{
					ImGui::TextDisabled("(none)");
				}
				else
				{
					for (const FGameplayTag& Tag : ExplicitTags)
					{
						ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
					}
				}
				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TextDisabled("(Tag details require ArcCoreAbilitySystemComponent)");
		}

		if (ArcASC)
		{
			if (ImGui::TreeNode("Minimal Replicated Tags"))
			{
				const FMinimalReplicationTagCountMap& MinTags = ArcASC->GetMinimalReplicationCountTags();
				if (MinTags.TagMap.Num() == 0)
				{
					ImGui::TextDisabled("(none)");
				}
				else
				{
					for (const auto& Pair : MinTags.TagMap)
					{
						ImGui::BulletText("%s : %d", TCHAR_TO_ANSI(*Pair.Key.ToString()), Pair.Value);
					}
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Loose Replicated Tags"))
			{
				const FMinimalReplicationTagCountMap& LooseTags = ArcASC->GetReplicatedLooseCountTags();
				if (LooseTags.TagMap.Num() == 0)
				{
					ImGui::TextDisabled("(none)");
				}
				else
				{
					for (const auto& Pair : LooseTags.TagMap)
					{
						ImGui::BulletText("%s : %d", TCHAR_TO_ANSI(*Pair.Key.ToString()), Pair.Value);
					}
				}
				ImGui::TreePop();
			}
		}
	}

	// ============ Active Gameplay Effects ============
	if (ImGui::CollapsingHeader("Active Gameplay Effects", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FActiveGameplayEffectsContainer& ActiveGEContainer = InASC->GetActiveGameplayEffects();
		const TArray<FActiveGameplayEffectHandle> AllHandles = ActiveGEContainer.GetAllActiveEffectHandles();

		ImGui::Text("Active Effects: %d", AllHandles.Num());
		ImGui::Separator();

		static char EffectFilter[256] = "";
		ImGui::SetNextItemWidth(300);
		ImGui::InputText("Filter Effects##GEFilter", EffectFilter, IM_ARRAYSIZE(EffectFilter));
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear##GEFilterClear"))
		{
			EffectFilter[0] = '\0';
		}

		FString FilterStr(EffectFilter);

		for (const FActiveGameplayEffectHandle& Handle : AllHandles)
		{
			const FActiveGameplayEffect* ActiveGE = InASC->GetActiveGameplayEffect(Handle);
			if (!ActiveGE)
			{
				continue;
			}

			const UGameplayEffect* GEDef = ActiveGE->Spec.Def;
			FString EffectName = GetNameSafe(GEDef);

			// Apply filter
			if (!FilterStr.IsEmpty() && !EffectName.Contains(FilterStr))
			{
				continue;
			}

			FString EffectLabel = FString::Printf(TEXT("%s (Stacks: %d)###GE_%s"),
				*EffectName, ActiveGE->Spec.GetStackCount(), *Handle.ToString());

			// Color inhibited effects
			bool bInhibited = ActiveGE->bIsInhibited;
			if (bInhibited)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			}

			if (ImGui::TreeNode(TCHAR_TO_ANSI(*EffectLabel)))
			{
				if (bInhibited)
				{
					ImGui::PopStyleColor();
				}

				// ---- Basic Info ----
				if (ImGui::BeginTable("GEBasic", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 200.0f);
					ImGui::TableSetupColumn("Value");

					TableRow("Effect Class", EffectName);
					TableRowBool("Is Inhibited", bInhibited);
					TableRowInt("Stack Count", ActiveGE->Spec.GetStackCount());

					if (GEDef)
					{
						TableRow("Duration Policy", FString(DurationPolicyToString(GEDef->DurationPolicy)));
					}

					TableRowFloat("Duration", ActiveGE->Spec.GetDuration());
					TableRowFloat("Period", ActiveGE->Spec.GetPeriod());
					TableRowFloat("Level", ActiveGE->Spec.GetLevel());

					float TimeRemaining = ActiveGE->GetTimeRemaining(WorldTime);
					if (ActiveGE->Spec.GetDuration() > 0.0f)
					{
						TableRowFloat("Time Remaining", TimeRemaining);
						TableRowFloat("End Time", ActiveGE->GetEndTime());
					}

					TableRowFloat("Start World Time", ActiveGE->StartWorldTime);
					TableRowFloat("Start Server World Time", ActiveGE->StartServerWorldTime);
					TableRowBool("Pending Remove", ActiveGE->IsPendingRemove);

					ImGui::EndTable();
				}

				// ---- Stacking Info ----
				if (GEDef && GEDef->GetStackingType() != EGameplayEffectStackingType::None)
				{
					if (ImGui::TreeNode("Stacking"))
					{
						if (ImGui::BeginTable("GEStacking", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
						{
							ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 200.0f);
							ImGui::TableSetupColumn("Value");

							TableRow("Stacking Type", FString(StackingTypeToString(GEDef->GetStackingType())));
							TableRowInt("Stack Limit", GEDef->GetStackLimitCount());
							TableRowInt("Current Stacks", ActiveGE->Spec.GetStackCount());
							TableRow("Duration Refresh", *UEnum::GetValueAsString(GEDef->StackDurationRefreshPolicy));
							TableRow("Period Reset", *UEnum::GetValueAsString(GEDef->StackPeriodResetPolicy));
							TableRow("Expiration Policy", *UEnum::GetValueAsString(GEDef->StackExpirationPolicy));

							ImGui::EndTable();
						}
						ImGui::TreePop();
					}
				}

				// ---- Tags ----
				if (ImGui::TreeNode("Tags"))
				{
					FGameplayTagContainer GrantedTags;
					ActiveGE->Spec.GetAllGrantedTags(GrantedTags);
					TableRowTags("Granted Tags", GrantedTags);

					FGameplayTagContainer BlockedTags;
					ActiveGE->Spec.GetAllBlockedAbilityTags(BlockedTags);
					TableRowTags("Blocked Ability Tags", BlockedTags);

					FGameplayTagContainer AssetTags;
					ActiveGE->Spec.GetAllAssetTags(AssetTags);
					TableRowTags("Asset Tags", AssetTags);

					TableRowTags("Dynamic Granted Tags", ActiveGE->Spec.DynamicGrantedTags);

					// Show captured source/target tags
					if (ImGui::TreeNode("Captured Source Tags"))
					{
						const FGameplayTagContainer* SpecSourceTags = ActiveGE->Spec.CapturedSourceTags.GetAggregatedTags();
						if (SpecSourceTags && !SpecSourceTags->IsEmpty())
						{
							for (const FGameplayTag& Tag : *SpecSourceTags)
							{
								ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
							}
						}
						else
						{
							ImGui::TextDisabled("(none)");
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Captured Target Tags"))
					{
						const FGameplayTagContainer* SpecTargetTags = ActiveGE->Spec.CapturedTargetTags.GetAggregatedTags();
						if (SpecTargetTags && !SpecTargetTags->IsEmpty())
						{
							for (const FGameplayTag& Tag : *SpecTargetTags)
							{
								ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
							}
						}
						else
						{
							ImGui::TextDisabled("(none)");
						}
						ImGui::TreePop();
					}

					ImGui::TreePop();
				}

				// ---- Tag Requirements (from GE Components) ----
				if (GEDef)
				{
					if (const UTargetTagRequirementsGameplayEffectComponent* TagReqComp = GEDef->FindComponent<UTargetTagRequirementsGameplayEffectComponent>())
					{
						if (ImGui::TreeNode("Tag Requirements"))
						{
							DrawTagRequirements("Application Requirements", TagReqComp->ApplicationTagRequirements);
							DrawTagRequirements("Ongoing Requirements", TagReqComp->OngoingTagRequirements);
							DrawTagRequirements("Removal Requirements", TagReqComp->RemovalTagRequirements);

							ImGui::TreePop();
						}
					}
				}

				// ---- Modifiers ----
				if (ImGui::TreeNode("Modifiers"))
				{
					if (GEDef && GEDef->Modifiers.Num() > 0)
					{
						for (int32 Idx = 0; Idx < GEDef->Modifiers.Num(); Idx++)
						{
							const FGameplayModifierInfo& ModInfo = GEDef->Modifiers[Idx];
							float Magnitude = (Idx < ActiveGE->Spec.Modifiers.Num())
								? ActiveGE->Spec.GetModifierMagnitude(Idx)
								: 0.0f;

							FString ModLabel = FString::Printf(TEXT("[%d] %s %s %.4f###Mod_%d_%s"),
								Idx,
								*ModInfo.Attribute.GetName(),
								*UEnum::GetValueAsString(ModInfo.ModifierOp),
								Magnitude,
								Idx,
								*Handle.ToString());

							if (ImGui::TreeNode(TCHAR_TO_ANSI(*ModLabel)))
							{
								if (ImGui::BeginTable("ModDetail", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
								{
									ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
									ImGui::TableSetupColumn("Value");

									TableRow("Attribute", ModInfo.Attribute.GetName());
									TableRow("Operation", *UEnum::GetValueAsString(ModInfo.ModifierOp));
									TableRowFloat("Magnitude", Magnitude);
									TableRow("Evaluation Channel", *UEnum::GetValueAsString(ModInfo.EvaluationChannelSettings.GetEvaluationChannel()));

									ImGui::EndTable();
								}

								DrawTagRequirements("Source Tags", ModInfo.SourceTags);
								DrawTagRequirements("Target Tags", ModInfo.TargetTags);

								ImGui::TreePop();
							}
						}
					}
					else
					{
						ImGui::TextDisabled("(no modifiers)");
					}
					ImGui::TreePop();
				}

				// ---- Modified Attributes ----
				if (ImGui::TreeNode("Modified Attributes"))
				{
					if (ActiveGE->Spec.ModifiedAttributes.Num() > 0)
					{
						if (ImGui::BeginTable("ModAttrs", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
						{
							ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthStretch);
							ImGui::TableSetupColumn("Total Magnitude", ImGuiTableColumnFlags_WidthFixed, 120.0f);
							ImGui::TableHeadersRow();

							for (const FGameplayEffectModifiedAttribute& ModAttr : ActiveGE->Spec.ModifiedAttributes)
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("%s", TCHAR_TO_ANSI(*ModAttr.Attribute.GetName()));
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%.4f", ModAttr.TotalMagnitude);
							}
							ImGui::EndTable();
						}
					}
					else
					{
						ImGui::TextDisabled("(none)");
					}
					ImGui::TreePop();
				}

				// ---- SetByCaller Magnitudes ----
				{
					bool bHasSetByCaller = false;

					// Tag-based SetByCaller
					for (auto It = ActiveGE->Spec.SetByCallerTagMagnitudes.CreateConstIterator(); It; ++It)
					{
						if (!bHasSetByCaller)
						{
							if (ImGui::TreeNode("SetByCaller Magnitudes"))
							{
								bHasSetByCaller = true;
							}
							else
							{
								break;
							}
						}
						ImGui::BulletText("%s: %.4f", TCHAR_TO_ANSI(*It.Key().ToString()), It.Value());
					}

					if (bHasSetByCaller)
					{
						ImGui::TreePop();
					}
				}

				// ---- Effect Context ----
				if (ImGui::TreeNode("Effect Context"))
				{
					const FGameplayEffectContextHandle& CtxHandle = ActiveGE->Spec.GetEffectContext();
					if (CtxHandle.IsValid())
					{
						if (ImGui::BeginTable("GEContext", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
						{
							ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
							ImGui::TableSetupColumn("Value");

							TableRow("Effect Causer", GetNameSafe(CtxHandle.GetEffectCauser()));
							TableRow("Instigator", GetNameSafe(CtxHandle.GetInstigator()));
							TableRow("Original Instigator", GetNameSafe(CtxHandle.GetOriginalInstigator()));
							TableRow("Source Object", GetNameSafe(CtxHandle.GetSourceObject()));
							TableRow("Ability", GetNameSafe(CtxHandle.GetAbility()));
							TableRowInt("Ability Level", CtxHandle.GetAbilityLevel());

							if (CtxHandle.GetHitResult())
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								if (ImGui::TreeNode("Hit Result"))
								{
									ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*CtxHandle.GetHitResult()->ToString()));
									ImGui::TreePop();
								}
							}

							ImGui::EndTable();
						}

						// Context actors
						if (CtxHandle.Get())
						{
							const TArray<TWeakObjectPtr<AActor>> Actors = CtxHandle.Get()->GetActors();
							if (Actors.Num() > 0)
							{
								if (ImGui::TreeNode("Context Actors"))
								{
									for (const TWeakObjectPtr<AActor>& Actor : Actors)
									{
										ImGui::BulletText("%s", TCHAR_TO_ANSI(*GetNameSafe(Actor.Get())));
									}
									ImGui::TreePop();
								}
							}
						}

						// Arc-specific context
						if (CtxHandle.Get() && CtxHandle.Get()->GetScriptStruct()->IsChildOf(FArcGameplayEffectContext::StaticStruct()))
						{
							const FArcGameplayEffectContext* ArcCtx = static_cast<const FArcGameplayEffectContext*>(CtxHandle.Get());

							if (ImGui::TreeNode("Arc Context"))
							{
								if (ImGui::BeginTable("ArcCtx", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
								{
									ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
									ImGui::TableSetupColumn("Value");

									TableRow("Source Item Handle", ArcCtx->GetSourceItemHandle().ToString());
									TableRowBool("Has Valid Item Data", ArcCtx->GetSourceItemPtr() != nullptr);
									TableRow("Source Item Def", GetNameSafe(ArcCtx->GetSourceItem()));

									ImGui::EndTable();
								}
								ImGui::TreePop();
							}
						}
					}
					else
					{
						ImGui::TextDisabled("(no context)");
					}
					ImGui::TreePop();
				}

				// ---- Granted Abilities ----
				if (ActiveGE->GrantedAbilityHandles.Num() > 0)
				{
					if (ImGui::TreeNode("Granted Abilities"))
					{
						for (const FGameplayAbilitySpecHandle& AbilityHandle : ActiveGE->GrantedAbilityHandles)
						{
							FGameplayAbilitySpec* Spec = InASC->FindAbilitySpecFromHandle(AbilityHandle);
							if (Spec && Spec->Ability)
							{
								ImGui::BulletText("%s (Level %d)", TCHAR_TO_ANSI(*Spec->Ability->GetName()), Spec->Level);
							}
							else
							{
								ImGui::BulletText("Handle: %s (spec not found)", TCHAR_TO_ANSI(*AbilityHandle.ToString()));
							}
						}
						ImGui::TreePop();
					}
				}

				// ---- Prediction Key ----
				if (ActiveGE->PredictionKey.IsValidKey())
				{
					if (ImGui::TreeNode("Prediction"))
					{
						ImGui::Text("Prediction Key: %s", TCHAR_TO_ANSI(*ActiveGE->PredictionKey.ToString()));
						ImGui::TreePop();
					}
				}

				// ---- GE Definition Details ----
				if (GEDef && ImGui::TreeNode("GE Definition"))
				{
					if (ImGui::BeginTable("GEDefTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
					{
						ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 200.0f);
						ImGui::TableSetupColumn("Value");

						TableRow("Class", GEDef->GetClass()->GetName());
						TableRow("Duration Policy", FString(DurationPolicyToString(GEDef->DurationPolicy)));
						TableRow("Stacking Type", FString(StackingTypeToString(GEDef->GetStackingType())));
						TableRowInt("Stack Limit", GEDef->GetStackLimitCount());
						TableRowInt("Modifier Count", GEDef->Modifiers.Num());
						TableRowInt("Execution Count", GEDef->Executions.Num());

						ImGui::EndTable();
					}

					// GE Definition tags
					TableRowTags("Asset Tags (Def)", GEDef->GetAssetTags());
					TableRowTags("Granted Tags (Def)", GEDef->GetGrantedTags());
					TableRowTags("Blocked Ability Tags (Def)", GEDef->GetBlockedAbilityTags());

					// Gameplay Cues
					if (GEDef->GameplayCues.Num() > 0)
					{
						if (ImGui::TreeNode("Gameplay Cues"))
						{
							for (const FGameplayEffectCue& Cue : GEDef->GameplayCues)
							{
								ImGui::BulletText("%s (Level: %.1f-%.1f)",
									TCHAR_TO_ANSI(*Cue.GameplayCueTags.ToStringSimple()),
									Cue.MinLevel, Cue.MaxLevel);
							}
							ImGui::TreePop();
						}
					}

					ImGui::TreePop();
				}

				ImGui::TreePop();
			}
			else
			{
				if (bInhibited)
				{
					ImGui::PopStyleColor();
				}
			}
		}
	}

	// ============ Effects Summary by Attribute ============
	if (ImGui::CollapsingHeader("Effects by Attribute"))
	{
		const FActiveGameplayEffectsContainer& ActiveGEContainer = InASC->GetActiveGameplayEffects();
		const TArray<FActiveGameplayEffectHandle> AllHandles = ActiveGEContainer.GetAllActiveEffectHandles();

		// Gather which attributes are affected by which effects
		TMap<FGameplayAttribute, TArray<const FActiveGameplayEffect*>> AttributeToEffects;

		for (const FActiveGameplayEffectHandle& Handle : AllHandles)
		{
			const FActiveGameplayEffect* ActiveGE = InASC->GetActiveGameplayEffect(Handle);
			if (!ActiveGE || !ActiveGE->Spec.Def)
			{
				continue;
			}

			for (const FGameplayModifierInfo& ModInfo : ActiveGE->Spec.Def->Modifiers)
			{
				AttributeToEffects.FindOrAdd(ModInfo.Attribute).Add(ActiveGE);
			}
		}

		if (AttributeToEffects.Num() == 0)
		{
			ImGui::TextDisabled("No effects modifying attributes.");
		}

		for (const auto& Pair : AttributeToEffects)
		{
			FString AttrLabel = FString::Printf(TEXT("%s (%d effects)###AttrFX_%s"),
				*Pair.Key.GetName(), Pair.Value.Num(), *Pair.Key.GetName());

			if (ImGui::TreeNode(TCHAR_TO_ANSI(*AttrLabel)))
			{
				if (ImGui::BeginTable("AttrFXTable", 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
				{
					ImGui::TableSetupColumn("Effect");
					ImGui::TableSetupColumn("Op", ImGuiTableColumnFlags_WidthFixed, 80.0f);
					ImGui::TableSetupColumn("Magnitude", ImGuiTableColumnFlags_WidthFixed, 80.0f);
					ImGui::TableSetupColumn("Stacks", ImGuiTableColumnFlags_WidthFixed, 50.0f);
					ImGui::TableSetupColumn("Inhibited", ImGuiTableColumnFlags_WidthFixed, 60.0f);
					ImGui::TableHeadersRow();

					for (const FActiveGameplayEffect* AGE : Pair.Value)
					{
						// Find the modifier index for this attribute
						for (int32 Idx = 0; Idx < AGE->Spec.Def->Modifiers.Num(); Idx++)
						{
							const FGameplayModifierInfo& ModInfo = AGE->Spec.Def->Modifiers[Idx];
							if (ModInfo.Attribute == Pair.Key)
							{
								float Magnitude = (Idx < AGE->Spec.Modifiers.Num())
									? AGE->Spec.GetModifierMagnitude(Idx)
									: 0.0f;

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(AGE->Spec.Def)));
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%s", TCHAR_TO_ANSI(*UEnum::GetValueAsString(ModInfo.ModifierOp).RightChop(FString("EGameplayModOp::").Len())));
								ImGui::TableSetColumnIndex(2);
								ImGui::Text("%.4f", Magnitude);
								ImGui::TableSetColumnIndex(3);
								ImGui::Text("%d", AGE->Spec.GetStackCount());
								ImGui::TableSetColumnIndex(4);
								if (AGE->bIsInhibited)
								{
									ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Yes");
								}
								else
								{
									ImGui::Text("No");
								}
							}
						}
					}
					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
		}
	}

	// ============ Attribute Aggregator Details ============
	if (ImGui::CollapsingHeader("Attribute Aggregators"))
	{
		const FActiveGameplayEffectsContainer& ActiveGEContainer = InASC->GetActiveGameplayEffects();
		const TMap<FGameplayAttribute, FAggregatorRef>& AggregatorMap = DebugHack::GetPrivateAggregaotMap(&ActiveGEContainer);

		if (AggregatorMap.Num() == 0)
		{
			ImGui::TextDisabled("No attribute aggregators.");
		}

		for (const auto& AggPair : AggregatorMap)
		{
			FString AggLabel = FString::Printf(TEXT("%s###Agg_%s"),
				*AggPair.Key.GetName(), *AggPair.Key.GetName());

			if (ImGui::TreeNode(TCHAR_TO_ANSI(*AggLabel)))
			{
				FAggregator* Agg = AggPair.Value.Get();
				if (Agg)
				{
					ImGui::Text("Base Value: %.4f", Agg->GetBaseValue());

					const FAggregatorModChannelContainer& ModChannels = DebugHack::GetPrivateAggregatorModChannels(Agg);

					TMap<EGameplayModEvaluationChannel, const TArray<FAggregatorMod>*> OutMods;
					ModChannels.GetAllAggregatorMods(OutMods);

					for (const auto& ChannelPair : OutMods)
					{
						FString ChannelName = UEnum::GetValueAsString(ChannelPair.Key);
						const TArray<FAggregatorMod>* Mods = ChannelPair.Value;

						if (!Mods || Mods->Num() == 0)
						{
							continue;
						}

						FString ChannelLabel = FString::Printf(TEXT("Channel: %s (%d mods)###AggCh_%s_%s"),
							*ChannelName, Mods->Num(), *AggPair.Key.GetName(), *ChannelName);

						if (ImGui::TreeNode(TCHAR_TO_ANSI(*ChannelLabel)))
						{
							// Show ModInfo per channel
							ModChannels.ForEachMod([&ChannelPair](const FAggregatorModInfo& ModInfo)
							{
								if (ModInfo.Channel == ChannelPair.Key)
								{
									ImGui::Text("Op: %s", TCHAR_TO_ANSI(*UEnum::GetValueAsString(ModInfo.Op)));
								}
							});

							// Show individual mods
							if (ImGui::BeginTable("AggMods", 6, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
							{
								ImGui::TableSetupColumn("Source Effect");
								ImGui::TableSetupColumn("Magnitude", ImGuiTableColumnFlags_WidthFixed, 80.0f);
								ImGui::TableSetupColumn("Stacks", ImGuiTableColumnFlags_WidthFixed, 50.0f);
								ImGui::TableSetupColumn("Predicted", ImGuiTableColumnFlags_WidthFixed, 60.0f);
								ImGui::TableSetupColumn("Qualified", ImGuiTableColumnFlags_WidthFixed, 60.0f);
								ImGui::TableSetupColumn("Tag Requirements");
								ImGui::TableHeadersRow();

								for (const FAggregatorMod& Mod : *Mods)
								{
									ImGui::TableNextRow();

									ImGui::TableSetColumnIndex(0);
									const FActiveGameplayEffect* ActiveEffect = InASC->GetActiveGameplayEffect(Mod.ActiveHandle);
									if (ActiveEffect)
									{
										ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(ActiveEffect->Spec.Def)));
									}
									else
									{
										ImGui::TextDisabled("(no source)");
									}

									ImGui::TableSetColumnIndex(1);
									ImGui::Text("%.4f", Mod.EvaluatedMagnitude);

									ImGui::TableSetColumnIndex(2);
									ImGui::Text("%.0f", Mod.StackCount);

									ImGui::TableSetColumnIndex(3);
									ImGui::Text("%s", Mod.IsPredicted ? "Yes" : "No");

									ImGui::TableSetColumnIndex(4);
									if (Mod.Qualifies())
									{
										ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Yes");
									}
									else
									{
										ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No");
									}

									ImGui::TableSetColumnIndex(5);
									FString TagReqStr;
									if (Mod.SourceTagReqs)
									{
										TagReqStr += FString::Printf(TEXT("Src: %s "), *Mod.SourceTagReqs->ToString());
									}
									if (Mod.TargetTagReqs)
									{
										TagReqStr += FString::Printf(TEXT("Tgt: %s"), *Mod.TargetTagReqs->ToString());
									}
									if (TagReqStr.IsEmpty())
									{
										ImGui::TextDisabled("(none)");
									}
									else
									{
										ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*TagReqStr));
									}
								}
								ImGui::EndTable();
							}
							ImGui::TreePop();
						}
					}
				}
				ImGui::TreePop();
			}
		}
	}

	// ============ Attribute Sets Summary ============
	if (ImGui::CollapsingHeader("Attribute Sets"))
	{
		const TArray<UAttributeSet*>& AttributeSets = InASC->GetSpawnedAttributes();
		if (AttributeSets.Num() == 0)
		{
			ImGui::TextDisabled("No attribute sets.");
		}

		for (UAttributeSet* AttrSet : AttributeSets)
		{
			if (!AttrSet)
			{
				continue;
			}

			FString SetLabel = FString::Printf(TEXT("%s###AttrSet_%s"),
				*AttrSet->GetClass()->GetName(), *GetNameSafe(AttrSet));

			if (ImGui::TreeNode(TCHAR_TO_ANSI(*SetLabel)))
			{
				if (ImGui::BeginTable("AttrTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
				{
					ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 100.0f);
					ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.0f);
					ImGui::TableHeadersRow();

					for (TFieldIterator<FProperty> It(AttrSet->GetClass()); It; ++It)
					{
						FStructProperty* StructProp = CastField<FStructProperty>(*It);
						if (!StructProp || !StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
						{
							continue;
						}

						const FGameplayAttributeData* AttrData = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttrSet);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", TCHAR_TO_ANSI(*StructProp->GetName()));
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%.4f", AttrData->GetBaseValue());
						ImGui::TableSetColumnIndex(2);

						float Diff = AttrData->GetCurrentValue() - AttrData->GetBaseValue();
						if (FMath::Abs(Diff) > KINDA_SMALL_NUMBER)
						{
							ImGui::TextColored(
								Diff > 0 ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
								"%.4f", AttrData->GetCurrentValue());
						}
						else
						{
							ImGui::Text("%.4f", AttrData->GetCurrentValue());
						}
					}
					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
		}
	}
}
