// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/Debugger.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassActorSubsystem.h"
#include "MassSmartObjectBehaviorDefinition.h"
#include "MassSmartObjectFragments.h"
#include "Operations/ArcMassItemOperations.h"
#include "Sim/ArcEconomyNames.h"
#include "Sim/ArcProductionSlots.h"
#include "Sim/ArcSettlementCenter.h"
#include "Sim/ArcWorldSimStores.h"
#include "Sim/ArcWorldSimTags.h"
#include "SmartObjectSubsystem.h"
#include "imgui.h"

namespace ArcEconomyDebuggerBuildingsPrivate
{
	struct FBuildingTypeInfo
	{
		const UScriptStruct* ConfigType = nullptr;
	};

	FString CapabilitySummary(
		const bool bProduction,
		const bool bGathering,
		const bool bItemSource,
		const bool bStorage,
		const bool bItemConsumer,
		const bool bService,
		const bool bStaffing,
		const bool bGatherArea)
	{
		TArray<FString> Names;
		if (bProduction) { Names.Add(TEXT("Production")); }
		if (bGathering) { Names.Add(TEXT("Gathering")); }
		if (bItemSource) { Names.Add(TEXT("ItemSource")); }
		if (bStorage) { Names.Add(TEXT("Storage")); }
		if (bItemConsumer) { Names.Add(TEXT("ItemConsumer")); }
		if (bService) { Names.Add(TEXT("Service")); }
		if (bStaffing) { Names.Add(TEXT("Staffing")); }
		if (bGatherArea) { Names.Add(TEXT("GatherArea")); }
		if (Names.IsEmpty())
		{
			return TEXT("None");
		}
		return FString::Join(Names, TEXT(" + "));
	}

	const char* SlotStateName(ESmartObjectSlotState State)
	{
		if (State == ESmartObjectSlotState::Free)
		{
			return "Free";
		}
		if (State == ESmartObjectSlotState::Claimed)
		{
			return "Claimed";
		}
		if (State == ESmartObjectSlotState::Occupied)
		{
			return "Occupied";
		}
		return "Invalid";
	}

	void CollectDefinitionIds(const FArcMassItemStoreFragment& Store, TArray<FPrimaryAssetId>& OutIds)
	{
		for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
		{
			const FArcItemData* Data = Item.ToItem();
			if (!Data)
			{
				continue;
			}
			const FPrimaryAssetId DefinitionId = Data->GetItemDefinitionId();
			if (DefinitionId.IsValid())
			{
				OutIds.AddUnique(DefinitionId);
			}
		}
	}

	void DrawLiveStoreSection(
		const char* Header,
		const FArcMassItemStoreFragment* Store,
		const FPrimaryAssetId& InfiniteItem)
	{
		ImGui::SeparatorText(Header);

		bool bDrewLine = false;
		if (InfiniteItem.IsValid())
		{
			ImGui::Text("%s xinf", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(InfiniteItem)));
			bDrewLine = true;
		}

		if (Store != nullptr)
		{
			TArray<FPrimaryAssetId> Ids;
			CollectDefinitionIds(*Store, Ids);
			for (const FPrimaryAssetId& Id : Ids)
			{
				const int32 Count = ArcMassItems::CountItemsByDefinition(*Store, Id);
				ImGui::Text("%s x%d", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Id)), Count);
				bDrewLine = true;
			}
		}

		if (!bDrewLine)
		{
			ImGui::TextDisabled("(empty)");
		}
	}

	void DrawLiveStoreSection(
		const FString& Header,
		const FArcMassItemStoreFragment* Store,
		const FPrimaryAssetId& InfiniteItem)
	{
		DrawLiveStoreSection(TCHAR_TO_ANSI(*Header), Store, InfiniteItem);
	}

	const FArcMassItemStoreFragment* LiveItemStore(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct& Type)
	{
		const FConstStructView View = EntityManager.GetSparseElementDataForEntity(&Type, Entity);
		return View.GetPtr<FArcMassItemStoreFragment>();
	}

	FString ItemStoreSectionHeader(const UScriptStruct& Type)
	{
#if WITH_EDITORONLY_DATA
		FString Header = Type.GetDisplayNameText().ToString();
#else
		FString Header;
#endif
		if (Header.IsEmpty())
		{
			Header = Type.GetName();
		}
		return Header;
	}

	FString SlotRole(const FGameplayTagContainer& ActivityTags)
	{
		FString Role;
		if (ActivityTags.HasTagExact(ArcWorldSimTags::Slot_Worker))
		{
			Role += TEXT("Worker ");
		}
		if (ActivityTags.HasTagExact(ArcWorldSimTags::Slot_Pickup))
		{
			Role += TEXT("Pickup ");
		}
		if (ActivityTags.HasTagExact(ArcWorldSimTags::Slot_Drop))
		{
			Role += TEXT("Drop ");
		}
		if (ActivityTags.HasTagExact(ArcWorldSimTags::Slot_Gather))
		{
			Role += TEXT("Gather ");
		}
		return Role.IsEmpty() ? FString(TEXT("(no role)")) : Role.TrimEnd();
	}

	FString BodyOccupantLabel(const FMassEntityView& EntityView, FMassEntityHandle Entity)
	{
		const FMassActorFragment* ActorFragment = EntityView.GetFragmentDataPtr<FMassActorFragment>();
		if (ActorFragment)
		{
			const AActor* Actor = ActorFragment->Get();
			if (Actor)
			{
				return Actor->GetActorNameOrLabel();
			}
		}
		return Entity.DebugGetDescription();
	}

	EArcProductionState AggregateProductionState(bool bAnyProducing, bool bAnyBlocked, bool bAnyStarved)
	{
		if (bAnyProducing)
		{
			return EArcProductionState::Producing;
		}
		if (bAnyBlocked)
		{
			return EArcProductionState::Blocked;
		}
		if (bAnyStarved)
		{
			return EArcProductionState::Starved;
		}
		return EArcProductionState::Unstaffed;
	}

	void SetSlotRecipeIndex(FArcProductionSlot& Slot, int32 NewRecipeIndex)
	{
		if (Slot.RecipeIndex == NewRecipeIndex)
		{
			return;
		}
		Slot.RecipeIndex = NewRecipeIndex;
		// Transferring progress onto a different CraftTime would complete instantly or show >100%.
		Slot.Progress = 0.0f;
	}
}

