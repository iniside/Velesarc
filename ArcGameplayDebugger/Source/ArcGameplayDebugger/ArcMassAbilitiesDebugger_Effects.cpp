// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesDebugger.h"

#if WITH_MASSENTITY_DEBUG

#include "imgui.h"
#include "MassEntityManager.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectModifier.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcEffectTask_StateTree.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectStateTreeContext.h"
#include "Effects/ArcAttributeCapture.h"
#include "Attributes/ArcAggregator.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "StateTreeInstanceData.h"
#include "StateTree.h"
#include "ArcImGuiReflectionWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace
{
	const char* ArcModifierOpToString(EArcModifierOp Op)
	{
		switch (Op)
		{
			case EArcModifierOp::Add:             return "Add";
			case EArcModifierOp::MultiplyAdditive: return "MultiplyAdditive";
			case EArcModifierOp::DivideAdditive:   return "DivideAdditive";
			case EArcModifierOp::MultiplyCompound: return "MultiplyCompound";
			case EArcModifierOp::AddFinal:         return "AddFinal";
			case EArcModifierOp::Override:         return "Override";
			default:                               return "Unknown";
		}
	}

	const char* ArcModifierTypeToString(EArcModifierType Type)
	{
		switch (Type)
		{
			case EArcModifierType::Simple:      return "Simple";
			case EArcModifierType::SetByCaller: return "SetByCaller";
			case EArcModifierType::Custom:      return "Custom";
			default:                            return "Unknown";
		}
	}

	const char* ArcEffectDurationToString(EArcEffectDuration Duration)
	{
		switch (Duration)
		{
			case EArcEffectDuration::Instant:  return "Instant";
			case EArcEffectDuration::Duration: return "Duration";
			case EArcEffectDuration::Infinite: return "Infinite";
			default:                           return "Unknown";
		}
	}

	const char* ArcStackTypeToString(EArcStackType Type)
	{
		switch (Type)
		{
			case EArcStackType::Counter:  return "Counter";
			case EArcStackType::Instance: return "Instance";
			default:                      return "Unknown";
		}
	}

	const char* ArcStackGroupingToString(EArcStackGrouping Grouping)
	{
		switch (Grouping)
		{
			case EArcStackGrouping::ByTarget: return "ByTarget";
			case EArcStackGrouping::BySource: return "BySource";
			default:                          return "Unknown";
		}
	}

	const char* ArcStackPolicyToString(EArcStackPolicy Policy)
	{
		switch (Policy)
		{
			case EArcStackPolicy::Aggregate: return "Aggregate";
			case EArcStackPolicy::Replace:   return "Replace";
			case EArcStackPolicy::DenyNew:   return "DenyNew";
			default:                         return "Unknown";
		}
	}

	const char* ArcStackDurationRefreshToString(EArcStackDurationRefresh Refresh)
	{
		switch (Refresh)
		{
			case EArcStackDurationRefresh::Refresh:     return "Refresh";
			case EArcStackDurationRefresh::Extend:      return "Extend";
			case EArcStackDurationRefresh::Independent: return "Independent";
			default:                                    return "Unknown";
		}
	}

	const char* ArcStackPeriodPolicyToString(EArcStackPeriodPolicy Policy)
	{
		switch (Policy)
		{
			case EArcStackPeriodPolicy::Unchanged: return "Unchanged";
			case EArcStackPeriodPolicy::Reset:     return "Reset";
			default:                               return "Unknown";
		}
	}

	const char* ArcStackOverflowPolicyToString(EArcStackOverflowPolicy Policy)
	{
		switch (Policy)
		{
			case EArcStackOverflowPolicy::Deny:          return "Deny";
			case EArcStackOverflowPolicy::RemoveOldest:  return "RemoveOldest";
			default:                                     return "Unknown";
		}
	}

	const char* ArcCaptureSourceToString(EArcCaptureSource Source)
	{
		switch (Source)
		{
			case EArcCaptureSource::Source: return "Source";
			case EArcCaptureSource::Target: return "Target";
			default:                        return "Unknown";
		}
	}
}

