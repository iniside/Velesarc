// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsDebugger.h"
#include "ArcMassItemsDebuggerShared.h"

#include "imgui.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Items/ArcItemDefinitionData.h"
#include "Equipment/Fragments/ArcItemFragment_ItemAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_ItemAttachmentSlots.h"
#include "Equipment/Fragments/ArcItemFragment_ActorAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_SkeletalMeshAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_SkeletalMeshComponentAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_StaticMeshAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_StaticMeshComponentAttachment.h"
#include "Commands/ArcMassAssignItemSlotCommand.h"
#include "Commands/ArcMassRemoveItemFromSlotCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"

namespace ArcMassItemsDebugger::Attachments
{
	static const ImVec4 ColorOk(0.4f, 1.0f, 0.4f, 1.0f);
	static const ImVec4 ColorDim(0.6f, 0.6f, 0.6f, 1.0f);
	static const ImVec4 ColorWarn(1.0f, 0.3f, 0.3f, 1.0f);
	static const ImVec4 ColorYellow(1.0f, 1.0f, 0.3f, 1.0f);

	void DrawAttachmentType(const FArcItemDefinitionData* SourceDef)
	{
		if (!SourceDef)
		{
			ImGui::TextColored(ColorWarn, "No source definition");
			return;
		}
		if (const FArcItemFragment_ActorAttachment* F =
			SourceDef->FindFragment<FArcItemFragment_ActorAttachment>())
		{
			ImGui::Text("Type: Actor");
			ImGui::Text("  ActorClass: %s", TCHAR_TO_ANSI(*F->ActorClass.ToString()));
			ImGui::Text("  AttachToCharacterMesh: %s",
				F->bAttachToCharacterMesh ? "true" : "false");
			return;
		}
		if (const FArcItemFragment_SkeletalMeshAttachment* F =
			SourceDef->FindFragment<FArcItemFragment_SkeletalMeshAttachment>())
		{
			ImGui::Text("Type: Skeletal Mesh");
			ImGui::Text("  Mesh: %s", TCHAR_TO_ANSI(*F->SkeletalMesh.ToString()));
			ImGui::Text("  LeaderPose: %s", F->bUseLeaderPose ? "true" : "false");
			return;
		}
		if (const FArcItemFragment_SkeletalMeshComponentAttachment* F =
			SourceDef->FindFragment<FArcItemFragment_SkeletalMeshComponentAttachment>())
		{
			ImGui::Text("Type: SkelMeshComponent");
			ImGui::Text("  ComponentClass: %s",
				TCHAR_TO_ANSI(*F->SkeletalMeshComponentClass.ToString()));
			return;
		}
		if (const FArcItemFragment_StaticMeshAttachment* F =
			SourceDef->FindFragment<FArcItemFragment_StaticMeshAttachment>())
		{
			ImGui::Text("Type: Static Mesh");
			ImGui::Text("  Mesh: %s", TCHAR_TO_ANSI(*F->StaticMeshAttachClass.ToString()));
			return;
		}
		if (const FArcItemFragment_StaticMeshComponentAttachment* F =
			SourceDef->FindFragment<FArcItemFragment_StaticMeshComponentAttachment>())
		{
			ImGui::Text("Type: StaticMeshComponent");
			ImGui::Text("  ComponentClass: %s",
				TCHAR_TO_ANSI(*F->StaticMeshAttachClass.ToString()));
			return;
		}
		ImGui::TextColored(ColorWarn, "Type: (no attachment fragment on definition)");
	}

	FString DescribeObject(const UObject* Obj)
	{
		if (!Obj)
		{
			return TEXT("null");
		}
		if (Obj->IsA<AActor>())
		{
			return FString::Printf(TEXT("Actor: %s"), *Obj->GetClass()->GetName());
		}
		if (Obj->IsA<USceneComponent>())
		{
			return FString::Printf(TEXT("Comp: %s"), *Obj->GetName());
		}
		return FString::Printf(TEXT("%s: %s"), *Obj->GetClass()->GetName(), *Obj->GetName());
	}
}

