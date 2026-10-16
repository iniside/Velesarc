// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Sim/ArcEconomyNames.h"
#include "Sim/ArcSettlementLedgerSubsystem.h"
#include "imgui.h"

namespace ArcEconomyDebuggerChainView
{
	constexpr float BoxWidth = 240.0f;
	constexpr float RowHeight = 18.0f;
	constexpr float BoxTitleHeight = 19.0f;
	constexpr float BoxPaddingY = 5.0f;
	constexpr float LayerSpacingX = 300.0f;
	constexpr float BoxSpacingY = 30.0f;
	constexpr float CanvasOffsetX = 40.0f;
	constexpr float CanvasOffsetY = 20.0f;
	constexpr float ArrowSize = 8.0f;
	constexpr float PinRadius = 5.0f;
	constexpr float RowTextInset = 12.0f;
	constexpr float RowInsetX = 2.0f;
	constexpr float DragThreshold = 4.0f;

	constexpr ImU32 UpstreamColor = IM_COL32(255, 150, 40, 255);
	constexpr ImU32 DownstreamColor = IM_COL32(200, 90, 220, 255);
	constexpr ImU32 SelectedColor = IM_COL32(255, 255, 255, 255);
	constexpr ImU32 BoxFillColor = IM_COL32(226, 226, 226, 240);
	constexpr ImU32 BoxBorderColor = IM_COL32(70, 70, 70, 220);
	constexpr ImU32 BoxTitleColor = IM_COL32(0, 0, 0, 255);
	constexpr ImU32 RowTextColor = IM_COL32(0, 0, 0, 255);
	constexpr ImU32 BaseEdgeColor = IM_COL32(255, 255, 255, 180);
	constexpr ImU32 InFlightEdgeColor = IM_COL32(255, 210, 60, 235);
	constexpr ImU32 EdgeLabelColor = IM_COL32(255, 255, 200, 220);
	constexpr float DimAlphaScale = 0.28f;
	constexpr float RelationBorderThickness = 2.0f;
	constexpr float RowFillAlphaScale = 0.6f;
	constexpr float HoverRingThickness = 1.5f;

	enum class ERelation : uint8
	{
		NoSelection,
		Selected,
		Upstream,
		Downstream,
		Unrelated
	};

	struct FRowGeometry
	{
		ImVec2 Min = ImVec2(0.0f, 0.0f);
		ImVec2 Max = ImVec2(0.0f, 0.0f);
		ImVec2 PinPos = ImVec2(0.0f, 0.0f);
	};

	ImU32 ScaleAlpha(ImU32 Color, float Scale)
	{
		const ImU32 AlphaMask = static_cast<ImU32>(0xFF) << IM_COL32_A_SHIFT;
		const float SourceAlpha = static_cast<float>((Color & AlphaMask) >> IM_COL32_A_SHIFT);
		const ImU32 NewAlpha = static_cast<ImU32>(FMath::Clamp(SourceAlpha * Scale, 0.0f, 255.0f));
		return (Color & ~AlphaMask) | (NewAlpha << IM_COL32_A_SHIFT);
	}

	void GatherReachable(const TArray<TArray<int32>>& Adjacency, int32 StartIdx, TSet<int32>& OutReached)
	{
		if (!Adjacency.IsValidIndex(StartIdx))
		{
			return;
		}

		TSet<int32> Visited;
		Visited.Add(StartIdx);

		TArray<int32> Queue;
		Queue.Add(StartIdx);

		int32 Head = 0;
		while (Head < Queue.Num())
		{
			const int32 CurrentIdx = Queue[Head++];
			for (int32 NextIdx : Adjacency[CurrentIdx])
			{
				if (Visited.Contains(NextIdx))
				{
					continue;
				}
				Visited.Add(NextIdx);
				Queue.Add(NextIdx);
				OutReached.Add(NextIdx);
			}
		}
	}

	ERelation ClassifyNode(int32 Idx, int32 SelectedIdx, const TSet<int32>& UpstreamSet, const TSet<int32>& DownstreamSet)
	{
		if (SelectedIdx == INDEX_NONE)
		{
			return ERelation::NoSelection;
		}
		if (Idx == SelectedIdx)
		{
			return ERelation::Selected;
		}
		if (UpstreamSet.Contains(Idx))
		{
			return ERelation::Upstream;
		}
		if (DownstreamSet.Contains(Idx))
		{
			return ERelation::Downstream;
		}
		return ERelation::Unrelated;
	}

	ERelation ClassifyEdge(int32 IdxA, int32 IdxB, int32 SelectedIdx, const TSet<int32>& UpstreamSet, const TSet<int32>& DownstreamSet)
	{
		if (SelectedIdx == INDEX_NONE)
		{
			return ERelation::NoSelection;
		}
		if ((IdxA == SelectedIdx || UpstreamSet.Contains(IdxA))
			&& (IdxB == SelectedIdx || UpstreamSet.Contains(IdxB)))
		{
			return ERelation::Upstream;
		}
		if ((IdxA == SelectedIdx || DownstreamSet.Contains(IdxA))
			&& (IdxB == SelectedIdx || DownstreamSet.Contains(IdxB)))
		{
			return ERelation::Downstream;
		}
		return ERelation::Unrelated;
	}

