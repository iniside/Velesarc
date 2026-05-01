// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesDebugger.h"

#if WITH_MASSENTITY_DEBUG

#include "imgui.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcImmunityFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAggregator.h"
#include "Modifiers/ArcDirectModifier.h"
#include "Tags/ArcTagCountContainer.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcImGuiReflectionWidget.h"

namespace
{
	const char* GetModifierOpName(EArcModifierOp Op)
	{
		switch (Op)
		{
		case EArcModifierOp::Add:              return "Add";
		case EArcModifierOp::MultiplyAdditive:  return "Multiply (Additive)";
		case EArcModifierOp::DivideAdditive:    return "Divide (Additive)";
		case EArcModifierOp::MultiplyCompound:  return "Multiply (Compound)";
		case EArcModifierOp::AddFinal:          return "Add (Final)";
		case EArcModifierOp::Override:          return "Override";
		default:                                return "Unknown";
		}
	}
}

void FArcMassAbilitiesDebugger::DrawAttributesTab(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
	const FArcAggregatorFragment* AggregatorFrag = Manager.GetFragmentDataPtr<FArcAggregatorFragment>(Entity);

	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::CollapsingHeader("Attribute Fragments"))
	{
		FMassArchetypeHandle Archetype = Manager.GetArchetypeForEntity(Entity);
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);

		TArray<const UScriptStruct*> AllElements;
		Composition.GetElementsBitSet().ExportTypes(AllElements);

		FMassEntityView EntityView(Manager, Entity);
		const UScriptStruct* AttributesBaseStruct = FArcMassAttributesBase::StaticStruct();
		const UScriptStruct* ArcAttributeStruct = FArcAttribute::StaticStruct();
		bool bFoundAny = false;

		for (const UScriptStruct* ElementType : AllElements)
		{
			if (!ElementType || !ElementType->IsChildOf(AttributesBaseStruct))
			{
				continue;
			}

			bFoundAny = true;
			FStructView FragView = EntityView.GetFragmentDataStruct(ElementType);
			if (!FragView.IsValid())
			{
				continue;
			}

			if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*ElementType->GetName()), ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable(TCHAR_TO_ANSI(*FString::Printf(TEXT("AttrFrag_%s"), *ElementType->GetName())), 3,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
				{
					ImGui::TableSetupColumn("Property");
					ImGui::TableSetupColumn("Base Value");
					ImGui::TableSetupColumn("Current Value");
					ImGui::TableHeadersRow();

					for (TFieldIterator<FStructProperty> PropIt(ElementType); PropIt; ++PropIt)
					{
						FStructProperty* StructProp = *PropIt;
						if (!StructProp->Struct || !StructProp->Struct->IsChildOf(ArcAttributeStruct))
						{
							continue;
						}

						const FArcAttribute* Attr = StructProp->ContainerPtrToValuePtr<FArcAttribute>(FragView.GetMemory());
						if (!Attr)
						{
							continue;
						}

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(TCHAR_TO_ANSI(*StructProp->GetName()));
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%.3f", Attr->GetBaseValue());
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("%.3f", Attr->GetCurrentValue());
					}

					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
		}

		if (!bFoundAny)
		{
			ImGui::TextDisabled("No FArcMassAttributesBase-derived fragments on entity");
		}
	}

	if (ImGui::CollapsingHeader("Aggregator Values"))
	{
		if (!AggregatorFrag)
		{
			ImGui::TextDisabled("No FArcAggregatorFragment on entity");
		}
		else
		{
			if (ImGui::BeginTable("AttrValuesTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Attribute");
				ImGui::TableSetupColumn("Base Value");
				ImGui::TableSetupColumn("Current Value");
				ImGui::TableSetupColumn("Mod Count");
				ImGui::TableHeadersRow();

				FMassEntityView EntityView(Manager, Entity);

				for (const TPair<FArcAttributeRef, FArcAggregator>& Pair : AggregatorFrag->Aggregators)
				{
					const FArcAttributeRef& AttrRef = Pair.Key;
					const FArcAggregator& Aggregator = Pair.Value;

					FString AttrLabel;
					if (AttrRef.FragmentType)
					{
						AttrLabel = FString::Printf(TEXT("%s.%s"), *AttrRef.FragmentType->GetName(), *AttrRef.PropertyName.ToString());
					}
					else
					{
						AttrLabel = AttrRef.PropertyName.ToString();
					}

					int32 ModCount = 0;
					for (int32 OpIdx = 0; OpIdx < static_cast<int32>(EArcModifierOp::Max); ++OpIdx)
					{
						ModCount += Aggregator.Mods[OpIdx].Num();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(TCHAR_TO_ANSI(*AttrLabel));

					float BaseVal = 0.f;
					float CurVal = 0.f;
					bool bFound = false;

					if (AttrRef.IsValid() && AttrRef.FragmentType)
					{
						FStructView FragView = EntityView.GetFragmentDataStruct(AttrRef.FragmentType);
						if (FragView.IsValid())
						{
							FProperty* Prop = AttrRef.GetCachedProperty();
							if (Prop)
							{
								const FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragView.GetMemory());
								if (Attr)
								{
									BaseVal = Attr->GetBaseValue();
									CurVal = Attr->GetCurrentValue();
									bFound = true;
								}
							}
						}
					}

					ImGui::TableSetColumnIndex(1);
					if (bFound)
					{
						ImGui::Text("%.3f", BaseVal);
					}
					else
					{
						ImGui::TextDisabled("N/A");
					}

					ImGui::TableSetColumnIndex(2);
					if (bFound)
					{
						ImGui::Text("%.3f", CurVal);
					}
					else
					{
						ImGui::TextDisabled("N/A");
					}

					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%d", ModCount);
				}

				ImGui::EndTable();
			}
		}
	}

	if (ImGui::CollapsingHeader("Aggregator Breakdown"))
	{
		if (!AggregatorFrag)
		{
			ImGui::TextDisabled("No FArcAggregatorFragment on entity");
		}
		else
		{
			FMassEntityView EntityView(Manager, Entity);

			for (const TPair<FArcAttributeRef, FArcAggregator>& Pair : AggregatorFrag->Aggregators)
			{
				const FArcAttributeRef& AttrRef = Pair.Key;
				const FArcAggregator& Aggregator = Pair.Value;

				FString AttrLabel;
				if (AttrRef.FragmentType)
				{
					AttrLabel = FString::Printf(TEXT("%s.%s"), *AttrRef.FragmentType->GetName(), *AttrRef.PropertyName.ToString());
				}
				else
				{
					AttrLabel = AttrRef.PropertyName.ToString();
				}

				if (ImGui::TreeNode(TCHAR_TO_ANSI(*AttrLabel)))
				{
					for (int32 OpIdx = 0; OpIdx < static_cast<int32>(EArcModifierOp::Max); ++OpIdx)
					{
						const TArray<FArcAggregatorMod>& ModArray = Aggregator.Mods[OpIdx];
						if (ModArray.IsEmpty())
						{
							continue;
						}

						EArcModifierOp Op = static_cast<EArcModifierOp>(OpIdx);
						FString OpNodeLabel = FString::Printf(TEXT("%s (%d)"), ANSI_TO_TCHAR(GetModifierOpName(Op)), ModArray.Num());

						if (ImGui::TreeNode(TCHAR_TO_ANSI(*OpNodeLabel)))
						{
							for (const FArcAggregatorMod& Mod : ModArray)
							{
								FString SourceDesc = Mod.Source.IsSet() ? Mod.Source.DebugGetDescription() : TEXT("None");
								FString ModLine = FString::Printf(TEXT("Mag=%.3f  Source=%s  Ch=%d  Id=%llu"), Mod.Magnitude, *SourceDesc, (int32)Mod.Channel, Mod.Handle.Id);
								ImGui::TextUnformatted(TCHAR_TO_ANSI(*ModLine));
								ImGui::SameLine();
								if (Mod.bQualified)
								{
									ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[Qualified]");
								}
								else
								{
									ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[Not Qualified]");
								}
							}
							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
			}
		}
	}

	if (ImGui::CollapsingHeader("Direct Modifiers"))
	{
		if (!AggregatorFrag)
		{
			ImGui::TextDisabled("No FArcAggregatorFragment on entity");
		}
		else
		{
			if (ImGui::BeginTable("DirectModTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Handle Id");
				ImGui::TableSetupColumn("Attribute");
				ImGui::TableSetupColumn("Operation");
				ImGui::TableSetupColumn("Magnitude");
				ImGui::TableHeadersRow();

				for (const TPair<FArcModifierHandle, FArcDirectModifier>& Pair : AggregatorFrag->OwnedModifiers)
				{
					const FArcModifierHandle& Handle = Pair.Key;
					const FArcDirectModifier& DirectMod = Pair.Value;

					FString AttrLabel;
					if (DirectMod.Attribute.FragmentType)
					{
						AttrLabel = FString::Printf(TEXT("%s.%s"), *DirectMod.Attribute.FragmentType->GetName(), *DirectMod.Attribute.PropertyName.ToString());
					}
					else
					{
						AttrLabel = DirectMod.Attribute.PropertyName.ToString();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%llu", Handle.Id);
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(TCHAR_TO_ANSI(*AttrLabel));
					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(GetModifierOpName(DirectMod.Operation));
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%.3f", DirectMod.Magnitude);
				}

				ImGui::EndTable();
			}
		}
	}

	if (ImGui::CollapsingHeader("Pending Attribute Ops"))
	{
		const FArcPendingAttributeOpsFragment* PendingFrag = Manager.GetFragmentDataPtr<FArcPendingAttributeOpsFragment>(Entity);
		if (!PendingFrag)
		{
			ImGui::TextDisabled("No FArcPendingAttributeOpsFragment on entity");
		}
		else
		{
			if (ImGui::BeginTable("PendingOpsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Attribute");
				ImGui::TableSetupColumn("Operation");
				ImGui::TableSetupColumn("Magnitude");
				ImGui::TableSetupColumn("From Execution");
				ImGui::TableHeadersRow();

				for (const FArcPendingAttributeOp& Op : PendingFrag->PendingOps)
				{
					FString AttrLabel;
					if (Op.Attribute.FragmentType)
					{
						AttrLabel = FString::Printf(TEXT("%s.%s"), *Op.Attribute.FragmentType->GetName(), *Op.Attribute.PropertyName.ToString());
					}
					else
					{
						AttrLabel = Op.Attribute.PropertyName.ToString();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(TCHAR_TO_ANSI(*AttrLabel));
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(GetModifierOpName(Op.Operation));
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%.3f", Op.Magnitude);
					ImGui::TableSetColumnIndex(3);
					ImGui::TextUnformatted(Op.OpType == EArcAttributeOpType::Execute ? "Execute" : "Change");
				}

				ImGui::EndTable();
			}
		}
	}

	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::CollapsingHeader("Owned Tags"))
	{
		const FArcOwnedTagsFragment* TagsFrag = Manager.GetFragmentDataPtr<FArcOwnedTagsFragment>(Entity);
		if (!TagsFrag)
		{
			ImGui::TextDisabled("No FArcOwnedTagsFragment on entity");
		}
		else
		{
			const TMap<FGameplayTag, int32>& TagCounts = TagsFrag->Tags.GetTagCounts();
			if (TagCounts.IsEmpty())
			{
				ImGui::TextDisabled("No tags");
			}
			else
			{
				if (ImGui::BeginTable("OwnedTagsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
				{
					ImGui::TableSetupColumn("Tag");
					ImGui::TableSetupColumn("Count");
					ImGui::TableHeadersRow();

					for (const TPair<FGameplayTag, int32>& TagPair : TagCounts)
					{
						FString TagStr = TagPair.Key.ToString();
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(TCHAR_TO_ANSI(*TagStr));
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%d", TagPair.Value);
					}

					ImGui::EndTable();
				}
			}
		}
	}

	if (ImGui::CollapsingHeader("Immunity Tags"))
	{
		const FArcImmunityFragment* ImmunityFrag = Manager.GetFragmentDataPtr<FArcImmunityFragment>(Entity);
		if (!ImmunityFrag)
		{
			ImGui::TextDisabled("No FArcImmunityFragment on entity");
		}
		else
		{
			if (ImmunityFrag->ImmunityTags.IsEmpty())
			{
				ImGui::TextDisabled("No immunity tags");
			}
			else
			{
				for (const FGameplayTag& Tag : ImmunityFrag->ImmunityTags)
				{
					FString TagStr = Tag.ToString();
					ImGui::TextUnformatted(TCHAR_TO_ANSI(*TagStr));
				}
			}
		}
	}
}

#endif
