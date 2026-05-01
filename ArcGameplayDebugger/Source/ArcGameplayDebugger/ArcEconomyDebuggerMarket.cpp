// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "imgui.h"
#include "MassEntitySubsystem.h"
#include "MassDebugger.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "Items/ArcItemDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace Arcx::GameplayDebugger::Economy
{
	extern UWorld* GetDebugWorld();
	extern FMassEntityManager* GetEntityManager();
	extern const ImVec4 DimColor;
} // namespace Arcx::GameplayDebugger::Economy

void FArcEconomyDebugger::RefreshMarketData()
{
#if WITH_MASSENTITY_DEBUG
	CachedMarketEntries.Reset();
	CachedMarketK = 0.3f;

	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];
	if (!Manager->IsEntityActive(Settlement.Entity))
	{
		return;
	}

	FStructView MarketView = Manager->GetFragmentDataStruct(
		Settlement.Entity, FArcSettlementMarketFragment::StaticStruct());
	if (!MarketView.IsValid())
	{
		return;
	}

	const FArcSettlementMarketFragment& Market = MarketView.Get<FArcSettlementMarketFragment>();
	CachedMarketK = Market.K;

	for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& Pair : Market.PriceTable)
	{
		FMarketEntry& Entry = CachedMarketEntries.AddDefaulted_GetRef();
		Entry.ItemDef = Pair.Key;
		Entry.ItemName = Pair.Key ? Pair.Key->GetName() : TEXT("(null)");
		Entry.Price = Pair.Value.Price;
		Entry.Supply = Pair.Value.SupplyCounter;
		Entry.Demand = Pair.Value.DemandCounter;
	}
#endif
}

void FArcEconomyDebugger::RefreshKnowledgeEntries()
{
#if WITH_MASSENTITY_DEBUG
	CachedKnowledgeEntries.Reset();

	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		return;
	}

	UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
	if (!World)
	{
		return;
	}

	UArcKnowledgeSubsystem* KnowledgeSub = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!KnowledgeSub)
	{
		return;
	}

	const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];

	// Get all knowledge entries owned by this settlement entity
	TArray<FArcKnowledgeHandle> SourceHandles;
	KnowledgeSub->GetKnowledgeBySource(Settlement.Entity, SourceHandles);

	for (const FArcKnowledgeHandle& Handle : SourceHandles)
	{
		const FArcKnowledgeEntry* Entry = KnowledgeSub->GetKnowledgeEntry(Handle);
		if (!Entry)
		{
			continue;
		}

		// Demand payload
		const FArcEconomyDemandPayload* DemandPayload = Entry->Payload.GetPtr<FArcEconomyDemandPayload>();
		if (DemandPayload)
		{
			FKnowledgeEntry& KEntry = CachedKnowledgeEntries.AddDefaulted_GetRef();
			KEntry.Type = TEXT("Demand");
			KEntry.ItemName = DemandPayload->ItemDefinition ? DemandPayload->ItemDefinition->GetName() : TEXT("(null)");
			KEntry.Detail = FString::Printf(TEXT("Qty: %d  Price: %.1f"), DemandPayload->QuantityNeeded, DemandPayload->OfferingPrice);
			continue;
		}

		// Supply payload
		const FArcEconomySupplyPayload* SupplyPayload = Entry->Payload.GetPtr<FArcEconomySupplyPayload>();
		if (SupplyPayload)
		{
			FKnowledgeEntry& KEntry = CachedKnowledgeEntries.AddDefaulted_GetRef();
			KEntry.Type = TEXT("Supply");
			KEntry.ItemName = SupplyPayload->ItemDefinition ? SupplyPayload->ItemDefinition->GetName() : TEXT("(null)");
			KEntry.Detail = FString::Printf(TEXT("Qty: %d  Price: %.1f"), SupplyPayload->QuantityAvailable, SupplyPayload->AskingPrice);
			continue;
		}

		// Market snapshot payload
		const FArcEconomyMarketPayload* MarketPayload = Entry->Payload.GetPtr<FArcEconomyMarketPayload>();
		if (MarketPayload)
		{
			FKnowledgeEntry& KEntry = CachedKnowledgeEntries.AddDefaulted_GetRef();
			KEntry.Type = TEXT("Market");
			KEntry.ItemName = TEXT("Snapshot");
			KEntry.Detail = FString::Printf(TEXT("%d items"), MarketPayload->PriceSnapshot.Num());
		}
	}
#endif
}