void FArcEconomyDebugger::RefreshBuildingList()
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityHandle PreviouslySelected;
	if (CachedBuildings.IsValidIndex(SelectedBuildingIdx))
	{
		PreviouslySelected = CachedBuildings[SelectedBuildingIdx].Entity;
	}
	FMassEntityHandle PreviousSpawnTarget;
	if (CachedBuildings.IsValidIndex(SpawnBuildingIdx))
	{
		PreviousSpawnTarget = CachedBuildings[SpawnBuildingIdx].Entity;
	}

	CachedBuildings.Reset();
	SelectedBuildingIdx = INDEX_NONE;
	SpawnBuildingIdx = INDEX_NONE;

	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
	UWorld* World = ArcEconomyDebuggerShared::GetDebugWorld();
	if (!Manager || !World)
	{
		return;
	}

	const USmartObjectSubsystem* SmartObjectSubsystem = World->GetSubsystem<USmartObjectSubsystem>();

	const ArcEconomyDebuggerBuildingsPrivate::FBuildingTypeInfo BuildingTypes[] = {
		{ FArcBuildingTag::StaticStruct() },
		{ FArcGatherAreaConfigFragment::StaticStruct() },
	};

	const TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);
	TMap<FSmartObjectSlotHandle, FMassEntityHandle> OccupantsBySlot;
	TMap<FSmartObjectSlotHandle, FString> OccupantLabelsBySlot;
	const UScriptStruct* BodyType = FArcBodyFragment::StaticStruct();
	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		const FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);
		if (!Composition.Contains(BodyType))
		{
			continue;
		}

		const TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			if (!Manager->IsEntityActive(Entity))
			{
				continue;
			}

			const FMassEntityView EntityView(*Manager, Entity);
			const FMassSmartObjectUserFragment* User = EntityView.GetFragmentDataPtr<FMassSmartObjectUserFragment>();
			if (!User || !User->InteractionHandle.SlotHandle.IsValid())
			{
				continue;
			}

			const FSmartObjectSlotHandle SlotHandle = User->InteractionHandle.SlotHandle;
			OccupantsBySlot.Add(SlotHandle, Entity);
			OccupantLabelsBySlot.Add(SlotHandle, ArcEconomyDebuggerBuildingsPrivate::BodyOccupantLabel(EntityView, Entity));
		}
	}

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		const FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);

		for (const ArcEconomyDebuggerBuildingsPrivate::FBuildingTypeInfo& TypeInfo : BuildingTypes)
		{
			if (!Composition.Contains(TypeInfo.ConfigType))
			{
				continue;
			}

			const TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
			for (const FMassEntityHandle& Entity : Entities)
			{
				if (!Manager->IsEntityActive(Entity))
				{
					continue;
				}

				const FMassEntityView EntityView(*Manager, Entity);

				FBuildingEntry& Entry = CachedBuildings.AddDefaulted_GetRef();
				Entry.Entity = Entity;
				Entry.Name = ArcEconomyNames::BuildingName(*Manager, Entity);
				Entry.bGatherArea = TypeInfo.ConfigType == FArcGatherAreaConfigFragment::StaticStruct();
				Entry.bProduction = EntityView.GetConstSharedFragmentDataPtr<FArcProductionConfigFragment>() != nullptr;
				Entry.bGathering = EntityView.GetConstSharedFragmentDataPtr<FArcGatheringConfigFragment>() != nullptr;
				Entry.bItemSource = EntityView.GetConstSharedFragmentDataPtr<FArcItemSourceConfigFragment>() != nullptr;
				Entry.bStorage = EntityView.GetConstSharedFragmentDataPtr<FArcStorageConfigFragment>() != nullptr;
				Entry.bItemConsumer = EntityView.GetConstSharedFragmentDataPtr<FArcItemConsumerConfigFragment>() != nullptr;
				Entry.bService = EntityView.GetConstSharedFragmentDataPtr<FArcServiceConfigFragment>() != nullptr;
				Entry.bStaffing = EntityView.GetConstSharedFragmentDataPtr<FArcStaffingConfigFragment>() != nullptr;

				const FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>();
				if (Transform)
				{
					Entry.Location = Transform->GetTransform().GetLocation();
				}

				const FArcStaffedFragment* Staffed = EntityView.GetFragmentDataPtr<FArcStaffedFragment>();
				if (Staffed)
				{
					Entry.bHasStaff = true;
					Entry.StaffCount = Staffed->StaffCount;
				}
				const FArcEmploymentCountFragment* Employment =
					EntityView.GetFragmentDataPtr<FArcEmploymentCountFragment>();
				if (Employment)
				{
					Entry.bHasEmployment = true;
					Entry.EmploymentCount = Employment->EmploymentCount;
				}

				if (Entry.bStaffing)
				{
					const FArcSmartObjectDefinitionSharedFragment* Def =
						EntityView.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>();
					if (Def && Def->SmartObjectDefinition)
					{
						Entry.StaffRequired = ArcWorldSim::CountWorkerSlots(*Def->SmartObjectDefinition);
					}
					if (Entry.bGathering && Entry.StaffRequired == 0)
					{
						Entry.StaffRequired = 1;
					}
				}

				const FArcGatheringStateFragment* GatheringState = EntityView.GetFragmentDataPtr<FArcGatheringStateFragment>();
				if (GatheringState)
				{
					Entry.bHasGatheringState = true;
					Entry.GatheringState = GatheringState->State;
				}

				const FArcProductionConfigFragment* ProductionConfig = EntityView.GetConstSharedFragmentDataPtr<FArcProductionConfigFragment>();
				TArray<FSlotEntry> ProductionSlotRows;
				const FArcProductionSlotsFragment* ProductionSlots = EntityView.GetFragmentDataPtr<FArcProductionSlotsFragment>();
				if (ProductionSlots)
				{
					Entry.bHasProductionState = true;
					bool bAnyProducing = false;
					bool bAnyBlocked = false;
					bool bAnyStarved = false;
					for (int32 ProdIndex = 0; ProdIndex < ProductionSlots->Slots.Num(); ++ProdIndex)
					{
						const FArcProductionSlot& ProdSlot = ProductionSlots->Slots[ProdIndex];
						FSlotEntry& ProdRow = ProductionSlotRows.AddDefaulted_GetRef();
						ProdRow.bHasProductionSlot = true;
						ProdRow.ProdSlotIndex = ProdIndex;
						ProdRow.Progress = ProdSlot.Progress;
						ProdRow.HaltDuration = ProdSlot.HaltDuration;
						ProdRow.ProdState = ProdSlot.State;

						FPrimaryAssetId RecipeId;
						if (ProductionConfig && ProductionConfig->Recipes.IsValidIndex(ProdSlot.RecipeIndex))
						{
							RecipeId = ProductionConfig->Recipes[ProdSlot.RecipeIndex].AssetId;
						}
						const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId);
						ProdRow.RecipeName = ArcEconomyNames::RecipeName(RecipeId);
						if (Recipe)
						{
							ProdRow.CraftTime = Recipe->CraftTime;
							++Entry.AssignedSlotCount;
						}

						if (ProdSlot.State == EArcProductionState::Producing)
						{
							bAnyProducing = true;
							++Entry.ProducingSlotCount;
						}
						else if (ProdSlot.State == EArcProductionState::Blocked)
						{
							bAnyBlocked = true;
						}
						else if (ProdSlot.State == EArcProductionState::Starved)
						{
							bAnyStarved = true;
						}
					}
					Entry.ProductionState = ArcEconomyDebuggerBuildingsPrivate::AggregateProductionState(
						bAnyProducing, bAnyBlocked, bAnyStarved);
				}

				const FArcItemSourceConfigFragment* ItemSourceConfig =
					EntityView.GetConstSharedFragmentDataPtr<FArcItemSourceConfigFragment>();
				if (ItemSourceConfig)
				{
					Entry.ItemSourceItem = ItemSourceConfig->ItemDefinition;
				}

				const FArcGatherAreaConfigFragment* AreaConfig = EntityView.GetConstSharedFragmentDataPtr<FArcGatherAreaConfigFragment>();
				if (AreaConfig)
				{
					Entry.AreaItem = AreaConfig->ItemDefinition;
					Entry.AreaTag = AreaConfig->AreaTag;
				}

				const FArcBuildingInputStore* InputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingInputStore>();
				if (InputStore)
				{
					TArray<FPrimaryAssetId> InputIds;
					ArcEconomyDebuggerBuildingsPrivate::CollectDefinitionIds(*InputStore, InputIds);
					for (const FPrimaryAssetId& DefinitionId : InputIds)
					{
						FStoreLine& Line = Entry.Inputs.AddDefaulted_GetRef();
						Line.Item = DefinitionId;
						Line.Count = ArcMassItems::CountItemsByDefinition(*InputStore, DefinitionId);
					}
				}

				const FArcBuildingOutputStore* OutputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
				if (OutputStore)
				{
					TArray<FPrimaryAssetId> OutputIds;
					ArcEconomyDebuggerBuildingsPrivate::CollectDefinitionIds(*OutputStore, OutputIds);
					for (const FPrimaryAssetId& DefinitionId : OutputIds)
					{
						FStoreLine& Line = Entry.Outputs.AddDefaulted_GetRef();
						Line.Item = DefinitionId;
						Line.Count = ArcMassItems::CountItemsByDefinition(*OutputStore, DefinitionId);
					}
				}

				TArray<FSlotEntry> SmartObjectSlotRows;
				const FArcSmartObjectOwnerFragment* SmartObjectOwner = EntityView.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>();
				if (SmartObjectSubsystem && SmartObjectOwner && SmartObjectOwner->SmartObjectHandle.IsValid())
				{
					TArray<FSmartObjectSlotHandle> Slots;
					SmartObjectSubsystem->GetAllSlots(SmartObjectOwner->SmartObjectHandle, Slots);

					for (const FSmartObjectSlotHandle& SlotHandle : Slots)
					{
						FSlotEntry& SlotEntry = SmartObjectSlotRows.AddDefaulted_GetRef();
						SlotEntry.bHasSmartObjectSlot = true;
						SlotEntry.Handle = SlotHandle;
						SlotEntry.State = SmartObjectSubsystem->GetSlotState(SlotHandle);

						SmartObjectSubsystem->ReadSlotData(SlotHandle, [&SlotEntry, &SlotHandle](FConstSmartObjectSlotView SlotView)
						{
							FGameplayTagContainer ActivityTags;
							SlotView.GetActivityTags(ActivityTags);
							SlotEntry.Role = ArcEconomyDebuggerBuildingsPrivate::SlotRole(ActivityTags);
							SlotEntry.bIsWorkerSlot = ActivityTags.HasTagExact(ArcWorldSimTags::Slot_Worker);
							SlotEntry.bHasMassBehavior = SlotView.GetSmartObjectDefinition().GetBehaviorDefinition(
								SlotHandle.GetSlotIndex(), USmartObjectMassBehaviorDefinition::StaticClass()) != nullptr;
						});

						if (const FMassEntityHandle* OccupantHandle = OccupantsBySlot.Find(SlotHandle))
						{
							SlotEntry.Occupant = *OccupantHandle;
							if (const FString* OccupantLabel = OccupantLabelsBySlot.Find(SlotHandle))
							{
								SlotEntry.OccupantLabel = *OccupantLabel;
							}
						}

						if (SlotEntry.bIsWorkerSlot
							&& (SlotEntry.State == ESmartObjectSlotState::Occupied
								|| SlotEntry.State == ESmartObjectSlotState::Claimed))
						{
							++Entry.OccupiedWorkerSlotCount;
						}
					}
				}

				const int32 RowCount = FMath::Max(SmartObjectSlotRows.Num(), ProductionSlotRows.Num());
				Entry.Slots.Reserve(RowCount);
				for (int32 RowIdx = 0; RowIdx < RowCount; ++RowIdx)
				{
					FSlotEntry& Row = Entry.Slots.AddDefaulted_GetRef();
					if (SmartObjectSlotRows.IsValidIndex(RowIdx))
					{
						const FSlotEntry& SoRow = SmartObjectSlotRows[RowIdx];
						Row.bHasSmartObjectSlot = true;
						Row.Handle = SoRow.Handle;
						Row.Occupant = SoRow.Occupant;
						Row.OccupantLabel = SoRow.OccupantLabel;
						Row.Role = SoRow.Role;
						Row.State = SoRow.State;
						Row.bHasMassBehavior = SoRow.bHasMassBehavior;
						Row.bIsWorkerSlot = SoRow.bIsWorkerSlot;
					}
					if (ProductionSlotRows.IsValidIndex(RowIdx))
					{
						const FSlotEntry& ProdRow = ProductionSlotRows[RowIdx];
						Row.bHasProductionSlot = true;
						Row.ProdSlotIndex = ProdRow.ProdSlotIndex;
						Row.RecipeName = ProdRow.RecipeName;
						Row.Progress = ProdRow.Progress;
						Row.CraftTime = ProdRow.CraftTime;
						Row.HaltDuration = ProdRow.HaltDuration;
						Row.ProdState = ProdRow.ProdState;
					}
				}
			}

			break;
		}
	}

	CachedBuildings.Sort([](const FBuildingEntry& A, const FBuildingEntry& B) -> bool
	{
		if (A.bGatherArea != B.bGatherArea)
		{
			return !A.bGatherArea;
		}
		return A.Name.Compare(B.Name) < 0;
	});

	for (int32 Index = 0; Index < CachedBuildings.Num(); ++Index)
	{
		if (PreviouslySelected.IsValid() && CachedBuildings[Index].Entity == PreviouslySelected)
		{
			SelectedBuildingIdx = Index;
		}
		if (PreviousSpawnTarget.IsValid() && CachedBuildings[Index].Entity == PreviousSpawnTarget)
		{
			SpawnBuildingIdx = Index;
		}
	}
