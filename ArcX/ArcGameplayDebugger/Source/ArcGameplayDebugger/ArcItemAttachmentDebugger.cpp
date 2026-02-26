// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemAttachmentDebugger.h"

#include "imgui.h"
#include "Engine/AssetManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "Equipment/Fragments/ArcItemFragment_ItemAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_ItemAttachmentSlots.h"
#include "Equipment/Fragments/ArcItemFragment_ActorAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_SkeletalMeshAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_SkeletalMeshComponentAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_StaticMeshAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_StaticMeshComponentAttachment.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Kismet/GameplayStatics.h"

namespace ArcAttachmentDebuggerLocal
{
	static const ImVec4 ColorGreen(0.3f, 1.0f, 0.3f, 1.0f);
	static const ImVec4 ColorRed(1.0f, 0.3f, 0.3f, 1.0f);
	static const ImVec4 ColorYellow(1.0f, 1.0f, 0.3f, 1.0f);
	static const ImVec4 ColorGray(0.6f, 0.6f, 0.6f, 1.0f);
	static const ImVec4 ColorCyan(0.3f, 0.9f, 1.0f, 1.0f);

	FString GetAttachmentTypeName(const UArcItemDefinition* ItemDef)
	{
		if (!ItemDef)
		{
			return TEXT("None");
		}

		if (ItemDef->FindFragment<FArcItemFragment_ActorAttachment>())
		{
			return TEXT("Actor");
		}
		if (ItemDef->FindFragment<FArcItemFragment_SkeletalMeshAttachment>())
		{
			return TEXT("Skeletal Mesh");
		}
		if (ItemDef->FindFragment<FArcItemFragment_SkeletalMeshComponentAttachment>())
		{
			return TEXT("Skeletal Mesh Component");
		}
		if (ItemDef->FindFragment<FArcItemFragment_StaticMeshAttachment>())
		{
			return TEXT("Static Mesh");
		}
		if (ItemDef->FindFragment<FArcItemFragment_StaticMeshComponentAttachment>())
		{
			return TEXT("Static Mesh Component");
		}

		return TEXT("Unknown");
	}

	FString GetObjectTypeName(const UObject* Obj)
	{
		if (!Obj)
		{
			return TEXT("null");
		}
		if (Obj->IsA<AActor>())
		{
			return FString::Printf(TEXT("Actor: %s"), *Obj->GetClass()->GetName());
		}
		if (Obj->IsA<USkeletalMeshComponent>())
		{
			return FString::Printf(TEXT("SkelMeshComp: %s"), *Obj->GetName());
		}
		if (Obj->IsA<UStaticMeshComponent>())
		{
			return FString::Printf(TEXT("StaticMeshComp: %s"), *Obj->GetName());
		}
		if (Obj->IsA<USceneComponent>())
		{
			return FString::Printf(TEXT("SceneComp: %s"), *Obj->GetName());
		}
		return FString::Printf(TEXT("%s: %s"), *Obj->GetClass()->GetName(), *Obj->GetName());
	}
}

void FArcItemAttachmentDebugger::Initialize()
{
}

void FArcItemAttachmentDebugger::Uninitialize()
{
}

