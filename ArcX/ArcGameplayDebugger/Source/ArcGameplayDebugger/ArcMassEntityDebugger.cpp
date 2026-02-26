// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityDebugger.h"

#include "imgui.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassEntityElementTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace
{
	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	FMassEntityManager* GetEntityManager()
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}
		UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
		if (!MassSub)
		{
			return nullptr;
		}
		return &MassSub->GetMutableEntityManager();
	}

	const char* GetElementCategory(const UScriptStruct* Type)
	{
		if (UE::Mass::IsA<FMassFragment>(Type))
		{
			return "Fragment";
		}
		if (UE::Mass::IsA<FMassTag>(Type))
		{
			return "Tag";
		}
		if (UE::Mass::IsA<FMassSharedFragment>(Type))
		{
			return "Shared";
		}
		if (UE::Mass::IsA<FMassConstSharedFragment>(Type))
		{
			return "ConstShared";
		}
		if (UE::Mass::IsA<FMassChunkFragment>(Type))
		{
			return "Chunk";
		}
		return "Unknown";
	}

	ImVec4 GetElementColor(const UScriptStruct* Type)
	{
		if (UE::Mass::IsA<FMassFragment>(Type))
		{
			return ImVec4(0.7f, 1.0f, 0.7f, 1.0f);
		}
		if (UE::Mass::IsA<FMassTag>(Type))
		{
			return ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
		}
		if (UE::Mass::IsA<FMassSharedFragment>(Type))
		{
			return ImVec4(1.0f, 0.85f, 0.5f, 1.0f);
		}
		if (UE::Mass::IsA<FMassConstSharedFragment>(Type))
		{
			return ImVec4(1.0f, 0.7f, 0.4f, 1.0f);
		}
		if (UE::Mass::IsA<FMassChunkFragment>(Type))
		{
			return ImVec4(0.8f, 0.6f, 1.0f, 1.0f);
		}
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	void DrawPropertyValue(const FProperty* Prop, const void* ContainerPtr)
	{
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerPtr);

		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			bool Value = BoolProp->GetPropertyValue(ValuePtr);
			ImGui::Text("%s", Value ? "true" : "false");
		}
		else if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
		{
			int32 Value = IntProp->GetPropertyValue(ValuePtr);
			ImGui::Text("%d", Value);
		}
		else if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
		{
			int64 Value = Int64Prop->GetPropertyValue(ValuePtr);
			ImGui::Text("%lld", Value);
		}
		else if (const FUInt32Property* UIntProp = CastField<FUInt32Property>(Prop))
		{
			uint32 Value = UIntProp->GetPropertyValue(ValuePtr);
			ImGui::Text("%u", Value);
		}
		else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			float Value = FloatProp->GetPropertyValue(ValuePtr);
			ImGui::Text("%.4f", Value);
		}
		else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			double Value = DoubleProp->GetPropertyValue(ValuePtr);
			ImGui::Text("%.6f", Value);
		}
		else if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
		{
			FName Value = NameProp->GetPropertyValue(ValuePtr);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Value.ToString()));
		}
		else if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
		{
			const FString& Value = StrProp->GetPropertyValue(ValuePtr);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Value));
		}
		else if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
		{
			const FText& Value = TextProp->GetPropertyValue(ValuePtr);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Value.ToString()));
		}
		else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			FString EnumStr;
			EnumProp->GetUnderlyingProperty()->ExportText_Direct(EnumStr, ValuePtr, ValuePtr, nullptr, 0);
			if (const UEnum* Enum = EnumProp->GetEnum())
			{
				int64 IntVal = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
				FText DisplayText = Enum->GetDisplayNameTextByValue(IntVal);
				if (!DisplayText.IsEmpty())
				{
					EnumStr = DisplayText.ToString();
				}
			}
			ImGui::Text("%s", TCHAR_TO_ANSI(*EnumStr));
		}
		else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
		{
			if (ByteProp->Enum)
			{
				uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
				FText DisplayText = ByteProp->Enum->GetDisplayNameTextByValue(Value);
				ImGui::Text("%s", TCHAR_TO_ANSI(*DisplayText.ToString()));
			}
			else
			{
				uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
				ImGui::Text("%u", Value);
			}
		}
		else if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
		{
			// For known types show inline, for others show as sub-tree
			if (StructProp->Struct == TBaseStructure<FVector>::Get())
			{
				const FVector& V = *static_cast<const FVector*>(ValuePtr);
				ImGui::Text("(%.2f, %.2f, %.2f)", V.X, V.Y, V.Z);
			}
			else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
			{
				const FRotator& R = *static_cast<const FRotator*>(ValuePtr);
				ImGui::Text("(P=%.2f, Y=%.2f, R=%.2f)", R.Pitch, R.Yaw, R.Roll);
			}
			else if (StructProp->Struct == TBaseStructure<FTransform>::Get())
			{
				const FTransform& T = *static_cast<const FTransform*>(ValuePtr);
				const FVector Loc = T.GetLocation();
				const FRotator Rot = T.Rotator();
				const FVector Scale = T.GetScale3D();
				ImGui::Text("Loc(%.1f,%.1f,%.1f) Rot(%.1f,%.1f,%.1f) Scl(%.1f,%.1f,%.1f)",
					Loc.X, Loc.Y, Loc.Z, Rot.Pitch, Rot.Yaw, Rot.Roll, Scale.X, Scale.Y, Scale.Z);
			}
			else if (StructProp->Struct == TBaseStructure<FQuat>::Get())
			{
				const FQuat& Q = *static_cast<const FQuat*>(ValuePtr);
				ImGui::Text("(X=%.4f, Y=%.4f, Z=%.4f, W=%.4f)", Q.X, Q.Y, Q.Z, Q.W);
			}
			else if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
			{
				const FLinearColor& C = *static_cast<const FLinearColor*>(ValuePtr);
				ImGui::Text("(%.2f, %.2f, %.2f, %.2f)", C.R, C.G, C.B, C.A);
				ImGui::SameLine();
				ImVec4 Col(C.R, C.G, C.B, C.A);
				ImGui::ColorButton("##color", Col, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
			}
			else if (StructProp->Struct == TBaseStructure<FColor>::Get())
			{
				const FColor& C = *static_cast<const FColor*>(ValuePtr);
				ImGui::Text("(%u, %u, %u, %u)", C.R, C.G, C.B, C.A);
				ImGui::SameLine();
				ImVec4 Col(C.R / 255.f, C.G / 255.f, C.B / 255.f, C.A / 255.f);
				ImGui::ColorButton("##color", Col, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
			}
			else
			{
				// Recurse into sub-struct
				FString NodeId = FString::Printf(TEXT("##%s_%p"), *Prop->GetName(), ValuePtr);
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*NodeId), "%s", TCHAR_TO_ANSI(*StructProp->Struct->GetName())))
				{
					for (TFieldIterator<FProperty> SubIt(StructProp->Struct); SubIt; ++SubIt)
					{
						FProperty* SubProp = *SubIt;
						ImGui::PushID(TCHAR_TO_ANSI(*SubProp->GetName()));

						if (ImGui::BeginTable("##subprop", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody))
						{
							ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
							ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::TextDisabled("%s", TCHAR_TO_ANSI(*SubProp->GetName()));
							ImGui::TableSetColumnIndex(1);
							DrawPropertyValue(SubProp, ValuePtr);
							ImGui::EndTable();
						}

						ImGui::PopID();
					}
					ImGui::TreePop();
				}
			}
		}
		else if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
		{
			const UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (Obj)
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*Obj->GetName()));
			}
			else
			{
				ImGui::TextDisabled("None");
			}
		}
		else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
			if (ArrayHelper.Num() == 0)
			{
				ImGui::TextDisabled("Empty [0]");
			}
			else
			{
				FString ArrayLabel = FString::Printf(TEXT("[%d] elements"), ArrayHelper.Num());
				FString ArrayId = FString::Printf(TEXT("##arr_%s_%p"), *Prop->GetName(), ValuePtr);
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*ArrayId), "%s", TCHAR_TO_ANSI(*ArrayLabel)))
				{
					const int32 MaxDisplay = FMath::Min(ArrayHelper.Num(), 64);
					for (int32 i = 0; i < MaxDisplay; ++i)
					{
						ImGui::PushID(i);
						FString ElemLabel = FString::Printf(TEXT("[%d]"), i);
						ImGui::Text("%s", TCHAR_TO_ANSI(*ElemLabel));
						ImGui::SameLine();
						DrawPropertyValue(ArrayProp->Inner, ArrayHelper.GetElementPtr(i));
						ImGui::PopID();
					}
					if (ArrayHelper.Num() > MaxDisplay)
					{
						ImGui::TextDisabled("... %d more", ArrayHelper.Num() - MaxDisplay);
					}
					ImGui::TreePop();
				}
			}
		}
		else if (const FMapProperty* MapProp = CastField<FMapProperty>(Prop))
		{
			FScriptMapHelper MapHelper(MapProp, ValuePtr);
			ImGui::Text("TMap [%d entries]", MapHelper.Num());
		}
		else if (const FSetProperty* SetProp = CastField<FSetProperty>(Prop))
		{
			FScriptSetHelper SetHelper(SetProp, ValuePtr);
			ImGui::Text("TSet [%d entries]", SetHelper.Num());
		}
		else
		{
			// Fallback: use ExportText
			FString ExportedVal;
			Prop->ExportText_Direct(ExportedVal, ValuePtr, ValuePtr, nullptr, 0);
			if (ExportedVal.Len() > 128)
			{
				ExportedVal = ExportedVal.Left(128) + TEXT("...");
			}
			ImGui::Text("%s", TCHAR_TO_ANSI(*ExportedVal));
		}
	}
}