#endif
}

void FArcEconomyDebugger::RefreshSettlementList()
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityHandle PreviouslySelected;
	if (CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		PreviouslySelected = CachedSettlements[SelectedSettlementIdx].Entity;
	}

	CachedSettlements.Reset();
	SelectedSettlementIdx = INDEX_NONE;

	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const UScriptStruct* CenterType = FArcSettlementCenterFragment::StaticStruct();
	const UScriptStruct* TransformType = FTransformFragment::StaticStruct();
	const TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);
	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		const FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);
		if (!Composition.Contains(CenterType) || !Composition.Contains(TransformType))
		{
			continue;
		}

		const TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			if (!Manager->IsEntityActive(Entity))
			{
				continue;
			}

			const FMassEntityView EntityView(*Manager, Entity);
			const FArcSettlementCenterFragment* Center = EntityView.GetFragmentDataPtr<FArcSettlementCenterFragment>();
			const FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>();
			if (!Center || !Transform)
			{
				continue;
			}

			FSettlementEntry& Entry = CachedSettlements.AddDefaulted_GetRef();
			Entry.Entity = Entity;
			Entry.Name = ArcEconomyNames::BuildingName(*Manager, Entity);
			Entry.Transform = Transform->GetTransform();
			Entry.Location = Entry.Transform.GetLocation();
			Entry.Extent = Center->Extent;
		}
	}

	for (int32 Index = 0; Index < CachedSettlements.Num(); ++Index)
	{
		if (PreviouslySelected.IsValid() && CachedSettlements[Index].Entity == PreviouslySelected)
		{
			SelectedSettlementIdx = Index;
			break;
		}
	}
