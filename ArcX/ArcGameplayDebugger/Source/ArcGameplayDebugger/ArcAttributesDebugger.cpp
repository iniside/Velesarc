// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAttributesDebugger.h"

#include "ArcDebuggerHacks.h"
#include "imgui.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	auto BoolToText = [](bool b) -> const TCHAR*
	{
		return b ? TEXT("True") : TEXT("False");
	};
}

void FArcAttributesDebugger::Initialize()
{
}

void FArcAttributesDebugger::Uninitialize()
{
	SelectedASC = nullptr;
}

void FArcAttributesDebugger::Draw()
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

	ImGui::SetNextWindowSize(ImVec2(750, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Attribute Sets Debugger", &bShow);

	DrawASCSelector();

	ImGui::Separator();

	UAbilitySystemComponent* ASC = SelectedASC.Get();
	if (ASC)
	{
		DrawAttributeDetails(ASC);
	}
	else
	{
		ImGui::TextDisabled("Select an Ability System Component above.");
	}

	ImGui::End();
}

void FArcAttributesDebugger::DrawASCSelector()
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
	if (ImGui::BeginCombo("##ASCSelector_Attrs", TCHAR_TO_ANSI(*CurrentLabel)))
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

void FArcAttributesDebugger::DrawAttributeDetails(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	const TArray<UAttributeSet*>& AttributeSets = InASC->GetSpawnedAttributes();
	const FActiveGameplayEffectsContainer& ActiveEffects = InASC->GetActiveGameplayEffects();
	const TMap<FGameplayAttribute, FAggregatorRef>& AggregatorMap = DebugHack::GetPrivateAggregaotMap(&ActiveEffects);

	if (AttributeSets.Num() == 0)
	{
		ImGui::TextDisabled("No attribute sets on this ASC.");
		return;
	}

	ImGui::Text("Attribute Sets: %d | Aggregators: %d", AttributeSets.Num(), AggregatorMap.Num());
	ImGui::Separator();

	for (UAttributeSet* AttributeSet : AttributeSets)
	{
		if (!AttributeSet)
		{
			continue;
		}

		FString SetLabel = FString::Printf(TEXT("%s###AttrSetD_%s"),
			*AttributeSet->GetClass()->GetName(), *GetNameSafe(AttributeSet));

		if (ImGui::TreeNode(TCHAR_TO_ANSI(*SetLabel)))
		{
			// Overview table
			if (ImGui::BeginTable("AttrOverview", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable))
			{
				ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("Modifiers", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				ImGui::TableHeadersRow();

				for (TFieldIterator<FProperty> It(AttributeSet->GetClass()); It; ++It)
				{
					FStructProperty* StructProp = CastField<FStructProperty>(*It);
					if (!StructProp || !StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
					{
						continue;
					}

					const FGameplayAttributeData* AttrData = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSet);
					FGameplayAttribute GameplayAttribute(StructProp);

					// Count modifiers for this attribute
					int32 ModCount = 0;
					if (const FAggregatorRef* AggRef = AggregatorMap.Find(GameplayAttribute))
					{
						FAggregator* Agg = AggRef->Get();
						if (Agg)
						{
							const FAggregatorModChannelContainer& ModChannels = DebugHack::GetPrivateAggregatorModChannels(Agg);
							TMap<EGameplayModEvaluationChannel, const TArray<FAggregatorMod>*> OutMods;
							ModChannels.GetAllAggregatorMods(OutMods);
							for (const auto& ChannelPair : OutMods)
							{
								if (ChannelPair.Value)
								{
									ModCount += ChannelPair.Value->Num();
								}
							}
						}
					}

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
							"%.4f (%+.4f)", AttrData->GetCurrentValue(), Diff);
					}
					else
					{
						ImGui::Text("%.4f", AttrData->GetCurrentValue());
					}

					ImGui::TableSetColumnIndex(3);
					if (ModCount > 0)
					{
						ImGui::Text("%d", ModCount);
					}
					else
					{
						ImGui::TextDisabled("0");
					}
				}
				ImGui::EndTable();
			}

			ImGui::Spacing();

			// Detailed aggregator breakdown per attribute
			if (ImGui::TreeNode("Aggregator Details"))
			{
				for (TFieldIterator<FProperty> It(AttributeSet->GetClass()); It; ++It)
				{
					FStructProperty* StructProp = CastField<FStructProperty>(*It);
					if (!StructProp || !StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
					{
						continue;
					}

					FGameplayAttribute GameplayAttribute(StructProp);
					const FAggregatorRef* AggRef = AggregatorMap.Find(GameplayAttribute);
					if (!AggRef)
					{
						continue;
					}

					FAggregator* Agg = AggRef->Get();
					if (!Agg)
					{
						continue;
					}

					const FGameplayAttributeData* AttrData = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSet);
					FString AggLabel = FString::Printf(TEXT("%s (Base: %.4f, Current: %.4f)###AggD_%s_%s"),
						*StructProp->GetName(), AttrData->GetBaseValue(), AttrData->GetCurrentValue(),
						*StructProp->GetName(), *GetNameSafe(AttributeSet));

					if (ImGui::TreeNode(TCHAR_TO_ANSI(*AggLabel)))
					{
						ImGui::Text("Aggregator Base Value: %.4f", Agg->GetBaseValue());

						const FAggregatorModChannelContainer& ModChannels = DebugHack::GetPrivateAggregatorModChannels(Agg);

						TMap<EGameplayModEvaluationChannel, const TArray<FAggregatorMod>*> OutMods;
						ModChannels.GetAllAggregatorMods(OutMods);

						for (const auto& ChannelPair : OutMods)
						{
							const TArray<FAggregatorMod>* Mods = ChannelPair.Value;
							if (!Mods || Mods->Num() == 0)
							{
								continue;
							}

							FString ChannelName = UEnum::GetValueAsString(ChannelPair.Key);
							FString ChannelLabel = FString::Printf(TEXT("Channel: %s (%d mods)###AggCh2_%s_%s_%s"),
								*ChannelName, Mods->Num(),
								*StructProp->GetName(), *GetNameSafe(AttributeSet), *ChannelName);

							if (ImGui::TreeNode(TCHAR_TO_ANSI(*ChannelLabel)))
							{
								// Show ModInfo
								ModChannels.ForEachMod([&ChannelPair](const FAggregatorModInfo& ModInfo)
								{
									if (ModInfo.Channel == ChannelPair.Key)
									{
										ImGui::Text("Op: %s", TCHAR_TO_ANSI(*UEnum::GetValueAsString(ModInfo.Op)));
									}
								});

								ImGui::Spacing();

								// Detailed mod table
								if (ImGui::BeginTable("AggModDetail", 7, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
								{
									ImGui::TableSetupColumn("Source Effect");
									ImGui::TableSetupColumn("Magnitude", ImGuiTableColumnFlags_WidthFixed, 80.0f);
									ImGui::TableSetupColumn("Stacks", ImGuiTableColumnFlags_WidthFixed, 50.0f);
									ImGui::TableSetupColumn("Predicted", ImGuiTableColumnFlags_WidthFixed, 60.0f);
									ImGui::TableSetupColumn("Qualified", ImGuiTableColumnFlags_WidthFixed, 60.0f);
									ImGui::TableSetupColumn("Source Tag Reqs");
									ImGui::TableSetupColumn("Target Tag Reqs");
									ImGui::TableHeadersRow();

									for (const FAggregatorMod& Mod : *Mods)
									{
										ImGui::TableNextRow();

										// Source Effect
										ImGui::TableSetColumnIndex(0);
										const FActiveGameplayEffect* ActiveEffect = InASC->GetActiveGameplayEffect(Mod.ActiveHandle);
										if (ActiveEffect && ActiveEffect->Spec.Def)
										{
											if (ActiveEffect->bIsInhibited)
											{
												ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s (Inhibited)",
													TCHAR_TO_ANSI(*GetNameSafe(ActiveEffect->Spec.Def)));
											}
											else
											{
												ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(ActiveEffect->Spec.Def)));
											}
										}
										else
										{
											ImGui::TextDisabled("(no source)");
										}

										// Magnitude
										ImGui::TableSetColumnIndex(1);
										ImGui::Text("%.4f", Mod.EvaluatedMagnitude);

										// Stacks
										ImGui::TableSetColumnIndex(2);
										ImGui::Text("%.0f", Mod.StackCount);

										// Predicted
										ImGui::TableSetColumnIndex(3);
										if (Mod.IsPredicted)
										{
											ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "Yes");
										}
										else
										{
											ImGui::Text("No");
										}

										// Qualified
										ImGui::TableSetColumnIndex(4);
										if (Mod.Qualifies())
										{
											ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Yes");
										}
										else
										{
											ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No");
										}

										// Source Tag Requirements
										ImGui::TableSetColumnIndex(5);
										if (Mod.SourceTagReqs)
										{
											FString Str = Mod.SourceTagReqs->ToString();
											if (!Str.IsEmpty())
											{
												ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Str));
											}
											else
											{
												ImGui::TextDisabled("(none)");
											}
										}
										else
										{
											ImGui::TextDisabled("(none)");
										}

										// Target Tag Requirements
										ImGui::TableSetColumnIndex(6);
										if (Mod.TargetTagReqs)
										{
											FString Str = Mod.TargetTagReqs->ToString();
											if (!Str.IsEmpty())
											{
												ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Str));
											}
											else
											{
												ImGui::TextDisabled("(none)");
											}
										}
										else
										{
											ImGui::TextDisabled("(none)");
										}
									}
									ImGui::EndTable();
								}
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}

			// Which effects are modifying attributes in this set
			if (ImGui::TreeNode("Effects Affecting This Set"))
			{
				const TArray<FActiveGameplayEffectHandle> AllHandles = ActiveEffects.GetAllActiveEffectHandles();

				TSet<FString> ShownEffects;

				for (TFieldIterator<FProperty> It(AttributeSet->GetClass()); It; ++It)
				{
					FStructProperty* StructProp = CastField<FStructProperty>(*It);
					if (!StructProp || !StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
					{
						continue;
					}

					FGameplayAttribute GameplayAttribute(StructProp);

					for (const FActiveGameplayEffectHandle& Handle : AllHandles)
					{
						const FActiveGameplayEffect* ActiveGE = InASC->GetActiveGameplayEffect(Handle);
						if (!ActiveGE || !ActiveGE->Spec.Def)
						{
							continue;
						}

						for (int32 Idx = 0; Idx < ActiveGE->Spec.Def->Modifiers.Num(); Idx++)
						{
							const FGameplayModifierInfo& ModInfo = ActiveGE->Spec.Def->Modifiers[Idx];
							if (ModInfo.Attribute != GameplayAttribute)
							{
								continue;
							}

							FString Key = FString::Printf(TEXT("%s_%s_%d"), *GetNameSafe(ActiveGE->Spec.Def), *Handle.ToString(), Idx);
							if (ShownEffects.Contains(Key))
							{
								continue;
							}
							ShownEffects.Add(Key);

							float Magnitude = (Idx < ActiveGE->Spec.Modifiers.Num())
								? ActiveGE->Spec.GetModifierMagnitude(Idx)
								: 0.0f;

							FString OpName = UEnum::GetValueAsString(ModInfo.ModifierOp);
							OpName = OpName.RightChop(FString("EGameplayModOp::").Len());

							FString InhibitStr = ActiveGE->bIsInhibited ? TEXT(" [INHIBITED]") : TEXT("");

							ImGui::BulletText("%s -> %s %s %.4f (x%d stacks)%s",
								TCHAR_TO_ANSI(*GetNameSafe(ActiveGE->Spec.Def)),
								TCHAR_TO_ANSI(*StructProp->GetName()),
								TCHAR_TO_ANSI(*OpName),
								Magnitude,
								ActiveGE->Spec.GetStackCount(),
								TCHAR_TO_ANSI(*InhibitStr));
						}
					}
				}

				if (ShownEffects.Num() == 0)
				{
					ImGui::TextDisabled("(no effects modifying this set)");
				}

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	}
}