void FArcMassEntityDebugger::Initialize()
{
	SelectedEntityIndex = INDEX_NONE;
	EntityFilterBuf[0] = '\0';
	CachedEntities.Reset();
	LastRefreshTime = 0.f;
	RefreshEntityList();
}

void FArcMassEntityDebugger::Uninitialize()
{
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
}

void FArcMassEntityDebugger::RefreshEntityList()
{
	CachedEntities.Reset();

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		return;
	}

#if WITH_MASSENTITY_DEBUG
	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			FEntityEntry& Entry = CachedEntities.AddDefaulted_GetRef();
			Entry.Entity = Entity;
			Entry.Label = FString::Printf(TEXT("Entity %d (SN:%d)"), Entity.Index, Entity.SerialNumber);
		}
	}
#endif
}

void FArcMassEntityDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Mass Entity Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	// Refresh button + auto-refresh
	if (ImGui::Button("Refresh"))
	{
		RefreshEntityList();
	}
	ImGui::SameLine();
	ImGui::Text("Entities: %d", CachedEntities.Num());

	// Auto-refresh every 2 seconds
	UWorld* World = GetDebugWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 2.0f)
		{
			LastRefreshTime = CurrentTime;
			RefreshEntityList();
		}
	}

	ImGui::Separator();

	// Split into two panes
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.35f;

	// Left panel: entity list
	if (ImGui::BeginChild("EntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: entity detail
	if (ImGui::BeginChild("EntityDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawEntityDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();
#endif
}

void FArcMassEntityDebugger::DrawEntityListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Entity List");
	ImGui::Separator();

	ImGui::InputText("Filter", EntityFilterBuf, IM_ARRAYSIZE(EntityFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(EntityFilterBuf)).ToLower();

	if (ImGui::BeginChild("EntityScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedEntities.Num(); ++i)
		{
			const FEntityEntry& Entry = CachedEntities[i];

			if (!Filter.IsEmpty() && !Entry.Label.ToLower().Contains(Filter))
			{
				continue;
			}

			const bool bSelected = (SelectedEntityIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Entry.Label), bSelected))
			{
				SelectedEntityIndex = i;
			}
		}
	}
	ImGui::EndChild();
#endif
}

void FArcMassEntityDebugger::DrawEntityDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		ImGui::TextDisabled("Select an entity from the list");
		return;
	}

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entity is no longer active");
		return;
	}

	ImGui::Text("Entity Detail");
	ImGui::Separator();

	// Header info
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", TCHAR_TO_ANSI(*Entry.Label));

	FMassArchetypeHandle Archetype = Manager->GetArchetypeForEntity(Entity);
	FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);

	// Collect all element types from the composition
	TArray<const UScriptStruct*> AllElements;
	Composition.GetElementsBitSet().ExportTypes(AllElements);

	// Categorize elements
	TArray<const UScriptStruct*> Fragments;
	TArray<const UScriptStruct*> Tags;
	TArray<const UScriptStruct*> SharedFragments;
	TArray<const UScriptStruct*> ConstSharedFragments;
	TArray<const UScriptStruct*> ChunkFragments;

	for (const UScriptStruct* ElementType : AllElements)
	{
		if (!ElementType)
		{
			continue;
		}
		if (UE::Mass::IsA<FMassConstSharedFragment>(ElementType))
		{
			ConstSharedFragments.Add(ElementType);
		}
		else if (UE::Mass::IsA<FMassSharedFragment>(ElementType))
		{
			SharedFragments.Add(ElementType);
		}
		else if (UE::Mass::IsA<FMassTag>(ElementType))
		{
			Tags.Add(ElementType);
		}
		else if (UE::Mass::IsA<FMassChunkFragment>(ElementType))
		{
			ChunkFragments.Add(ElementType);
		}
		else if (UE::Mass::IsA<FMassFragment>(ElementType))
		{
			Fragments.Add(ElementType);
		}
	}

	// Archetype stats
	UE::Mass::Debug::FArchetypeStats Stats;
	FMassDebugger::GetArchetypeEntityStats(Archetype, Stats);
	ImGui::TextDisabled("Archetype: %d entities, %d chunks, %llu bytes/entity",
		Stats.EntitiesCount, Stats.ChunksCount, Stats.BytesPerEntity);

	ImGui::Spacing();

	// Fragments section
	if (Fragments.Num() > 0)
	{
		ImGui::SeparatorText("Fragments");
		for (const UScriptStruct* FragType : Fragments)
		{
			FStructView FragData = Manager->GetFragmentDataStruct(Entity, FragType);
			if (FragData.IsValid())
			{
				DrawFragmentProperties(FragType, FragData.GetMemory());
			}
			else
			{
				ImGui::TextColored(GetElementColor(FragType), "%s", TCHAR_TO_ANSI(*FragType->GetName()));
			}
		}
	}

	// Tags section
	if (Tags.Num() > 0)
	{
		ImGui::SeparatorText("Tags");
		for (const UScriptStruct* TagType : Tags)
		{
			ImGui::BulletText("%s", TCHAR_TO_ANSI(*TagType->GetName()));
		}
	}

	// Shared Fragments section
	if (SharedFragments.Num() > 0)
	{
		ImGui::SeparatorText("Shared Fragments");
		for (const UScriptStruct* SharedType : SharedFragments)
		{
			FConstStructView SharedData = Manager->GetSharedFragmentDataStruct(Entity, SharedType);
			if (SharedData.IsValid())
			{
				DrawFragmentProperties(SharedType, SharedData.GetMemory());
			}
			else
			{
				ImGui::TextColored(GetElementColor(SharedType), "%s", TCHAR_TO_ANSI(*SharedType->GetName()));
			}
		}
	}

	// Const Shared Fragments section
	if (ConstSharedFragments.Num() > 0)
	{
		ImGui::SeparatorText("Const Shared Fragments");
		for (const UScriptStruct* ConstSharedType : ConstSharedFragments)
		{
			FConstStructView ConstSharedData = Manager->GetConstSharedFragmentDataStruct(Entity, ConstSharedType);
			if (ConstSharedData.IsValid())
			{
				DrawFragmentProperties(ConstSharedType, ConstSharedData.GetMemory());
			}
			else
			{
				ImGui::TextColored(GetElementColor(ConstSharedType), "%s", TCHAR_TO_ANSI(*ConstSharedType->GetName()));
			}
		}
	}

	// Chunk Fragments section
	if (ChunkFragments.Num() > 0)
	{
		ImGui::SeparatorText("Chunk Fragments");
		for (const UScriptStruct* ChunkType : ChunkFragments)
		{
			ImGui::BulletText("%s", TCHAR_TO_ANSI(*ChunkType->GetName()));
		}
	}