#endif
}

void FArcEconomyDebugger::DrawBuildingsTab()
{
#if WITH_MASSENTITY_DEBUG
	if (CachedBuildings.IsEmpty())
	{
		ImGui::TextDisabled("No settlement buildings in this world");
		return;
	}

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float AvailableHeight = ImGui::GetContentRegionAvail().y;
	const float SplitterWidth = 6.0f;
	const float ListWidth = FMath::Max(1.0f, AvailableWidth * BuildingListWidthFraction);

	if (ImGui::BeginChild("BuildingTableChild", ImVec2(ListWidth, AvailableHeight), ImGuiChildFlags_None))
	{
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_Resizable;

		if (ImGui::BeginTable("BuildingTable", 8, TableFlags))
		{
			ImGui::TableSetupColumn("Building", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Capabilities", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 85.0f);
			ImGui::TableSetupColumn("Employment", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Active staff", ImGuiTableColumnFlags_WidthFixed, 75.0f);
			ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Output", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableHeadersRow();

			for (int32 Index = 0; Index < CachedBuildings.Num(); ++Index)
			{
				const FBuildingEntry& Building = CachedBuildings[Index];

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				const FString SelectLabel = FString::Printf(TEXT("%s##building%d"), *Building.Name, Index);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*SelectLabel), SelectedBuildingIdx == Index, ImGuiSelectableFlags_SpanAllColumns))
				{
					SelectedBuildingIdx = Index;
					SelectedGraphNode.Reset();
				}

				ImGui::TableSetColumnIndex(1);
				const FString Capabilities = ArcEconomyDebuggerBuildingsPrivate::CapabilitySummary(
					Building.bProduction,
					Building.bGathering,
					Building.bItemSource,
					Building.bStorage,
					Building.bItemConsumer,
					Building.bService,
					Building.bStaffing,
					Building.bGatherArea);
				ImGui::Text("%s", TCHAR_TO_ANSI(*Capabilities));

				ImGui::TableSetColumnIndex(2);
				if (Building.bHasProductionState)
				{
					ImGui::Text("%s", ArcEconomyDebuggerShared::ProductionStateName(Building.ProductionState));
				}
				else if (Building.bHasGatheringState)
				{
					ImGui::Text("%s", ArcEconomyDebuggerShared::GatheringStateName(Building.GatheringState));
				}
				else
				{
					ImGui::TextDisabled("---");
				}

				ImGui::TableSetColumnIndex(3);
				if (Building.bHasEmployment)
				{
					ImGui::Text("%d/%d", Building.EmploymentCount, Building.StaffRequired);
				}
				else
				{
					ImGui::TextDisabled("---");
				}

				ImGui::TableSetColumnIndex(4);
				if (Building.bHasStaff)
				{
					ImGui::Text("%d", Building.StaffCount);
				}
				else
				{
					ImGui::TextDisabled("---");
				}

				ImGui::TableSetColumnIndex(5);
				if (Building.Inputs.IsEmpty())
				{
					ImGui::TextDisabled("---");
				}
				else
				{
					for (const FStoreLine& Line : Building.Inputs)
					{
						ImGui::Text("%s x%d", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Line.Item)), Line.Count);
					}
				}

				ImGui::TableSetColumnIndex(6);
				if (Building.bItemSource && Building.ItemSourceItem.IsValid())
				{
					ImGui::Text("%s xinf", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Building.ItemSourceItem)));
				}
				else if (Building.bGatherArea && Building.AreaItem.IsValid())
				{
					ImGui::Text("%s xinf", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Building.AreaItem)));
				}
				else if (Building.Outputs.IsEmpty())
				{
					ImGui::TextDisabled("---");
				}
				else
				{
					for (const FStoreLine& Line : Building.Outputs)
					{
						ImGui::Text("%s x%d", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Line.Item)), Line.Count);
					}
				}

				ImGui::TableSetColumnIndex(7);
				if (Building.bHasProductionState)
				{
					ImGui::Text("%d/%d producing", Building.ProducingSlotCount, Building.AssignedSlotCount);
				}
				else
				{
					ImGui::TextDisabled("---");
				}
			}

			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	const ImVec2 SplitterMin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("BuildingSplitter", ImVec2(SplitterWidth, AvailableHeight));
	const ImVec2 SplitterMax(SplitterMin.x + SplitterWidth, SplitterMin.y + AvailableHeight);
	const ImU32 SplitterColor = ImGui::IsItemActive()
		? IM_COL32(180, 180, 180, 255)
		: (ImGui::IsItemHovered() ? IM_COL32(140, 140, 140, 255) : IM_COL32(90, 90, 90, 255));
	ImGui::GetWindowDrawList()->AddRectFilled(SplitterMin, SplitterMax, SplitterColor);
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if (ImGui::IsItemActive() && AvailableWidth > 0.0f)
	{
		BuildingListWidthFraction += ImGui::GetIO().MouseDelta.x / AvailableWidth;
		BuildingListWidthFraction = FMath::Clamp(BuildingListWidthFraction, 0.22f, 0.75f);
	}

	ImGui::SameLine();
	if (ImGui::BeginChild("BuildingDetailChild", ImVec2(0, AvailableHeight), ImGuiChildFlags_None))
	{
		DrawBuildingDetail();
	}
	ImGui::EndChild();