void FArcEconomyDebugger::DrawMarketTab()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		return;
	}

	const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];

	// Market K
	ImGui::SeparatorText("Market Parameters");
	{
		FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
		if (Manager && Manager->IsEntityActive(Settlement.Entity))
		{
			FStructView MarketView = Manager->GetFragmentDataStruct(
				Settlement.Entity, FArcSettlementMarketFragment::StaticStruct());
			if (MarketView.IsValid())
			{
				FArcSettlementMarketFragment& Market = MarketView.Get<FArcSettlementMarketFragment>();
				ImGui::InputFloat("Market K (volatility)", &Market.K, 0.01f, 0.1f, "%.3f");
				ImGui::InputInt("Total Storage Cap", &Market.TotalStorageCap);
			}
		}
	}

	// Price table
	ImGui::SeparatorText("Price Table");

	if (CachedMarketEntries.IsEmpty())
	{
		ImGui::TextDisabled("No market data");
	}
	else
	{
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable;

		if (ImGui::BeginTable("PriceTable", 4, TableFlags))
		{
			ImGui::TableSetupColumn("Item",   ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
			ImGui::TableSetupColumn("Price",  ImGuiTableColumnFlags_None, 0.0f, 1);
			ImGui::TableSetupColumn("Supply", ImGuiTableColumnFlags_None, 0.0f, 2);
			ImGui::TableSetupColumn("Demand", ImGuiTableColumnFlags_None, 0.0f, 3);
			ImGui::TableHeadersRow();

			// Sort
			ImGuiTableSortSpecs* SortSpecs = ImGui::TableGetSortSpecs();
			if (SortSpecs && SortSpecs->SpecsCount > 0)
			{
				const ImGuiTableColumnSortSpecs& Spec = SortSpecs->Specs[0];
				bool bAscending = (Spec.SortDirection == ImGuiSortDirection_Ascending);
				CachedMarketEntries.Sort([&Spec, bAscending](const FMarketEntry& A, const FMarketEntry& B)
				{
					int32 Cmp = 0;
					switch (Spec.ColumnUserID)
					{
					case 0: Cmp = A.ItemName.Compare(B.ItemName); break;
					case 1: Cmp = (A.Price < B.Price) ? -1 : (A.Price > B.Price) ? 1 : 0; break;
					case 2: Cmp = (A.Supply < B.Supply) ? -1 : (A.Supply > B.Supply) ? 1 : 0; break;
					case 3: Cmp = (A.Demand < B.Demand) ? -1 : (A.Demand > B.Demand) ? 1 : 0; break;
					default: break;
					}
					return bAscending ? Cmp < 0 : Cmp > 0;
				});
			}

			for (const FMarketEntry& Entry : CachedMarketEntries)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", TCHAR_TO_ANSI(*Entry.ItemName));
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.1f", Entry.Price);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.1f", Entry.Supply);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.1f", Entry.Demand);
			}

			ImGui::EndTable();
		}
	}

	// Reset counters
	{
		FMassEntityManager* ResetManager = Arcx::GameplayDebugger::Economy::GetEntityManager();
		if (ResetManager && ResetManager->IsEntityActive(Settlement.Entity))
		{
			FStructView ResetMarketView = ResetManager->GetFragmentDataStruct(
				Settlement.Entity, FArcSettlementMarketFragment::StaticStruct());
			if (ResetMarketView.IsValid())
			{
				if (ImGui::Button("Reset All Supply/Demand Counters"))
				{
					FArcSettlementMarketFragment& ResetMarket = ResetMarketView.Get<FArcSettlementMarketFragment>();
					for (TTuple<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& Pair : ResetMarket.PriceTable)
					{
						Pair.Value.SupplyCounter = 0.0f;
						Pair.Value.DemandCounter = 0.0f;
					}
				}
			}
		}
	}

	// Inject Demand
	ImGui::SeparatorText("Inject Demand");

	if (!CachedMarketEntries.IsEmpty())
	{
		// Build item name list for the combo
		TArray<FString> ItemNames;
		ItemNames.Reserve(CachedMarketEntries.Num());
		for (const FMarketEntry& ME : CachedMarketEntries)
		{
			ItemNames.Add(ME.ItemName);
		}

		// Clamp index in case market shrunk
		InjectDemandItemIdx = FMath::Clamp(InjectDemandItemIdx, 0, ItemNames.Num() - 1);

		FString CurrentItemName = ItemNames[InjectDemandItemIdx];
		if (ImGui::BeginCombo("Item##InjectDemand", TCHAR_TO_ANSI(*CurrentItemName)))
		{
			for (int32 ItemIdx = 0; ItemIdx < ItemNames.Num(); ++ItemIdx)
			{
				const bool bItemSelected = (InjectDemandItemIdx == ItemIdx);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*ItemNames[ItemIdx]), bItemSelected))
				{
					InjectDemandItemIdx = ItemIdx;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::InputInt("Quantity##InjectDemand", &InjectDemandQuantity);
		if (InjectDemandQuantity < 1)
		{
			InjectDemandQuantity = 1;
		}

		if (ImGui::Button("Post Demand"))
		{
			UWorld* DemandWorld = Arcx::GameplayDebugger::Economy::GetDebugWorld();
			if (DemandWorld)
			{
				UArcKnowledgeSubsystem* KnowledgeSub = DemandWorld->GetSubsystem<UArcKnowledgeSubsystem>();
				if (KnowledgeSub && CachedMarketEntries.IsValidIndex(InjectDemandItemIdx))
				{
					const FMarketEntry& SelectedItem = CachedMarketEntries[InjectDemandItemIdx];

					FArcEconomyDemandPayload DemandPayload;
					DemandPayload.ItemDefinition = SelectedItem.ItemDef;
					DemandPayload.QuantityNeeded = InjectDemandQuantity;
					DemandPayload.SettlementHandle = Settlement.Entity;
					DemandPayload.OfferingPrice = SelectedItem.Price;

					FArcKnowledgeEntry DemandEntry;
					DemandEntry.Tags.AddTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Transport);
					DemandEntry.SourceEntity = Settlement.Entity;
					DemandEntry.Relevance = 1.0f;
					DemandEntry.Payload.InitializeAs<FArcEconomyDemandPayload>(DemandPayload);

					KnowledgeSub->PostAdvertisement(DemandEntry);
				}
			}
		}
	}
	else
	{
		ImGui::TextDisabled("No items in market to inject demand for");
	}

	// Force-Set Prices
	ImGui::SeparatorText("Force-Set Prices");

	{
		FMassEntityManager* PriceManager = Arcx::GameplayDebugger::Economy::GetEntityManager();
		if (PriceManager && PriceManager->IsEntityActive(Settlement.Entity))
		{
			FStructView PriceMarketView = PriceManager->GetFragmentDataStruct(
				Settlement.Entity, FArcSettlementMarketFragment::StaticStruct());
			if (PriceMarketView.IsValid())
			{
				FArcSettlementMarketFragment& PriceMarket = PriceMarketView.Get<FArcSettlementMarketFragment>();

				for (FMarketEntry& PriceEntry : CachedMarketEntries)
				{
					if (!PriceEntry.ItemDef)
					{
						continue;
					}
					FArcResourceMarketData* MarketData = PriceMarket.PriceTable.Find(PriceEntry.ItemDef);
					if (!MarketData)
					{
						continue;
					}
					FString PriceLabel = FString::Printf(TEXT("Price##%s"), *PriceEntry.ItemName);
					ImGui::InputFloat(TCHAR_TO_ANSI(*PriceLabel), &MarketData->Price, 0.1f, 1.0f, "%.2f");
				}

				if (PriceMarket.PriceTable.IsEmpty())
				{
					ImGui::TextDisabled("No price table entries");
				}
			}
		}
	}

	// Override Storage Cap
	ImGui::SeparatorText("Storage Cap");

	{
		ImGui::Checkbox("Override Storage Cap", &bOverrideStorageCap);

		FMassEntityManager* CapManager = Arcx::GameplayDebugger::Economy::GetEntityManager();
		if (CapManager && CapManager->IsEntityActive(Settlement.Entity))
		{
			FStructView CapMarketView = CapManager->GetFragmentDataStruct(
				Settlement.Entity, FArcSettlementMarketFragment::StaticStruct());
			if (CapMarketView.IsValid())
			{
				FArcSettlementMarketFragment& CapMarket = CapMarketView.Get<FArcSettlementMarketFragment>();

				if (bOverrideStorageCap)
				{
					ImGui::InputInt("Cap##StorageCap", &OverriddenStorageCap);
					if (OverriddenStorageCap < 0)
					{
						OverriddenStorageCap = 0;
					}
					CapMarket.TotalStorageCap = OverriddenStorageCap;
				}

				const int32 CurrentStorage = CapMarket.CurrentTotalStorage;
				const int32 TotalCap = CapMarket.TotalStorageCap;
				const float StorageFraction = (TotalCap > 0) ? FMath::Clamp(static_cast<float>(CurrentStorage) / static_cast<float>(TotalCap), 0.f, 1.f) : 0.f;
				FString StorageLabel = FString::Printf(TEXT("%d / %d"), CurrentStorage, TotalCap);
				ImGui::ProgressBar(StorageFraction, ImVec2(-1.f, 0.f), TCHAR_TO_ANSI(*StorageLabel));
			}
		}
	}

	// Knowledge entries
	ImGui::SeparatorText("Knowledge Entries");

	if (CachedKnowledgeEntries.IsEmpty())
	{
		ImGui::TextDisabled("No economy knowledge entries for this settlement");
	}
	else
	{
		constexpr ImGuiTableFlags KTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;

		if (ImGui::BeginTable("KnowledgeTable", 3, KTableFlags))
		{
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.f);
			ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			for (const FKnowledgeEntry& KEntry : CachedKnowledgeEntries)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", TCHAR_TO_ANSI(*KEntry.Type));
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s", TCHAR_TO_ANSI(*KEntry.ItemName));
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%s", TCHAR_TO_ANSI(*KEntry.Detail));
			}

			ImGui::EndTable();
		}
	}
#endif
}