#endif
}

void FArcMassEntityDebugger::DrawFragmentProperties(const UScriptStruct* FragmentType, const void* FragmentData)
{
	if (!FragmentType || !FragmentData)
	{
		return;
	}

	ImVec4 Color = GetElementColor(FragmentType);
	FString TreeId = FString::Printf(TEXT("%s##%p"), *FragmentType->GetName(), FragmentData);

	ImGui::PushStyleColor(ImGuiCol_Text, Color);
	bool bOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*TreeId), ImGuiTreeNodeFlags_None, "%s", TCHAR_TO_ANSI(*FragmentType->GetName()));
	ImGui::PopStyleColor();

	if (!bOpen)
	{
		return;
	}

	// Count reflected properties
	int32 PropCount = 0;
	for (TFieldIterator<FProperty> It(FragmentType); It; ++It)
	{
		PropCount++;
	}

	if (PropCount == 0)
	{
		ImGui::TextDisabled("No reflected properties");
		ImGui::TreePop();
		return;
	}

	if (ImGui::BeginTable("##fragprops", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		for (TFieldIterator<FProperty> It(FragmentType); It; ++It)
		{
			FProperty* Prop = *It;
			ImGui::PushID(TCHAR_TO_ANSI(*Prop->GetName()));
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Prop->GetName()));
			ImGui::TableSetColumnIndex(1);
			DrawPropertyValue(Prop, FragmentData);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	ImGui::TreePop();
}