	ImU32 ApplyRelationColor(ImU32 BaseColor, ERelation Relation)
	{
		switch (Relation)
		{
		case ERelation::Selected:   return SelectedColor;
		case ERelation::Upstream:   return UpstreamColor;
		case ERelation::Downstream: return DownstreamColor;
		case ERelation::Unrelated:  return ScaleAlpha(BaseColor, DimAlphaScale);
		case ERelation::NoSelection:
		default:                    return BaseColor;
		}
	}

	int32 LayerForKind(EArcChainNodeKind Kind)
	{
		switch (Kind)
		{
		case EArcChainNodeKind::Gathering: return 0;
		case EArcChainNodeKind::ItemSource: return 0;
		case EArcChainNodeKind::GatherArea: return 0;
		case EArcChainNodeKind::Production: return 1;
		case EArcChainNodeKind::Storage:   return 2;
		case EArcChainNodeKind::ItemConsumer: return 3;
		default:                           return 1;
		}
	}

	const TCHAR* KindName(EArcChainNodeKind Kind)
	{
		switch (Kind)
		{
		case EArcChainNodeKind::Production: return TEXT("Production");
		case EArcChainNodeKind::Gathering: return TEXT("Gathering");
		case EArcChainNodeKind::ItemSource: return TEXT("ItemSource");
		case EArcChainNodeKind::Storage: return TEXT("Storage");
		case EArcChainNodeKind::ItemConsumer: return TEXT("ItemConsumer");
		case EArcChainNodeKind::GatherArea: return TEXT("GatherArea");
		default: return TEXT("Unknown");
		}
	}

	void DrawArrowHead(ImDrawList* DrawList, const ImVec2& Tip, const ImVec2& Direction, ImU32 Color)
	{
		const float Length = FMath::Sqrt(Direction.x * Direction.x + Direction.y * Direction.y);
		if (Length <= 0.0f)
		{
			return;
		}
		const ImVec2 Dir(Direction.x / Length, Direction.y / Length);
		const ImVec2 Perp(-Dir.y, Dir.x);
		const ImVec2 Left(Tip.x - Dir.x * ArrowSize + Perp.x * ArrowSize * 0.5f,
			Tip.y - Dir.y * ArrowSize + Perp.y * ArrowSize * 0.5f);
		const ImVec2 Right(Tip.x - Dir.x * ArrowSize - Perp.x * ArrowSize * 0.5f,
			Tip.y - Dir.y * ArrowSize - Perp.y * ArrowSize * 0.5f);
		DrawList->AddTriangleFilled(Tip, Left, Right, Color);
	}

	struct FGatherAreaRecord
	{
		FGameplayTag AreaTag;
		FPrimaryAssetId ItemDefinition;
		FVector Location = FVector::ZeroVector;
	};

	FPrimaryAssetId NearestAreaItem(const TArray<FGatherAreaRecord>& Areas, const FGameplayTag& AreaTag, const FVector& FromLocation)
	{
		FPrimaryAssetId NearestItem;
		double BestDistSq = 0.0;
		bool bFound = false;
		for (const FGatherAreaRecord& Record : Areas)
		{
			if (Record.AreaTag != AreaTag)
			{
				continue;
			}

			const double DistSq = FVector::DistSquared(FromLocation, Record.Location);
			if (!bFound || DistSq < BestDistSq)
			{
				bFound = true;
				BestDistSq = DistSq;
				NearestItem = Record.ItemDefinition;
			}
		}
		return NearestItem;
	}
}