#endif
}

void FArcEconomyDebugger::DrawBuildingDetail()
{
#if WITH_MASSENTITY_DEBUG
	if (!CachedBuildings.IsValidIndex(SelectedBuildingIdx))
	{
		ImGui::TextDisabled("Click a building row to inspect");
		return;
	}

	const FBuildingEntry& Building = CachedBuildings[SelectedBuildingIdx];

	FMassEntityManager* EntityManager = ArcEconomyDebuggerShared::GetEntityManager();
	FArcProductionSlotsFragment* LiveSlots = nullptr;
	const FArcProductionConfigFragment* ProductionConfig = nullptr;
	const FArcMassItemStoreFragment* InputStore = nullptr;
	const FArcMassItemStoreFragment* OutputStore = nullptr;
	TArray<const UScriptStruct*> ExtraStoreTypes;
	if (EntityManager && EntityManager->IsEntityActive(Building.Entity))
	{
		LiveSlots = EntityManager->GetFragmentDataPtr<FArcProductionSlotsFragment>(Building.Entity);
		ProductionConfig = EntityManager->GetConstSharedFragmentDataPtr<FArcProductionConfigFragment>(Building.Entity);

		const UScriptStruct* StoreBase = FArcMassItemStoreFragment::StaticStruct();
		const UScriptStruct* InputType = FArcBuildingInputStore::StaticStruct();
		const UScriptStruct* OutputType = FArcBuildingOutputStore::StaticStruct();

		TArray<const UScriptStruct*> SparseTypes;
		EntityManager->GetSparseElementsBitSetForEntity(Building.Entity).ExportTypes(SparseTypes);
		for (const UScriptStruct* Type : SparseTypes)
		{
			if (!Type || !Type->IsChildOf(StoreBase))
			{
				continue;
			}

			if (Type == InputType)
			{
				InputStore = ArcEconomyDebuggerBuildingsPrivate::LiveItemStore(*EntityManager, Building.Entity, *Type);
			}
			else if (Type == OutputType)
			{
				OutputStore = ArcEconomyDebuggerBuildingsPrivate::LiveItemStore(*EntityManager, Building.Entity, *Type);
			}
			else
			{
				ExtraStoreTypes.Add(Type);
			}
		}
	}

	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", TCHAR_TO_ANSI(*Building.Name));
	const FString Capabilities = ArcEconomyDebuggerBuildingsPrivate::CapabilitySummary(
		Building.bProduction,
		Building.bGathering,
		Building.bItemSource,
		Building.bStorage,
		Building.bItemConsumer,
		Building.bService,
		Building.bStaffing,
		Building.bGatherArea);
	ImGui::Text("Capabilities: %s", TCHAR_TO_ANSI(*Capabilities));
	ImGui::Text("Entity: %s", TCHAR_TO_ANSI(*Building.Entity.DebugGetDescription()));
	ImGui::Text("Location: (%.0f, %.0f, %.0f)", Building.Location.X, Building.Location.Y, Building.Location.Z);
	if (Building.bHasStaff || Building.bHasEmployment || Building.bHasProductionState)
	{
		ImGui::Text("Active staff: %d", Building.StaffCount);
		ImGui::Text("Employment: %d / %d", Building.EmploymentCount, Building.StaffRequired);
	}
	if (LiveSlots)
	{
		ImGui::Checkbox("Production Enabled", &LiveSlots->bProductionEnabled);
	}

	if (Building.bHasProductionState
		&& Building.OccupiedWorkerSlotCount != Building.ProducingSlotCount)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
			"Mismatch: occupied Worker slots %d != producing %d",
			Building.OccupiedWorkerSlotCount, Building.ProducingSlotCount);
	}

	FPrimaryAssetId InfiniteOutput;
	if (Building.bItemSource)
	{
		InfiniteOutput = Building.ItemSourceItem;
	}
	else if (Building.bGatherArea)
	{
		InfiniteOutput = Building.AreaItem;
	}
	ArcEconomyDebuggerBuildingsPrivate::DrawLiveStoreSection("Input", InputStore, FPrimaryAssetId());
	ArcEconomyDebuggerBuildingsPrivate::DrawLiveStoreSection("Output", OutputStore, InfiniteOutput);
	if (EntityManager)
	{
		for (const UScriptStruct* Type : ExtraStoreTypes)
		{
			const FArcMassItemStoreFragment* ExtraStore =
				ArcEconomyDebuggerBuildingsPrivate::LiveItemStore(*EntityManager, Building.Entity, *Type);
			ArcEconomyDebuggerBuildingsPrivate::DrawLiveStoreSection(
				ArcEconomyDebuggerBuildingsPrivate::ItemStoreSectionHeader(*Type),
				ExtraStore,
				FPrimaryAssetId());
		}
	}

	ImGui::SeparatorText("Slots");

	bool bHasSmartObjectSlot = false;
	for (const FSlotEntry& Slot : Building.Slots)
	{
		if (Slot.bHasSmartObjectSlot)
		{
			bHasSmartObjectSlot = true;
			break;
		}
	}
	if (!bHasSmartObjectSlot)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "NO SMART OBJECT SLOTS (required)");
	}

	if (Building.Slots.IsEmpty())
	{
		return;
	}

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
		| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX;
	if (ImGui::BeginTable("BuildingSlots", 8, TableFlags))
	{
		// Slot and Prod columns are independent numberings (claim order vs production gate order).
		ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("SO State", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Occupant", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Prod", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Recipe", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 110.0f);
		ImGui::TableSetupColumn("ProdState", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Halt", ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableHeadersRow();

		for (const FSlotEntry& Slot : Building.Slots)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			if (Slot.bHasSmartObjectSlot)
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*Slot.Role));
			}
			else
			{
				ImGui::TextDisabled("no worker slot");
			}

			ImGui::TableSetColumnIndex(1);
			if (Slot.bHasSmartObjectSlot)
			{
				ImGui::Text("%s", ArcEconomyDebuggerBuildingsPrivate::SlotStateName(Slot.State));
			}
			else
			{
				ImGui::TextDisabled("---");
			}

			ImGui::TableSetColumnIndex(2);
			if (!Slot.bHasSmartObjectSlot)
			{
				ImGui::TextDisabled("---");
			}
			else if (Slot.OccupantLabel.IsEmpty())
			{
				ImGui::TextUnformatted("\xE2\x80\x94");
			}
			else
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*Slot.OccupantLabel));
			}

			ImGui::TableSetColumnIndex(3);
			if (Slot.bHasProductionSlot)
			{
				ImGui::Text("%d", Slot.ProdSlotIndex);
			}
			else
			{
				ImGui::TextDisabled("no production slot");
			}

			ImGui::TableSetColumnIndex(4);
			if (Slot.bHasProductionSlot && LiveSlots && LiveSlots->Slots.IsValidIndex(Slot.ProdSlotIndex))
			{
				FArcProductionSlot& LiveSlot = LiveSlots->Slots[Slot.ProdSlotIndex];
				ImGui::PushID(Slot.ProdSlotIndex);

				FPrimaryAssetId CurrentRecipeId;
				if (ProductionConfig && ProductionConfig->Recipes.IsValidIndex(LiveSlot.RecipeIndex))
				{
					CurrentRecipeId = ProductionConfig->Recipes[LiveSlot.RecipeIndex].AssetId;
				}
				const FString PreviewName = ArcEconomyNames::RecipeName(CurrentRecipeId);
				if (ImGui::BeginCombo("##recipe", TCHAR_TO_ANSI(*PreviewName)))
				{
					if (ImGui::Selectable("(none)", LiveSlot.RecipeIndex == INDEX_NONE))
					{
						ArcEconomyDebuggerBuildingsPrivate::SetSlotRecipeIndex(LiveSlot, INDEX_NONE);
					}

					if (ProductionConfig)
					{
						for (int32 RecipeIdx = 0; RecipeIdx < ProductionConfig->Recipes.Num(); ++RecipeIdx)
						{
							const FPrimaryAssetId& RecipeId = ProductionConfig->Recipes[RecipeIdx].AssetId;
							if (ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId) == nullptr)
							{
								continue;
							}

							ImGui::PushID(RecipeIdx);
							const FString RecipeLabel = ArcEconomyNames::RecipeName(RecipeId);
							if (ImGui::Selectable(TCHAR_TO_ANSI(*RecipeLabel), LiveSlot.RecipeIndex == RecipeIdx))
							{
								ArcEconomyDebuggerBuildingsPrivate::SetSlotRecipeIndex(LiveSlot, RecipeIdx);
							}
							ImGui::PopID();
						}
					}
					ImGui::EndCombo();
				}

				ImGui::PopID();
			}
			else if (Slot.bHasProductionSlot)
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*Slot.RecipeName));
			}
			else
			{
				ImGui::TextDisabled("---");
			}

			ImGui::TableSetColumnIndex(5);
			if (Slot.bHasProductionSlot && Slot.CraftTime > 0.0f)
			{
				const FString Overlay = FString::Printf(TEXT("%.1f / %.1fs"), Slot.Progress, Slot.CraftTime);
				ImGui::ProgressBar(Slot.Progress / Slot.CraftTime, ImVec2(-1.0f, 0.0f), TCHAR_TO_ANSI(*Overlay));
			}
			else if (Slot.bHasProductionSlot)
			{
				ImGui::TextDisabled("---");
			}

			ImGui::TableSetColumnIndex(6);
			if (Slot.bHasProductionSlot)
			{
				ImGui::Text("%s", ArcEconomyDebuggerShared::ProductionStateName(Slot.ProdState));
			}
			else
			{
				ImGui::TextDisabled("---");
			}

			ImGui::TableSetColumnIndex(7);
			if (Slot.bHasProductionSlot)
			{
				ImGui::Text("%.1f", Slot.HaltDuration);
			}
			else
			{
				ImGui::TextDisabled("---");
			}
		}

		ImGui::EndTable();
	}
#endif
}