void FArcItemAttachmentDebugger::Draw()
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

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC || !PC->PlayerState)
	{
		return;
	}

	APlayerState* PS = PC->PlayerState;

	UArcItemAttachmentComponent* AttachComp = PS->FindComponentByClass<UArcItemAttachmentComponent>();
	if (!AttachComp)
	{
		ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorRed, "No UArcItemAttachmentComponent found on PlayerState");
		return;
	}
	
	ImGui::SetNextWindowSize(ImVec2(1400.f, 700.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Arc Craft Debug (ImGui)", &bShow))
	{
		ImGui::End();
		return;
	}
	
	const FArcItemAttachmentContainer& Attachments = AttachComp->GetReplicatedAttachments();
	const TMap<FArcItemId, FArcItemId>& PendingAttachments = AttachComp->GetPendingAttachments();
	const FArcItemAttachmentSlotContainer& StaticSlots = AttachComp->GetStaticAttachmentSlots();
	const TMap<FGameplayTag, TSet<FName>>& TakenSockets = AttachComp->GetTakenSockets();
	const FArcLinkedAnimLayer& LinkedAnimLayer = AttachComp->GetLinkedAnimLayer();
	const TMap<const UArcItemDefinition*, TArray<UObject*>>& ObjectsMap = AttachComp->GetObjectsAttachedFromItem();

	// --- Overview ---
	ImGui::Text("Attachments: %d", Attachments.Items.Num());
	ImGui::SameLine();
	ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGray, "| Pending: %d", PendingAttachments.Num());
	ImGui::SameLine();
	ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGray, "| Static Slots: %d", StaticSlots.Slots.Num());

	ImGui::Separator();

	// --- Static Attachment Slots ---
	if (ImGui::TreeNode("Static Attachment Slots"))
	{
		for (int32 SlotIdx = 0; SlotIdx < StaticSlots.Slots.Num(); ++SlotIdx)
		{
			const FArcItemAttachmentSlot& Slot = StaticSlots.Slots[SlotIdx];
			FString SlotLabel = Slot.SlotId.IsValid() ? Slot.SlotId.ToString() : TEXT("(invalid)");

			bool bHasAttachment = AttachComp->DoesSlotHaveAttachment(Slot.SlotId);
			const ImVec4& SlotColor = bHasAttachment ? ArcAttachmentDebuggerLocal::ColorGreen : ArcAttachmentDebuggerLocal::ColorGray;

			ImGui::PushID(SlotIdx);
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*SlotLabel)))
			{
				ImGui::TextColored(SlotColor, bHasAttachment ? "Occupied" : "Empty");

				if (Slot.DefaultVisualItem.IsValid())
				{
					ImGui::Text("Default Visual: %s", TCHAR_TO_ANSI(*Slot.DefaultVisualItem.ToString()));
				}

				if (Slot.Handlers.Num() > 0)
				{
					ImGui::Text("Handlers: %d", Slot.Handlers.Num());
					for (int32 HandlerIdx = 0; HandlerIdx < Slot.Handlers.Num(); ++HandlerIdx)
					{
						const FInstancedStruct& Handler = Slot.Handlers[HandlerIdx];
						FString HandlerType = Handler.IsValid() ? Handler.GetScriptStruct()->GetName() : TEXT("(null)");
						ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGray, "  [%d] %s", HandlerIdx, TCHAR_TO_ANSI(*HandlerType));
					}
				}

				// Show taken sockets for this slot
				if (const TSet<FName>* Sockets = TakenSockets.Find(Slot.SlotId))
				{
					ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorYellow, "Taken Sockets: %d", Sockets->Num());
					for (const FName& SocketName : *Sockets)
					{
						ImGui::Text("  - %s", TCHAR_TO_ANSI(*SocketName.ToString()));
					}
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	ImGui::Separator();

	// --- Replicated Attachments ---
	if (ImGui::TreeNode("Replicated Attachments"))
	{
		if (Attachments.Items.Num() == 0)
		{
			ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGray, "(none)");
		}

		for (int32 Idx = 0; Idx < Attachments.Items.Num(); ++Idx)
		{
			const FArcItemAttachment& Attachment = Attachments.Items[Idx];

			FString ItemDefName = GetNameSafe(Attachment.ItemDefinition);
			FString SlotStr = Attachment.SlotId.IsValid() ? Attachment.SlotId.ToString() : TEXT("(no slot)");
			FString AttachType = ArcAttachmentDebuggerLocal::GetAttachmentTypeName(Attachment.ItemDefinition);

			FString NodeLabel = FString::Printf(TEXT("[%d] %s | %s | %s"), Idx, *SlotStr, *ItemDefName, *AttachType);

			ImGui::PushID(Idx);
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*NodeLabel)))
			{
				// IDs
				ImGui::SeparatorText("Identity");
				ImGui::Text("ItemId: %s", TCHAR_TO_ANSI(*Attachment.ItemId.ToString()));
				if (Attachment.OwnerId.IsValid())
				{
					ImGui::Text("OwnerId: %s", TCHAR_TO_ANSI(*Attachment.OwnerId.ToString()));
				}
				else
				{
					ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGray, "OwnerId: (none - root attachment)");
				}
				ImGui::Text("Item Definition: %s", *GetNameSafe(Attachment.ItemDefinition));
				if (Attachment.OwnerItemDefinition)
				{
					ImGui::Text("Owner Item Definition: %s", *GetNameSafe(Attachment.OwnerItemDefinition));
				}

				// Slots
				ImGui::SeparatorText("Slots");
				ImGui::Text("SlotId: %s", TCHAR_TO_ANSI(*Attachment.SlotId.ToString()));
				if (Attachment.OwnerSlotId.IsValid())
				{
					ImGui::Text("OwnerSlotId: %s", TCHAR_TO_ANSI(*Attachment.OwnerSlotId.ToString()));
				}
				else
				{
					ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGray, "OwnerSlotId: (none)");
				}

				// Socket info
				ImGui::SeparatorText("Sockets");
				ImGui::Text("Socket: %s", TCHAR_TO_ANSI(*Attachment.SocketName.ToString()));
				if (Attachment.SocketComponentTag != NAME_None)
				{
					ImGui::Text("SocketComponentTag: %s", TCHAR_TO_ANSI(*Attachment.SocketComponentTag.ToString()));
				}
				if (Attachment.ChangedSocket != NAME_None)
				{
					ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorYellow, "ChangedSocket: %s", TCHAR_TO_ANSI(*Attachment.ChangedSocket.ToString()));
					if (Attachment.ChangeSceneComponentTag != NAME_None)
					{
						ImGui::Text("ChangeSceneComponentTag: %s", TCHAR_TO_ANSI(*Attachment.ChangeSceneComponentTag.ToString()));
					}
				}

				// Attachment type details from fragment
				ImGui::SeparatorText("Attachment Type");
				ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorCyan, "Type: %s", TCHAR_TO_ANSI(*AttachType));

				const UArcItemDefinition* SourceDef = Attachment.VisualItemDefinition ? Attachment.VisualItemDefinition : Attachment.ItemDefinition;
				if (SourceDef)
				{
					if (const FArcItemFragment_ActorAttachment* ActorFrag = SourceDef->FindFragment<FArcItemFragment_ActorAttachment>())
					{
						ImGui::Text("  ActorClass: %s", TCHAR_TO_ANSI(*ActorFrag->ActorClass.ToString()));
						ImGui::Text("  AttachToCharacterMesh: %s", ActorFrag->bAttachToCharacterMesh ? "true" : "false");
						if (ActorFrag->AttachTags.Num() > 0)
						{
							ImGui::Text("  AttachTags: %s", TCHAR_TO_ANSI(*ActorFrag->AttachTags.ToString()));
						}
					}
					else if (const FArcItemFragment_SkeletalMeshAttachment* SkelFrag = SourceDef->FindFragment<FArcItemFragment_SkeletalMeshAttachment>())
					{
						ImGui::Text("  SkeletalMesh: %s", TCHAR_TO_ANSI(*SkelFrag->SkeletalMesh.ToString()));
						ImGui::Text("  LeaderPose: %s", SkelFrag->bUseLeaderPose ? "true" : "false");
						if (SkelFrag->SkeletalMeshAnimInstance.IsValid())
						{
							ImGui::Text("  AnimInstance: %s", TCHAR_TO_ANSI(*SkelFrag->SkeletalMeshAnimInstance.ToString()));
						}
					}
					else if (const FArcItemFragment_SkeletalMeshComponentAttachment* SkelCompFrag = SourceDef->FindFragment<FArcItemFragment_SkeletalMeshComponentAttachment>())
					{
						ImGui::Text("  ComponentClass: %s", TCHAR_TO_ANSI(*SkelCompFrag->SkeletalMeshComponentClass.ToString()));
						ImGui::Text("  LeaderPose: %s", SkelCompFrag->bUseLeaderPose ? "true" : "false");
					}
					else if (const FArcItemFragment_StaticMeshAttachment* StaticFrag = SourceDef->FindFragment<FArcItemFragment_StaticMeshAttachment>())
					{
						ImGui::Text("  StaticMesh: %s", TCHAR_TO_ANSI(*StaticFrag->StaticMeshAttachClass.ToString()));
					}
					else if (const FArcItemFragment_StaticMeshComponentAttachment* StaticCompFrag = SourceDef->FindFragment<FArcItemFragment_StaticMeshComponentAttachment>())
					{
						ImGui::Text("  ComponentClass: %s", TCHAR_TO_ANSI(*StaticCompFrag->StaticMeshAttachClass.ToString()));
					}
					else
					{
						ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorRed, "  No attachment fragment found on definition!");
					}

					// Sub-attachment slots on this item
					if (const FArcItemFragment_ItemAttachmentSlots* SubSlotsFrag = SourceDef->FindFragment<FArcItemFragment_ItemAttachmentSlots>())
					{
						ImGui::SeparatorText("Sub-Attachment Slots");
						for (int32 SubIdx = 0; SubIdx < SubSlotsFrag->AttachmentSlots.Num(); ++SubIdx)
						{
							const FArcItemAttachmentSlot& SubSlot = SubSlotsFrag->AttachmentSlots[SubIdx];
							FString SubSlotName = SubSlot.SlotId.IsValid() ? SubSlot.SlotId.ToString() : TEXT("(invalid)");

							// Check if any attachment uses this sub-slot
							bool bSubSlotOccupied = false;
							for (const FArcItemAttachment& Other : Attachments.Items)
							{
								if (Other.OwnerSlotId == SubSlot.SlotId && Other.OwnerId == Attachment.ItemId)
								{
									bSubSlotOccupied = true;
									ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGreen, "  [%d] %s -> %s",
										SubIdx, TCHAR_TO_ANSI(*SubSlotName), *GetNameSafe(Other.ItemDefinition));
									break;
								}
							}
							if (!bSubSlotOccupied)
							{
								ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGray, "  [%d] %s (empty)", SubIdx, TCHAR_TO_ANSI(*SubSlotName));
							}
						}
					}
				}

				// Spawned objects
				ImGui::SeparatorText("Spawned Objects");
				const UArcItemDefinition* ObjKey = Attachment.VisualItemDefinition ? Attachment.VisualItemDefinition.Get() : Attachment.ItemDefinition.Get();
				if (ObjKey)
				{
					if (const TArray<UObject*>* Objects = ObjectsMap.Find(ObjKey))
					{
						for (int32 ObjIdx = 0; ObjIdx < Objects->Num(); ++ObjIdx)
						{
							UObject* Obj = (*Objects)[ObjIdx];
							if (IsValid(Obj))
							{
								FString ObjDesc = ArcAttachmentDebuggerLocal::GetObjectTypeName(Obj);
								ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGreen, "  [%d] %s", ObjIdx, TCHAR_TO_ANSI(*ObjDesc));

								// If it's an actor, show attach parent info
								if (AActor* Actor = Cast<AActor>(Obj))
								{
									AActor* ParentActor = Actor->GetAttachParentActor();
									if (ParentActor)
									{
										ImGui::Text("       Attached to: %s", TCHAR_TO_ANSI(*GetNameSafe(ParentActor)));
										USceneComponent* Root = Actor->GetRootComponent();
										if (Root)
										{
											ImGui::Text("       Socket: %s", TCHAR_TO_ANSI(*Root->GetAttachSocketName().ToString()));
										}
									}
								}
								else if (USceneComponent* Comp = Cast<USceneComponent>(Obj))
								{
									if (Comp->GetAttachParent())
									{
										ImGui::Text("       Parent: %s", TCHAR_TO_ANSI(*GetNameSafe(Comp->GetAttachParent())));
										ImGui::Text("       Socket: %s", TCHAR_TO_ANSI(*Comp->GetAttachSocketName().ToString()));
									}
								}
							}
							else
							{
								ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorRed, "  [%d] INVALID/STALE", ObjIdx);
							}
						}
					}
					else
					{
						ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorRed, "  No spawned objects tracked");
					}
				}
				else
				{
					ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorRed, "  No item definition - cannot look up objects");
				}

				// Visual items
				if (Attachment.VisualItemDefinition)
				{
					ImGui::SeparatorText("Visual Override");
					ImGui::Text("Visual: %s", *GetNameSafe(Attachment.VisualItemDefinition));
					if (Attachment.OldVisualItemDefinition)
					{
						ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGray, "Previous: %s", *GetNameSafe(Attachment.OldVisualItemDefinition));
					}
				}

				// Transform
				if (!Attachment.RelativeTransform.Equals(FTransform::Identity))
				{
					ImGui::SeparatorText("Transform");
					const FVector& Loc = Attachment.RelativeTransform.GetLocation();
					const FRotator Rot = Attachment.RelativeTransform.GetRotation().Rotator();
					const FVector& Scale = Attachment.RelativeTransform.GetScale3D();
					ImGui::Text("Loc: (%.1f, %.1f, %.1f)", Loc.X, Loc.Y, Loc.Z);
					ImGui::Text("Rot: (%.1f, %.1f, %.1f)", Rot.Pitch, Rot.Yaw, Rot.Roll);
					if (!Scale.Equals(FVector::OneVector))
					{
						ImGui::Text("Scale: (%.2f, %.2f, %.2f)", Scale.X, Scale.Y, Scale.Z);
					}
				}

				ImGui::Text("ChangeUpdate: %d", Attachment.ChangeUpdate);

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	ImGui::Separator();

	// --- Pending Attachments ---
	if (PendingAttachments.Num() > 0)
	{
		if (ImGui::TreeNode("Pending Attachments"))
		{
			for (const auto& Pair : PendingAttachments)
			{
				ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorYellow, "Owner: %s -> Child: %s",
					TCHAR_TO_ANSI(*Pair.Key.ToString()), TCHAR_TO_ANSI(*Pair.Value.ToString()));
			}
			ImGui::TreePop();
		}
	}

	// --- Linked Anim Layer ---
	if (LinkedAnimLayer.AnimLayers.Num() > 0 || LinkedAnimLayer.OwningItem.IsValid())
	{
		if (ImGui::TreeNode("Linked Anim Layer"))
		{
			ImGui::Text("Owning Item: %s", TCHAR_TO_ANSI(*LinkedAnimLayer.OwningItem.ToString()));
			ImGui::Text("Source Definition: %s", *GetNameSafe(LinkedAnimLayer.SourceItemDefinition));
			ImGui::Text("Layers: %d", LinkedAnimLayer.AnimLayers.Num());
			for (int32 LayerIdx = 0; LayerIdx < LinkedAnimLayer.AnimLayers.Num(); ++LayerIdx)
			{
				ImGui::Text("  [%d] %s", LayerIdx, TCHAR_TO_ANSI(*LinkedAnimLayer.AnimLayers[LayerIdx].ToString()));
			}
			ImGui::TreePop();
		}
	}

	// --- All Taken Sockets ---
	if (TakenSockets.Num() > 0)
	{
		if (ImGui::TreeNode("All Taken Sockets"))
		{
			for (const auto& Pair : TakenSockets)
			{
				FString SlotName = Pair.Key.ToString();
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*SlotName)))
				{
					for (const FName& SocketName : Pair.Value)
					{
						ImGui::Text("- %s", TCHAR_TO_ANSI(*SocketName.ToString()));
					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	}

	// --- All Spawned Objects ---
	if (ObjectsMap.Num() > 0)
	{
		if (ImGui::TreeNode("All Spawned Objects"))
		{
			for (const auto& Pair : ObjectsMap)
			{
				FString DefName = GetNameSafe(Pair.Key);
				FString Label = FString::Printf(TEXT("%s (%d)"), *DefName, Pair.Value.Num());
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*Label)))
				{
					for (int32 ObjIdx = 0; ObjIdx < Pair.Value.Num(); ++ObjIdx)
					{
						UObject* Obj = Pair.Value[ObjIdx];
						if (IsValid(Obj))
						{
							FString ObjDesc = ArcAttachmentDebuggerLocal::GetObjectTypeName(Obj);
							ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorGreen, "[%d] %s", ObjIdx, TCHAR_TO_ANSI(*ObjDesc));
						}
						else
						{
							ImGui::TextColored(ArcAttachmentDebuggerLocal::ColorRed, "[%d] INVALID/STALE", ObjIdx);
						}
					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	}
	
	ImGui::End();
}