void FArcMassItemsDebugger::DrawAttachmentsTab(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
	using namespace ArcMassItemsDebugger::Attachments;

	FMassEntityView View(Manager, Entity);
	FArcMassItemAttachmentStateFragment* State =
		View.GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
	if (!State)
	{
		ImGui::TextColored(ColorWarn,
			"Entity has no FArcMassItemAttachmentStateFragment.");
		ImGui::TextDisabled(
			"Add UArcMassItemAttachmentTrait (with a UArcMassItemAttachmentConfig) to this entity's template.");
		return;
	}

	const FArcMassItemAttachmentConfigFragment* ConfigFrag =
		View.GetConstSharedFragmentDataPtr<FArcMassItemAttachmentConfigFragment>();
	const UArcMassItemAttachmentConfig* Config = ConfigFrag ? ConfigFrag->Config.Get() : nullptr;

	ImGui::Text("Active: %d   Taken socket-keys: %d",
		State->ActiveAttachments.Num(),
		State->TakenSockets.Num());
	ImGui::Separator();

	if (ImGui::TreeNode("Active Attachments"))
	{
		if (State->ActiveAttachments.Num() == 0)
		{
			ImGui::TextColored(ColorDim, "(none)");
		}
		for (int32 Idx = 0; Idx < State->ActiveAttachments.Num(); ++Idx)
		{
			const FArcMassItemAttachment& A = State->ActiveAttachments[Idx];
			const FString Label = FString::Printf(TEXT("[%d] %s | %s"),
				Idx,
				*ArcMassItemsDebugger::FormatGameplayTag(A.SlotId),
				*ArcMassItemsDebugger::FormatItemDefinition(A.ItemDefinition));

			ImGui::PushID(Idx);
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*Label)))
			{
				ImGui::SeparatorText("Identity");
				ImGui::Text("ItemId: %s",
					TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(A.ItemId)));
				ImGui::SameLine();
				if (ImGui::SmallButton("Show in Items"))
				{
					RequestJumpToItem(A.ItemId);
				}
				ImGui::Text("OwnerId: %s",
					TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(A.OwnerId)));
				ImGui::Text("ItemDef: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(A.ItemDefinition)));
				ImGui::Text("OwnerDef: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(A.OwnerItemDefinition)));

				ImGui::SeparatorText("Slots & Sockets");
				ImGui::Text("SlotId: %s",
					TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(A.SlotId)));
				ImGui::Text("OwnerSlotId: %s",
					TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(A.OwnerSlotId)));
				ImGui::Text("Socket: %s", TCHAR_TO_ANSI(*A.SocketName.ToString()));
				if (A.SocketComponentTag != NAME_None)
				{
					ImGui::Text("SocketComponentTag: %s",
						TCHAR_TO_ANSI(*A.SocketComponentTag.ToString()));
				}

				ImGui::SeparatorText("Attachment Type");
				const FArcItemDefinitionData* SourceDef = FArcItemDefinitionData::Find(
					A.VisualItemDefinition.IsValid() ? A.VisualItemDefinition : A.ItemDefinition);
				DrawAttachmentType(SourceDef);

				if (A.VisualItemDefinition.IsValid())
				{
					ImGui::SeparatorText("Visual Override");
					ImGui::Text("Visual: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(A.VisualItemDefinition)));
					if (A.OldVisualItemDefinition.IsValid())
					{
						ImGui::TextColored(ColorDim, "Previous: %s",
							TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(A.OldVisualItemDefinition)));
					}
				}

				if (!A.RelativeTransform.Equals(FTransform::Identity))
				{
					ImGui::SeparatorText("Transform");
					const FVector& Loc = A.RelativeTransform.GetLocation();
					const FRotator Rot = A.RelativeTransform.GetRotation().Rotator();
					const FVector& Scale = A.RelativeTransform.GetScale3D();
					ImGui::Text("Loc: (%.1f, %.1f, %.1f)", Loc.X, Loc.Y, Loc.Z);
					ImGui::Text("Rot: (%.1f, %.1f, %.1f)", Rot.Pitch, Rot.Yaw, Rot.Roll);
					if (!Scale.Equals(FVector::OneVector))
					{
						ImGui::Text("Scale: (%.2f, %.2f, %.2f)", Scale.X, Scale.Y, Scale.Z);
					}
				}

				ImGui::SeparatorText("Spawned Objects");
				if (const FArcMassItemAttachmentObjectArray* Objs =
					State->ObjectsAttachedFromItem.Find(A.ItemId))
				{
					if (Objs->Objects.Num() == 0)
					{
						ImGui::TextColored(ColorDim, "  (none tracked)");
					}
					for (int32 ObjIdx = 0; ObjIdx < Objs->Objects.Num(); ++ObjIdx)
					{
						UObject* Obj = Objs->Objects[ObjIdx].Get();
						if (IsValid(Obj))
						{
							ImGui::TextColored(ColorOk, "  [%d] %s", ObjIdx,
								TCHAR_TO_ANSI(*DescribeObject(Obj)));

							if (AActor* Actor = Cast<AActor>(Obj))
							{
								if (AActor* Parent = Actor->GetAttachParentActor())
								{
									ImGui::Text("       Attached to: %s",
										TCHAR_TO_ANSI(*GetNameSafe(Parent)));
									if (USceneComponent* Root = Actor->GetRootComponent())
									{
										ImGui::Text("       Socket: %s",
											TCHAR_TO_ANSI(*Root->GetAttachSocketName().ToString()));
									}
								}
							}
							else if (USceneComponent* Comp = Cast<USceneComponent>(Obj))
							{
								if (USceneComponent* Parent = Comp->GetAttachParent())
								{
									ImGui::Text("       Parent: %s",
										TCHAR_TO_ANSI(*GetNameSafe(Parent)));
									ImGui::Text("       Socket: %s",
										TCHAR_TO_ANSI(*Comp->GetAttachSocketName().ToString()));
								}
							}
						}
						else
						{
							ImGui::TextColored(ColorWarn, "  [%d] STALE", ObjIdx);
						}
					}
				}
				else
				{
					ImGui::TextColored(ColorDim, "  (no objects tracked for this item)");
				}

				const FArcItemDefinitionData* ItemDef = FArcItemDefinitionData::Find(A.ItemDefinition);
				const FArcItemFragment_ItemAttachmentSlots* SlotsFrag =
					ItemDef ? ItemDef->FindFragment<FArcItemFragment_ItemAttachmentSlots>() : nullptr;
				if (SlotsFrag)
				{
					ImGui::SeparatorText("Sub-Attachment Slots");

					FArcMassItemStoreFragment* Store = ArcMassItemsDebugger::GetStore(Manager, Entity);

					for (int32 SubIdx = 0; SubIdx < SlotsFrag->AttachmentSlots.Num(); ++SubIdx)
					{
						const FArcItemAttachmentSlot& Sub = SlotsFrag->AttachmentSlots[SubIdx];
						const FString SubLabel = ArcMassItemsDebugger::FormatGameplayTag(Sub.SlotId);

						// Find any active attachment whose OwnerSlotId matches this sub-slot and OwnerId == this item.
						const FArcMassItemAttachment* Occupant = nullptr;
						for (const FArcMassItemAttachment& Other : State->ActiveAttachments)
						{
							if (Other.OwnerSlotId == Sub.SlotId && Other.OwnerId == A.ItemId)
							{
								Occupant = &Other;
								break;
							}
						}

						ImGui::PushID(SubIdx);
						if (Occupant)
						{
							ImGui::TextColored(ColorOk, "[%d] %s -> %s",
								SubIdx, TCHAR_TO_ANSI(*SubLabel),
								TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(Occupant->ItemDefinition)));
							ImGui::SameLine();
							if (ImGui::SmallButton("Detach"))
							{
								UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
								if (World != nullptr)
								{
									Arcx::SendServerCommand<FArcMassRemoveItemFromSlotCommand>(World, Entity, Occupant->ItemId);
								}
							}
						}
						else
						{
							ImGui::TextColored(ColorDim, "[%d] %s (empty)",
								SubIdx, TCHAR_TO_ANSI(*SubLabel));
							if (Store && Store->ReplicatedItems.Items.Num() > 0)
							{
								ImGui::SameLine();
								if (ImGui::BeginCombo("##AssignSubSlot", "Assign..."))
								{
									// Two-pass: collect candidates whose item has no slot yet, then render.
									TArray<TPair<FArcItemId, const FArcItemData*>> Candidates;
									Candidates.Reserve(Store->ReplicatedItems.Items.Num());
									for (const FArcMassReplicatedItem& R : Store->ReplicatedItems.Items)
									{
										const FArcItemData* I = R.ToItem();
										if (I && !I->GetSlotId().IsValid())
										{
											Candidates.Add(TPair<FArcItemId, const FArcItemData*>(I->GetItemId(), I));
										}
									}
									for (const TPair<FArcItemId, const FArcItemData*>& C : Candidates)
									{
										const FString CandLabel = FString::Printf(TEXT("%s (%s)"),
											*ArcMassItemsDebugger::FormatItemDefinition(C.Value->GetItemDefinitionId()),
											*ArcMassItemsDebugger::FormatItemId(C.Key));
										if (ImGui::Selectable(TCHAR_TO_ANSI(*CandLabel)))
										{
											UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
											if (World != nullptr)
											{
												Arcx::SendServerCommand<FArcMassAssignItemSlotCommand>(World, Entity, C.Key, Sub.SlotId);
											}
										}
									}
									ImGui::EndCombo();
								}
							}
						}
						ImGui::PopID();
					}
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	if (State->TakenSockets.Num() > 0 && ImGui::TreeNode("Taken Sockets"))
	{
		for (const TPair<FGameplayTag, FArcMassItemAttachmentNameArray>& P : State->TakenSockets)
		{
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*P.Key.ToString())))
			{
				for (const FName& SocketName : P.Value.Names)
				{
					ImGui::BulletText("%s", TCHAR_TO_ANSI(*SocketName.ToString()));
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}

	if (Config)
	{
		ImGui::Text("Static config slots: %d", Config->Slots.Num());
	}
}