void FArcEconomyDebugger::RefreshProductionChain()
{
#if WITH_MASSENTITY_DEBUG
	ChainNodes.Reset();
	ChainEdges.Reset();
	ChainDiagnostics.Reset();

	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	UArcSettlementLedgerSubsystem* Ledger = ArcEconomyDebuggerShared::GetLedger();

	TArray<ArcEconomyDebuggerChainView::FGatherAreaRecord> GatherAreas;
	for (const FBuildingEntry& AreaBuilding : CachedBuildings)
	{
		if (!AreaBuilding.bGatherArea)
		{
			continue;
		}

		ArcEconomyDebuggerChainView::FGatherAreaRecord& Record = GatherAreas.AddDefaulted_GetRef();
		Record.AreaTag = AreaBuilding.AreaTag;
		Record.ItemDefinition = AreaBuilding.AreaItem;
		Record.Location = AreaBuilding.Location;
	}

	for (const FBuildingEntry& Building : CachedBuildings)
	{
		if (!Manager->IsEntityActive(Building.Entity))
		{
			continue;
		}

		TArray<EArcChainNodeKind> Kinds;
		if (Building.bProduction) { Kinds.Add(EArcChainNodeKind::Production); }
		if (Building.bGathering) { Kinds.Add(EArcChainNodeKind::Gathering); }
		if (Building.bItemSource) { Kinds.Add(EArcChainNodeKind::ItemSource); }
		if (Building.bStorage) { Kinds.Add(EArcChainNodeKind::Storage); }
		if (Building.bItemConsumer) { Kinds.Add(EArcChainNodeKind::ItemConsumer); }
		if (Building.bGatherArea) { Kinds.Add(EArcChainNodeKind::GatherArea); }

		const FMassEntityView EntityView(*Manager, Building.Entity);
		for (const EArcChainNodeKind Kind : Kinds)
		{
			FArcChainViewNode& Node = ChainNodes.AddDefaulted_GetRef();
			Node.Key.Entity = Building.Entity;
			Node.Key.Kind = Kind;
			Node.Name = Building.Name;

			if (Kind == EArcChainNodeKind::Production)
			{
				const FArcProductionConfigFragment* Config = EntityView.GetConstSharedFragmentDataPtr<FArcProductionConfigFragment>();
				const FArcProductionSlotsFragment* Production = EntityView.GetFragmentDataPtr<FArcProductionSlotsFragment>();
				if (!Config || !Production)
				{
					continue;
				}
				for (const FArcProductionSlot& Slot : Production->Slots)
				{
					if (!Config->Recipes.IsValidIndex(Slot.RecipeIndex))
					{
						continue;
					}
					const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(Config->Recipes[Slot.RecipeIndex].AssetId);
					if (Recipe == nullptr)
					{
						continue;
					}
					for (const FInstancedStruct& IngredientStruct : Recipe->Ingredients)
					{
						const FArcRecipeIngredient* Ingredient = IngredientStruct.GetPtr<FArcRecipeIngredient>();
						if (Ingredient != nullptr && Ingredient->bOptional)
						{
							continue;
						}

						if (const FArcRecipeIngredient_ItemDef* ItemDef = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>())
						{
							const FPrimaryAssetId Id = ItemDef->ItemDefinitionId;
							if (Id.IsValid()) { Node.Inputs.AddUnique(Id); }
						}
						else if (const FArcRecipeIngredient_Tags* Tags = IngredientStruct.GetPtr<FArcRecipeIngredient_Tags>())
						{
							if (Ledger)
							{
								TArray<FPrimaryAssetId> TaggedIds;
								Ledger->CollectItemIdsForAssetTags(Tags->RequiredTags, Tags->DenyTags, TaggedIds);
								for (const FPrimaryAssetId& Id : TaggedIds) { Node.Inputs.AddUnique(Id); }
							}
						}
					}
					for (const FArcRecipeOutput& Output : Recipe->Outputs)
					{
						const FPrimaryAssetId OutputId = Output.ItemDefinition;
						if (OutputId.IsValid()) { Node.Outputs.AddUnique(OutputId); }
					}
				}
			}
			else if (Kind == EArcChainNodeKind::Gathering)
			{
				const FArcGatheringConfigFragment* Config = EntityView.GetConstSharedFragmentDataPtr<FArcGatheringConfigFragment>();
				if (Config)
				{
					const FPrimaryAssetId NearestItem = ArcEconomyDebuggerChainView::NearestAreaItem(
						GatherAreas, Config->AreaTag, Building.Location);
					if (NearestItem.IsValid()) { Node.Outputs.AddUnique(NearestItem); }
				}
			}
			else if (Kind == EArcChainNodeKind::ItemSource)
			{
				const FArcItemSourceConfigFragment* Config = EntityView.GetConstSharedFragmentDataPtr<FArcItemSourceConfigFragment>();
				if (Config)
				{
					const FPrimaryAssetId ItemId = Config->ItemDefinition;
					if (ItemId.IsValid()) { Node.Outputs.AddUnique(ItemId); }
				}
			}
			else if (Kind == EArcChainNodeKind::ItemConsumer)
			{
				const FArcItemConsumerConfigFragment* Config = EntityView.GetConstSharedFragmentDataPtr<FArcItemConsumerConfigFragment>();
				if (Config)
				{
					for (const FArcNamedPrimaryAssetId& Wanted : Config->WantedItems)
					{
						const FPrimaryAssetId ItemId = Wanted;
						if (ItemId.IsValid()) { Node.Inputs.AddUnique(ItemId); }
					}
				}
			}
			else if (Kind == EArcChainNodeKind::GatherArea && Building.AreaItem.IsValid())
			{
				Node.Outputs.AddUnique(Building.AreaItem);
			}
		}
	}

	for (int32 FromIdx = 0; FromIdx < ChainNodes.Num(); ++FromIdx)
	{
		if (ChainNodes[FromIdx].Key.Kind == EArcChainNodeKind::Storage)
		{
			continue;
		}

		for (int32 ToIdx = 0; ToIdx < ChainNodes.Num(); ++ToIdx)
		{
			if (ToIdx == FromIdx)
			{
				continue;
			}

			for (const FPrimaryAssetId& Item : ChainNodes[FromIdx].Outputs)
			{
				// The chain is what the recipes declare, so stock in the ledger never adds or removes an edge.
				if (!ChainNodes[ToIdx].Inputs.Contains(Item))
				{
					continue;
				}

				FArcChainViewEdge& Edge = ChainEdges.AddDefaulted_GetRef();
				Edge.FromIdx = FromIdx;
				Edge.ToIdx = ToIdx;
				Edge.Item = Item;
			}
		}
	}

	for (const FNPCEntry& NPC : CachedNPCs)
	{
		if (!NPC.bHasTransportJob)
		{
			continue;
		}

		const EArcChainNodeKind SourceOrder[] = {
			EArcChainNodeKind::Production,
			EArcChainNodeKind::Gathering,
			EArcChainNodeKind::ItemSource,
			EArcChainNodeKind::GatherArea,
		};
		const EArcChainNodeKind SinkOrder[] = {
			EArcChainNodeKind::Production,
			EArcChainNodeKind::ItemConsumer,
		};

		int32 FromIdx = INDEX_NONE;
		for (const EArcChainNodeKind Kind : SourceOrder)
		{
			TArray<int32> Matches;
			for (int32 NodeIdx = 0; NodeIdx < ChainNodes.Num(); ++NodeIdx)
			{
				const FArcChainViewNode& Node = ChainNodes[NodeIdx];
				if (Node.Key.Entity == NPC.Source && Node.Key.Kind == Kind && Node.Outputs.Contains(NPC.Item))
				{
					Matches.Add(NodeIdx);
				}
			}
			if (!Matches.IsEmpty())
			{
				FromIdx = Matches[0];
				if (Matches.Num() > 1)
				{
					ChainDiagnostics.AddUnique(FString::Printf(TEXT("Ambiguous source %s for %s at capability %s"),
						*NPC.Source.DebugGetDescription(), *NPC.Item.ToString(), ArcEconomyDebuggerChainView::KindName(Kind)));
				}
				break;
			}
		}
		if (FromIdx == INDEX_NONE)
		{
			TArray<int32> StorageMatches;
			for (int32 NodeIdx = 0; NodeIdx < ChainNodes.Num(); ++NodeIdx)
			{
				const FArcChainViewNode& Node = ChainNodes[NodeIdx];
				if (Node.Key.Entity == NPC.Source && Node.Key.Kind == EArcChainNodeKind::Storage)
				{
					StorageMatches.Add(NodeIdx);
				}
			}
			if (!StorageMatches.IsEmpty())
			{
				FromIdx = StorageMatches[0];
				if (StorageMatches.Num() > 1)
				{
					ChainDiagnostics.AddUnique(FString::Printf(TEXT("Ambiguous source %s for %s at capability Storage"),
						*NPC.Source.DebugGetDescription(), *NPC.Item.ToString()));
				}
			}
		}

		int32 ToIdx = INDEX_NONE;
		for (const EArcChainNodeKind Kind : SinkOrder)
		{
			TArray<int32> Matches;
			for (int32 NodeIdx = 0; NodeIdx < ChainNodes.Num(); ++NodeIdx)
			{
				const FArcChainViewNode& Node = ChainNodes[NodeIdx];
				if (Node.Key.Entity == NPC.Sink && Node.Key.Kind == Kind && Node.Inputs.Contains(NPC.Item))
				{
					Matches.Add(NodeIdx);
				}
			}
			if (!Matches.IsEmpty())
			{
				ToIdx = Matches[0];
				if (Matches.Num() > 1)
				{
					ChainDiagnostics.AddUnique(FString::Printf(TEXT("Ambiguous sink %s for %s at capability %s"),
						*NPC.Sink.DebugGetDescription(), *NPC.Item.ToString(), ArcEconomyDebuggerChainView::KindName(Kind)));
				}
				break;
			}
		}
		if (ToIdx == INDEX_NONE)
		{
			TArray<int32> StorageMatches;
			for (int32 NodeIdx = 0; NodeIdx < ChainNodes.Num(); ++NodeIdx)
			{
				const FArcChainViewNode& Node = ChainNodes[NodeIdx];
				if (Node.Key.Entity == NPC.Sink && Node.Key.Kind == EArcChainNodeKind::Storage)
				{
					StorageMatches.Add(NodeIdx);
				}
			}
			if (!StorageMatches.IsEmpty())
			{
				ToIdx = StorageMatches[0];
				if (StorageMatches.Num() > 1)
				{
					ChainDiagnostics.AddUnique(FString::Printf(TEXT("Ambiguous sink %s for %s at capability Storage"),
						*NPC.Sink.DebugGetDescription(), *NPC.Item.ToString()));
				}
			}
		}

		if (FromIdx == INDEX_NONE || ToIdx == INDEX_NONE)
		{
			continue;
		}

		FArcChainViewEdge& Edge = ChainEdges.AddDefaulted_GetRef();
		Edge.FromIdx = FromIdx;
		Edge.ToIdx = ToIdx;
		Edge.Item = NPC.Item;
		Edge.bInFlight = true;
	}
#endif
}