void FArcMassAbilitiesDebugger::DrawEffectsTab(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
	const FArcEffectStackFragment* StackFragment = Manager.GetFragmentDataPtr<FArcEffectStackFragment>(Entity);
	if (!StackFragment)
	{
		ImGui::TextDisabled("No FArcEffectStackFragment on entity");
		return;
	}

	const TArray<FArcActiveEffect>& ActiveEffects = StackFragment->ActiveEffects;

	float AvailWidth = ImGui::GetContentRegionAvail().x;
	float LeftWidth = AvailWidth * 0.30f;

	if (ImGui::BeginChild("EffectListPanel", ImVec2(LeftWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		ImGui::Text("Active Effects: %d", ActiveEffects.Num());
		ImGui::Separator();

		for (int32 i = 0; i < ActiveEffects.Num(); ++i)
		{
			const FArcActiveEffect& Effect = ActiveEffects[i];
			const FArcEffectSpec* Spec = Effect.Spec.GetPtr<FArcEffectSpec>();

			FString LabelStr;
			if (!Spec)
			{
				LabelStr = FString::Printf(TEXT("[%d] (no spec)"), i);
			}
			else if (!Spec->Definition)
			{
				LabelStr = FString::Printf(TEXT("[%d] (no definition)"), i);
			}
			else
			{
				LabelStr = FString::Printf(TEXT("[%d] %s"), i, *Spec->Definition->GetName());
			}

			bool bSelected = (EffectSelectedIndex == i);

			if (Effect.bInhibited)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			}

			if (ImGui::Selectable(TCHAR_TO_ANSI(*LabelStr), bSelected))
			{
				if (EffectSelectedIndex != i)
				{
					EffectSelectedIndex = i;
					EffectSTSelectedFrameIdx = INDEX_NONE;
					EffectSTSelectedStateIdx = MAX_uint16;
					bEffectSTSelectedFrameGlobal = false;
				}
			}

			if (Effect.bInhibited)
			{
				ImGui::PopStyleColor();
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("EffectDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		if (EffectSelectedIndex == INDEX_NONE || !ActiveEffects.IsValidIndex(EffectSelectedIndex))
		{
			ImGui::TextDisabled("Select an effect from the list");
		}
		else
		{
			const FArcActiveEffect& SelectedEffect = ActiveEffects[EffectSelectedIndex];
			const FArcEffectSpec* Spec = SelectedEffect.Spec.GetPtr<FArcEffectSpec>();

			if (ImGui::BeginTable("EffectOverviewTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 0.35f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
				ImGui::TableHeadersRow();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Definition");
				ImGui::TableSetColumnIndex(1);
				if (Spec && Spec->Definition)
				{
					ImGui::Text("%s", TCHAR_TO_ANSI(*Spec->Definition->GetPathName()));
				}
				else
				{
					ImGui::TextDisabled("(none)");
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("RemainingDuration");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", SelectedEffect.RemainingDuration);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("PeriodTimer");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", SelectedEffect.PeriodTimer);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("bInhibited");
				ImGui::TableSetColumnIndex(1);
				if (SelectedEffect.bInhibited)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "true");
				}
				else
				{
					ImGui::TextUnformatted("false");
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("OwnerEntity");
				ImGui::TableSetColumnIndex(1); ImGui::Text("Index=%d Serial=%d", SelectedEffect.OwnerEntity.Index, SelectedEffect.OwnerEntity.SerialNumber);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("ModifierHandles");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%d", SelectedEffect.ModifierHandles.Num());

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Tasks");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%d", SelectedEffect.Tasks.Num());

				ImGui::EndTable();
			}

			if (Spec && Spec->Definition)
			{
				const UArcEffectDefinition* Def = Spec->Definition;
				const FArcEffectPolicy& Policy = Def->StackingPolicy;

				if (ImGui::CollapsingHeader("Stacking Policy"))
				{
					if (ImGui::BeginTable("StackingPolicyTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
					{
						ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 0.4f);
						ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
						ImGui::TableHeadersRow();

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("DurationType");
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ArcEffectDurationToString(Policy.DurationType));

						if (Policy.DurationType == EArcEffectDuration::Duration)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Duration");
							ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", Policy.Duration);
						}

						if (Policy.Periodicity == EArcEffectPeriodicity::Periodic)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Periodicity");
							ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Periodic");

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Period");
							ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", Policy.Period);
						}

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("StackType");
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ArcStackTypeToString(Policy.StackType));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Grouping");
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ArcStackGroupingToString(Policy.Grouping));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("StackPolicy");
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ArcStackPolicyToString(Policy.StackPolicy));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("MaxStacks");
						ImGui::TableSetColumnIndex(1); ImGui::Text("%d", Policy.MaxStacks);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("RefreshPolicy");
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ArcStackDurationRefreshToString(Policy.RefreshPolicy));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("PeriodPolicy");
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ArcStackPeriodPolicyToString(Policy.PeriodPolicy));

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("OverflowPolicy");
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ArcStackOverflowPolicyToString(Policy.OverflowPolicy));

						ImGui::EndTable();
					}
				}

				if (ImGui::CollapsingHeader("Resolved Magnitudes"))
				{
					if (ImGui::BeginTable("MagnitudesTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
					{
						ImGui::TableSetupColumn("Index",    ImGuiTableColumnFlags_WidthFixed, 40.0f);
						ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthStretch, 0.35f);
						ImGui::TableSetupColumn("Operation", ImGuiTableColumnFlags_WidthStretch, 0.2f);
						ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_WidthStretch, 0.2f);
						ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthStretch, 0.15f);
						ImGui::TableHeadersRow();

						for (int32 ModIdx = 0; ModIdx < Spec->ResolvedMagnitudes.Num(); ++ModIdx)
						{
							float ResolvedVal = Spec->ResolvedMagnitudes[ModIdx];

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0); ImGui::Text("%d", ModIdx);

							ImGui::TableSetColumnIndex(1);
							if (Def->Modifiers.IsValidIndex(ModIdx))
							{
								const FArcEffectModifier& Mod = Def->Modifiers[ModIdx];
								FString AttrStr;
								if (Mod.Attribute.FragmentType && Mod.Attribute.PropertyName != NAME_None)
								{
									AttrStr = FString::Printf(TEXT("%s::%s"), *Mod.Attribute.FragmentType->GetName(), *Mod.Attribute.PropertyName.ToString());
								}
								else
								{
									AttrStr = TEXT("(invalid)");
								}
								ImGui::Text("%s", TCHAR_TO_ANSI(*AttrStr));

								ImGui::TableSetColumnIndex(2);
								ImGui::TextUnformatted(ArcModifierOpToString(Mod.Operation));

								ImGui::TableSetColumnIndex(3);
								ImGui::TextUnformatted(ArcModifierTypeToString(Mod.Type));
							}
							else
							{
								ImGui::TextDisabled("(no modifier)");
								ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("-");
								ImGui::TableSetColumnIndex(3); ImGui::TextDisabled("-");
							}

							ImGui::TableSetColumnIndex(4);
							ImGui::Text("%.3f", ResolvedVal);
						}

						ImGui::EndTable();
					}
				}
			}

			if (Spec)
			{
				if (ImGui::CollapsingHeader("SetByCaller Magnitudes"))
				{
					if (Spec->SetByCallerMagnitudes.Num() == 0)
					{
						ImGui::TextDisabled("(none)");
					}
					else if (ImGui::BeginTable("SetByCallerTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
					{
						ImGui::TableSetupColumn("Tag",   ImGuiTableColumnFlags_WidthStretch, 0.6f);
						ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.4f);
						ImGui::TableHeadersRow();

						for (const TPair<FGameplayTag, float>& Pair : Spec->SetByCallerMagnitudes)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text("%s", TCHAR_TO_ANSI(*Pair.Key.ToString()));
							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%.3f", Pair.Value);
						}

						ImGui::EndTable();
					}
				}

				if (ImGui::CollapsingHeader("Captured Tags"))
				{
					ImGui::Text("Source: %s", TCHAR_TO_ANSI(*Spec->CapturedSourceTags.ToStringSimple()));
					ImGui::Text("Target: %s", TCHAR_TO_ANSI(*Spec->CapturedTargetTags.ToStringSimple()));
				}

				if (ImGui::CollapsingHeader("Captured Attributes"))
				{
					if (Spec->CapturedAttributes.Num() == 0)
					{
						ImGui::TextDisabled("(none)");
					}
					else if (ImGui::BeginTable("CapturedAttrsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
					{
						ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthStretch, 0.4f);
						ImGui::TableSetupColumn("Source",    ImGuiTableColumnFlags_WidthStretch, 0.15f);
						ImGui::TableSetupColumn("Snapshot",  ImGuiTableColumnFlags_WidthStretch, 0.15f);
						ImGui::TableSetupColumn("Value",     ImGuiTableColumnFlags_WidthStretch, 0.2f);
						ImGui::TableHeadersRow();

						for (const FArcCapturedAttribute& CapturedAttr : Spec->CapturedAttributes)
						{
							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							const FArcAttributeRef& AttrRef = CapturedAttr.Definition.Attribute;
							if (AttrRef.FragmentType && AttrRef.PropertyName != NAME_None)
							{
								FString AttrStr = FString::Printf(TEXT("%s::%s"), *AttrRef.FragmentType->GetName(), *AttrRef.PropertyName.ToString());
								ImGui::Text("%s", TCHAR_TO_ANSI(*AttrStr));
							}
							else
							{
								ImGui::TextDisabled("(invalid)");
							}

							ImGui::TableSetColumnIndex(1);
							ImGui::TextUnformatted(ArcCaptureSourceToString(CapturedAttr.Definition.CaptureSource));

							ImGui::TableSetColumnIndex(2);
							ImGui::TextUnformatted(CapturedAttr.Definition.bSnapshot ? "true" : "false");

							ImGui::TableSetColumnIndex(3);
							if (CapturedAttr.bHasSnapshot)
							{
								ImGui::Text("%.3f", CapturedAttr.SnapshotValue);
							}
							else
							{
								ImGui::TextDisabled("(no snapshot)");
							}
						}

						ImGui::EndTable();
					}
				}

				if (ImGui::CollapsingHeader("Scoped Modifiers"))
				{
					if (Spec->ResolvedScopedModifiers.Num() == 0)
					{
						ImGui::TextDisabled("(none)");
					}
					else if (ImGui::BeginTable("ScopedModsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
					{
						ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthStretch, 0.5f);
						ImGui::TableSetupColumn("Operation", ImGuiTableColumnFlags_WidthStretch, 0.25f);
						ImGui::TableSetupColumn("Magnitude", ImGuiTableColumnFlags_WidthStretch, 0.25f);
						ImGui::TableHeadersRow();

						for (const FArcResolvedScopedMod& ScopedMod : Spec->ResolvedScopedModifiers)
						{
							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							const FArcAttributeRef& AttrRef = ScopedMod.CaptureDef.Attribute;
							if (AttrRef.FragmentType && AttrRef.PropertyName != NAME_None)
							{
								FString AttrStr = FString::Printf(TEXT("%s::%s"), *AttrRef.FragmentType->GetName(), *AttrRef.PropertyName.ToString());
								ImGui::Text("%s", TCHAR_TO_ANSI(*AttrStr));
							}
							else
							{
								ImGui::TextDisabled("(invalid)");
							}

							ImGui::TableSetColumnIndex(1);
							ImGui::TextUnformatted(ArcModifierOpToString(ScopedMod.Operation));

							ImGui::TableSetColumnIndex(2);
							ImGui::Text("%.3f", ScopedMod.Magnitude);
						}

						ImGui::EndTable();
					}
				}

				if (ImGui::CollapsingHeader("Effect Context"))
				{
					const FArcEffectContext& Ctx = Spec->Context;

					ImGui::Text("TargetEntity: Index=%d Serial=%d", Ctx.TargetEntity.Index, Ctx.TargetEntity.SerialNumber);
					ImGui::Text("SourceEntity: Index=%d Serial=%d", Ctx.SourceEntity.Index, Ctx.SourceEntity.SerialNumber);
					ImGui::Text("StackCount: %d", Ctx.StackCount);

					ImGui::Text("SourceAbility: %s", Ctx.SourceAbility ? TCHAR_TO_ANSI(*Ctx.SourceAbility->GetName()) : "(none)");
					ImGui::Text("SourceAbilityHandle: %s", TCHAR_TO_ANSI(*Ctx.SourceAbilityHandle.ToString()));

					if (Ctx.SourceData.IsValid())
					{
						ArcImGuiReflection::DrawStructView(TEXT("SourceData"), Ctx.SourceData);
					}
					else
					{
						ImGui::TextDisabled("SourceData: (none)");
					}
				}
			}

			if (ImGui::CollapsingHeader("Modifier Handles"))
			{
				if (SelectedEffect.ModifierHandles.Num() == 0)
				{
					ImGui::TextDisabled("(none)");
				}
				else
				{
					for (int32 HIdx = 0; HIdx < SelectedEffect.ModifierHandles.Num(); ++HIdx)
					{
						const FArcModifierHandle& Handle = SelectedEffect.ModifierHandles[HIdx];
						ImGui::Text("[%d] Id=%llu", HIdx, (unsigned long long)Handle.Id);
					}
				}
			}

			if (Spec && Spec->Definition && ImGui::CollapsingHeader("Components"))
			{
				const TArray<FInstancedStruct>& Components = Spec->Definition->Components;
				if (Components.Num() == 0)
				{
					ImGui::TextDisabled("(none)");
				}
				else
				{
					for (int32 CIdx = 0; CIdx < Components.Num(); ++CIdx)
					{
						const FInstancedStruct& Comp = Components[CIdx];
						if (!Comp.IsValid())
						{
							ImGui::PushID(CIdx);
							ImGui::TextDisabled("[%d] (invalid)", CIdx);
							ImGui::PopID();
							continue;
						}

						FString CompLabel = FString::Printf(TEXT("[%d] %s"), CIdx, *Comp.GetScriptStruct()->GetName());
						ImGui::PushID(CIdx);
						if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*CompLabel), ImGuiTreeNodeFlags_DefaultOpen))
						{
							ArcImGuiReflection::DrawStructView(*FString::Printf(TEXT("[%d]"), CIdx), Comp);
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
				}
			}

			if (ImGui::CollapsingHeader("Tasks"))
			{
				const TArray<FInstancedStruct>& Tasks = SelectedEffect.Tasks;
				if (Tasks.Num() == 0)
				{
					ImGui::TextDisabled("(none)");
				}
				else
				{
					for (int32 TIdx = 0; TIdx < Tasks.Num(); ++TIdx)
					{
						const FInstancedStruct& TaskStruct = Tasks[TIdx];
						if (!TaskStruct.IsValid())
						{
							ImGui::PushID(TIdx);
							ImGui::TextDisabled("[%d] (invalid)", TIdx);
							ImGui::PopID();
							continue;
						}

						const UScriptStruct* TaskScriptStruct = TaskStruct.GetScriptStruct();
						FString TaskLabel = FString::Printf(TEXT("[%d] %s"), TIdx, *TaskScriptStruct->GetName());

						ImGui::PushID(TIdx);

						const FArcEffectTask_StateTree* STTask = TaskStruct.GetPtr<FArcEffectTask_StateTree>();
						if (STTask)
						{
							if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*TaskLabel), ImGuiTreeNodeFlags_DefaultOpen))
							{
								const FArcEffectStateTreeContext& TreeCtx = STTask->GetTreeContext();
								if (TreeCtx.IsRunning())
								{
									const FStateTreeInstanceData& InstanceData = TreeCtx.GetInstanceData();
									const UStateTree* CachedTree = TreeCtx.GetCachedStateTree();

									FString STIntrospectId = FString::Printf(TEXT("EffectST_%d"), TIdx);
									DrawStateTreeIntrospection(InstanceData, CachedTree, EffectSTSelectedFrameIdx, EffectSTSelectedStateIdx, bEffectSTSelectedFrameGlobal, TCHAR_TO_ANSI(*STIntrospectId));

									if (EffectSTSelectedFrameIdx != INDEX_NONE)
									{
										ImGui::Separator();
										DrawStateTreeSelectedStateDetails(InstanceData, CachedTree, EffectSTSelectedFrameIdx, EffectSTSelectedStateIdx, bEffectSTSelectedFrameGlobal);
									}
								}
								else
								{
									ImGui::TextDisabled("StateTree not running");
								}
								ImGui::TreePop();
							}
						}
						else
						{
							if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*TaskLabel), ImGuiTreeNodeFlags_DefaultOpen))
							{
								ArcImGuiReflection::DrawStructView(*FString::Printf(TEXT("[%d]"), TIdx), TaskStruct);
								ImGui::TreePop();
							}
						}

						ImGui::PopID();
					}
				}
			}
		}
	}
	ImGui::EndChild();
}

#endif // WITH_MASSENTITY_DEBUG