void FArcEconomyDebugger::DrawProductionChainTab()
{
#if WITH_MASSENTITY_DEBUG
	using namespace ArcEconomyDebuggerChainView;

	if (ChainNodes.IsEmpty())
	{
		ImGui::TextDisabled("No settlement buildings to chain");
		return;
	}

	ImGui::TextDisabled("Click a node:");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "selected");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.0f, 0.59f, 0.16f, 1.0f), "upstream (it requires)");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.78f, 0.35f, 0.86f, 1.0f), "downstream (requires it)");
	ImGui::SameLine();
	if (ImGui::Button("Reset layout"))
	{
		ManualBoxPositions.Reset();
		DraggedBoxKey.Reset();
		bDragExceededThreshold = false;
		bPanningCanvas = false;
		GraphViewOffset = FVector2D::ZeroVector;
	}
	for (const FString& Diagnostic : ChainDiagnostics)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "%s", TCHAR_TO_ANSI(*Diagnostic));
	}

	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	if (CanvasSize.x < 50.0f || CanvasSize.y < 50.0f)
	{
		return;
	}

	const int32 NumNodes = ChainNodes.Num();

	const ImVec2 CanvasOrigin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("ProductionChainCanvas", CanvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
	const bool bCanvasHovered = ImGui::IsItemHovered();
	const bool bCanvasClicked = ImGui::IsItemClicked();
	const bool bCanvasActive = ImGui::IsItemActive();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 MousePos = ImGui::GetMousePos();

	if (bCanvasActive && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
	{
		const ImVec2 MouseDelta = ImGui::GetIO().MouseDelta;
		GraphViewOffset.X += static_cast<double>(MouseDelta.x);
		GraphViewOffset.Y += static_cast<double>(MouseDelta.y);
	}

	if (bPanningCanvas && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const FVector2D PressDisplacement(
			static_cast<double>(MousePos.x) - DragPressMousePos.X,
			static_cast<double>(MousePos.y) - DragPressMousePos.Y);
		if (PressDisplacement.Size() > static_cast<double>(DragThreshold))
		{
			bDragExceededThreshold = true;
		}

		if (bDragExceededThreshold)
		{
			const ImVec2 MouseDelta = ImGui::GetIO().MouseDelta;
			GraphViewOffset.X += static_cast<double>(MouseDelta.x);
			GraphViewOffset.Y += static_cast<double>(MouseDelta.y);
		}
	}

	const ImVec2 ViewOrigin(
		CanvasOrigin.x + static_cast<float>(GraphViewOffset.X),
		CanvasOrigin.y + static_cast<float>(GraphViewOffset.Y));

	if (DraggedBoxKey.IsValid() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const FVector2D PressDisplacement(
			static_cast<double>(MousePos.x) - DragPressMousePos.X,
			static_cast<double>(MousePos.y) - DragPressMousePos.Y);
		if (PressDisplacement.Size() > static_cast<double>(DragThreshold))
		{
			bDragExceededThreshold = true;
		}

		if (bDragExceededThreshold)
		{
			FVector2D* Manual = ManualBoxPositions.Find(DraggedBoxKey);
			if (!Manual)
			{
				Manual = &ManualBoxPositions.Add(DraggedBoxKey, DragStartPos);
			}

			const ImVec2 MouseDelta = ImGui::GetIO().MouseDelta;
			Manual->X += static_cast<double>(MouseDelta.x);
			Manual->Y += static_cast<double>(MouseDelta.y);
		}
	}

	TArray<ImVec2> BoxOrigin;
	TArray<float> BoxHeight;
	BoxOrigin.SetNum(NumNodes);
	BoxHeight.SetNumZeroed(NumNodes);

	TArray<TArray<FRowGeometry>> InputRows;
	TArray<TArray<FRowGeometry>> OutputRows;
	InputRows.SetNum(NumNodes);
	OutputRows.SetNum(NumNodes);

	TArray<float> LayerCursorY;
	LayerCursorY.Init(CanvasOffsetY, 4);

	for (int32 NodeIdx = 0; NodeIdx < NumNodes; ++NodeIdx)
	{
		const FArcChainViewNode& Node = ChainNodes[NodeIdx];
		const int32 Layer = LayerForKind(Node.Key.Kind);
		const float NodeHeight = BoxPaddingY * 2.0f + BoxTitleHeight
			+ static_cast<float>(Node.Inputs.Num() + Node.Outputs.Num()) * RowHeight;

		const FVector2D AutoPos(
			static_cast<double>(CanvasOffsetX + static_cast<float>(Layer) * LayerSpacingX),
			static_cast<double>(LayerCursorY[Layer]));
		LayerCursorY[Layer] += NodeHeight + BoxSpacingY;

		const FVector2D* Manual = ManualBoxPositions.Find(Node.Key);
		const FVector2D LocalPos = Manual ? *Manual : AutoPos;

		BoxOrigin[NodeIdx] = ImVec2(
			ViewOrigin.x + static_cast<float>(LocalPos.X),
			ViewOrigin.y + static_cast<float>(LocalPos.Y));
		BoxHeight[NodeIdx] = NodeHeight;

		float CursorY = BoxOrigin[NodeIdx].y + BoxPaddingY + BoxTitleHeight;

		InputRows[NodeIdx].SetNum(Node.Inputs.Num());
		for (int32 RowIdx = 0; RowIdx < Node.Inputs.Num(); ++RowIdx)
		{
			FRowGeometry& Geometry = InputRows[NodeIdx][RowIdx];
			Geometry.Min = ImVec2(BoxOrigin[NodeIdx].x + RowInsetX, CursorY);
			Geometry.Max = ImVec2(BoxOrigin[NodeIdx].x + BoxWidth - RowInsetX, CursorY + RowHeight);
			Geometry.PinPos = ImVec2(BoxOrigin[NodeIdx].x, CursorY + RowHeight * 0.5f);
			CursorY += RowHeight;
		}

		OutputRows[NodeIdx].SetNum(Node.Outputs.Num());
		for (int32 RowIdx = 0; RowIdx < Node.Outputs.Num(); ++RowIdx)
		{
			FRowGeometry& Geometry = OutputRows[NodeIdx][RowIdx];
			Geometry.Min = ImVec2(BoxOrigin[NodeIdx].x + RowInsetX, CursorY);
			Geometry.Max = ImVec2(BoxOrigin[NodeIdx].x + BoxWidth - RowInsetX, CursorY + RowHeight);
			Geometry.PinPos = ImVec2(BoxOrigin[NodeIdx].x + BoxWidth, CursorY + RowHeight * 0.5f);
			CursorY += RowHeight;
		}
	}

	HoveredNodeKey.Reset();
	int32 HoveredGeometryIdx = INDEX_NONE;
	if (bCanvasHovered)
	{
		for (int32 NodeIdx = 0; NodeIdx < NumNodes; ++NodeIdx)
		{
			if (MousePos.x >= BoxOrigin[NodeIdx].x - PinRadius && MousePos.x <= BoxOrigin[NodeIdx].x + BoxWidth + PinRadius
				&& MousePos.y >= BoxOrigin[NodeIdx].y && MousePos.y <= BoxOrigin[NodeIdx].y + BoxHeight[NodeIdx])
			{
				HoveredGeometryIdx = NodeIdx;
				HoveredNodeKey = ChainNodes[NodeIdx].Key;
			}
		}
	}

	if (bCanvasClicked && !DraggedBoxKey.IsValid())
	{
		if (HoveredGeometryIdx != INDEX_NONE)
		{
			DraggedBoxKey = HoveredNodeKey;
			bDragExceededThreshold = false;
			DragPressMousePos = FVector2D(static_cast<double>(MousePos.x), static_cast<double>(MousePos.y));
			DragStartPos = FVector2D(
				BoxOrigin[HoveredGeometryIdx].x - ViewOrigin.x,
				BoxOrigin[HoveredGeometryIdx].y - ViewOrigin.y);
		}
		else
		{
			bPanningCanvas = true;
			bDragExceededThreshold = false;
			DragPressMousePos = FVector2D(static_cast<double>(MousePos.x), static_cast<double>(MousePos.y));
		}
	}

	if (DraggedBoxKey.IsValid() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		if (!bDragExceededThreshold)
		{
			SelectedGraphNode = DraggedBoxKey;
			SelectedBuildingIdx = FindBuildingIndex(DraggedBoxKey.Entity);
		}
		DraggedBoxKey.Reset();
		bDragExceededThreshold = false;
	}

	if (bPanningCanvas && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		if (!bDragExceededThreshold)
		{
			SelectedBuildingIdx = INDEX_NONE;
			SelectedGraphNode.Reset();
		}
		bPanningCanvas = false;
		bDragExceededThreshold = false;
	}

	int32 SelectedNodeIdx = INDEX_NONE;
	if (SelectedGraphNode.IsValid())
	{
		for (int32 NodeIdx = 0; NodeIdx < NumNodes; ++NodeIdx)
		{
			if (ChainNodes[NodeIdx].Key == SelectedGraphNode)
			{
				SelectedNodeIdx = NodeIdx;
				break;
			}
		}
	}

	TSet<int32> UpstreamSet;
	TSet<int32> DownstreamSet;
	if (SelectedNodeIdx != INDEX_NONE)
	{
		TArray<TArray<int32>> UpstreamAdjacency;
		TArray<TArray<int32>> DownstreamAdjacency;
		UpstreamAdjacency.SetNum(NumNodes);
		DownstreamAdjacency.SetNum(NumNodes);

		for (const FArcChainViewEdge& Edge : ChainEdges)
		{
			if (Edge.bInFlight || !UpstreamAdjacency.IsValidIndex(Edge.FromIdx) || !UpstreamAdjacency.IsValidIndex(Edge.ToIdx))
			{
				continue;
			}
			UpstreamAdjacency[Edge.ToIdx].AddUnique(Edge.FromIdx);
			DownstreamAdjacency[Edge.FromIdx].AddUnique(Edge.ToIdx);
		}

		GatherReachable(UpstreamAdjacency, SelectedNodeIdx, UpstreamSet);
		GatherReachable(DownstreamAdjacency, SelectedNodeIdx, DownstreamSet);
	}

	const ImVec2 CanvasMax(CanvasOrigin.x + CanvasSize.x, CanvasOrigin.y + CanvasSize.y);
	DrawList->AddRectFilled(CanvasOrigin, CanvasMax, IM_COL32(30, 30, 30, 255));
	DrawList->PushClipRect(CanvasOrigin, CanvasMax, true);

	for (const FArcChainViewEdge& Edge : ChainEdges)
	{
		if (!ChainNodes.IsValidIndex(Edge.FromIdx) || !ChainNodes.IsValidIndex(Edge.ToIdx))
		{
			continue;
		}

		const int32 FromRowIdx = ChainNodes[Edge.FromIdx].Outputs.IndexOfByKey(Edge.Item);
		const int32 ToRowIdx = ChainNodes[Edge.ToIdx].Inputs.IndexOfByKey(Edge.Item);

		const ImVec2 Start = OutputRows[Edge.FromIdx].IsValidIndex(FromRowIdx)
			? OutputRows[Edge.FromIdx][FromRowIdx].PinPos
			: ImVec2(BoxOrigin[Edge.FromIdx].x + BoxWidth, BoxOrigin[Edge.FromIdx].y + BoxHeight[Edge.FromIdx] * 0.5f);
		const ImVec2 End = InputRows[Edge.ToIdx].IsValidIndex(ToRowIdx)
			? InputRows[Edge.ToIdx][ToRowIdx].PinPos
			: ImVec2(BoxOrigin[Edge.ToIdx].x, BoxOrigin[Edge.ToIdx].y + BoxHeight[Edge.ToIdx] * 0.5f);

		const ERelation EdgeRelation = ClassifyEdge(
			Edge.FromIdx, Edge.ToIdx, SelectedNodeIdx, UpstreamSet, DownstreamSet);
		const ImU32 EdgeColor = Edge.bInFlight
			? InFlightEdgeColor
			: ApplyRelationColor(BaseEdgeColor, EdgeRelation);

		const float Slack = FMath::Max(45.0f, FMath::Abs(End.x - Start.x) * 0.4f);
		const ImVec2 StartTangent(Start.x + Slack, Start.y);
		const ImVec2 EndTangent(End.x - Slack, End.y);

		DrawList->AddBezierCubic(Start, StartTangent, EndTangent, End, EdgeColor, Edge.bInFlight ? 2.5f : 1.5f, 0);
		DrawArrowHead(DrawList, End, ImVec2(End.x - EndTangent.x, End.y - EndTangent.y), EdgeColor);

		const ImVec2 MidPoint((Start.x + End.x) * 0.5f, (Start.y + End.y) * 0.5f - 10.0f);
		const FString EdgeLabel = ArcEconomyNames::ItemName(Edge.Item);
		DrawList->AddText(MidPoint,
			Edge.bInFlight ? InFlightEdgeColor : ApplyRelationColor(EdgeLabelColor, EdgeRelation),
			TCHAR_TO_ANSI(*EdgeLabel));
	}

	for (int32 NodeIdx = 0; NodeIdx < NumNodes; ++NodeIdx)
	{
		const FArcChainViewNode& Node = ChainNodes[NodeIdx];
		const ImVec2 RectMin = BoxOrigin[NodeIdx];
		const ImVec2 RectMax(RectMin.x + BoxWidth, RectMin.y + BoxHeight[NodeIdx]);

		DrawList->AddRectFilled(RectMin, RectMax, BoxFillColor, 4.0f);
		DrawList->AddRect(RectMin, RectMax, BoxBorderColor, 4.0f);

		const ERelation NodeRelation = ClassifyNode(NodeIdx, SelectedNodeIdx, UpstreamSet, DownstreamSet);
		const bool bDimmed = (NodeRelation == ERelation::Unrelated);

		const int32 BuildingIdx = FindBuildingIndex(Node.Key.Entity);
		ImU32 StatusColor = IM_COL32(140, 140, 140, 255);
		FString StatusText(TEXT("---"));
		if (CachedBuildings.IsValidIndex(BuildingIdx))
		{
			const FBuildingEntry& Building = CachedBuildings[BuildingIdx];
			if (Node.Key.Kind == EArcChainNodeKind::Production && Building.bHasProductionState)
			{
				StatusText = ArcEconomyDebuggerShared::ProductionStateName(Building.ProductionState);
				switch (Building.ProductionState)
				{
				case EArcProductionState::Producing: StatusColor = IM_COL32(80, 180, 80, 255); break;
				case EArcProductionState::Starved:   StatusColor = IM_COL32(210, 70, 70, 255); break;
				case EArcProductionState::Blocked:   StatusColor = IM_COL32(220, 190, 40, 255); break;
				default:                             StatusColor = IM_COL32(140, 140, 140, 255); break;
				}
			}
			else if (Node.Key.Kind == EArcChainNodeKind::Gathering && Building.bHasGatheringState)
			{
				StatusText = ArcEconomyDebuggerShared::GatheringStateName(Building.GatheringState);
				switch (Building.GatheringState)
				{
				case EArcGatheringState::Gathering: StatusColor = IM_COL32(80, 180, 80, 255); break;
				case EArcGatheringState::Blocked:   StatusColor = IM_COL32(220, 190, 40, 255); break;
				default:                            StatusColor = IM_COL32(140, 140, 140, 255); break;
				}
			}
			else
			{
				StatusColor = IM_COL32(70, 150, 200, 255);
			}
		}

		const FString TitleStr = FString::Printf(TEXT("%s / %s [%s]"),
			*Node.Name, KindName(Node.Key.Kind), *StatusText);
		DrawList->AddText(ImVec2(RectMin.x + 6.0f, RectMin.y + BoxPaddingY), BoxTitleColor, TCHAR_TO_ANSI(*TitleStr));

		if (NodeRelation == ERelation::Selected || NodeRelation == ERelation::Upstream
			|| NodeRelation == ERelation::Downstream)
		{
			DrawList->AddRect(RectMin, RectMax,
				ApplyRelationColor(StatusColor, NodeRelation), 4.0f, RelationBorderThickness);
		}

		if (HoveredNodeKey == Node.Key)
		{
			DrawList->AddRect(RectMin, RectMax, IM_COL32(255, 255, 255, 255), 4.0f, HoverRingThickness);
		}

		const ImU32 RowFillColor = ScaleAlpha(StatusColor,
			bDimmed ? RowFillAlphaScale * DimAlphaScale : RowFillAlphaScale);
		const ImU32 PinColor = bDimmed ? ScaleAlpha(StatusColor, DimAlphaScale) : StatusColor;
		const ImU32 TextColor = bDimmed ? ScaleAlpha(RowTextColor, DimAlphaScale) : RowTextColor;

		for (int32 RowIdx = 0; RowIdx < Node.Inputs.Num(); ++RowIdx)
		{
			const FRowGeometry& Geometry = InputRows[NodeIdx][RowIdx];
			DrawList->AddRectFilled(Geometry.Min, Geometry.Max, RowFillColor, 3.0f);
			DrawList->AddCircleFilled(Geometry.PinPos, PinRadius, PinColor, 12);

			const FString RowLabel = ArcEconomyNames::ItemName(Node.Inputs[RowIdx]);
			DrawList->AddText(ImVec2(Geometry.Min.x + RowTextInset, Geometry.Min.y + 1.0f),
				TextColor, TCHAR_TO_ANSI(*RowLabel));
		}

		for (int32 RowIdx = 0; RowIdx < Node.Outputs.Num(); ++RowIdx)
		{
			const FRowGeometry& Geometry = OutputRows[NodeIdx][RowIdx];
			DrawList->AddRectFilled(Geometry.Min, Geometry.Max, RowFillColor, 3.0f);
			DrawList->AddCircleFilled(Geometry.PinPos, PinRadius, PinColor, 12);

			const FString RowLabel = ArcEconomyNames::ItemName(Node.Outputs[RowIdx]);
			const ImVec2 LabelSize = ImGui::CalcTextSize(TCHAR_TO_ANSI(*RowLabel));
			DrawList->AddText(ImVec2(Geometry.Max.x - RowTextInset - LabelSize.x, Geometry.Min.y + 1.0f),
				TextColor, TCHAR_TO_ANSI(*RowLabel));
		}
	}

	DrawList->PopClipRect();

	if (ChainNodes.IsValidIndex(HoveredGeometryIdx))
	{
		const FArcChainViewNode& Node = ChainNodes[HoveredGeometryIdx];
		ImGui::BeginTooltip();
		ImGui::Text("Building: %s", TCHAR_TO_ANSI(*Node.Name));
		ImGui::Text("Capability: %s", TCHAR_TO_ANSI(KindName(Node.Key.Kind)));
		ImGui::Text("Entity: %s", TCHAR_TO_ANSI(*Node.Key.Entity.DebugGetDescription()));
		ImGui::Text("Inputs: %d  Outputs: %d", Node.Inputs.Num(), Node.Outputs.Num());
		ImGui::EndTooltip();
	}
#endif
}
