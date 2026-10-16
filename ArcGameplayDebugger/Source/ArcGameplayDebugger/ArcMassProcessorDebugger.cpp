// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassProcessorDebugger.h"

#include "imgui.h"

#if WITH_MASSENTITY_DEBUG
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Mass/Debugger.h"
#include "Mass/EntityHandle.h"
#include "Mass/PhaseCoordinator.h"
#include "Mass/ProcessingTypes.h"
#include "Mass/Processor.h"
#include "MassEntitySettings.h"
#include "MassProcessingPhaseManager.h"
#include "MassSignalProcessorBase.h"
#include "MassSignalSubsystem.h"
#include "MassSimulationSubsystem.h"
#include "Misc/ScopeLock.h"

namespace Arcx::GameplayDebugger::MassProcessor
{
	constexpr float LayerSpacing = 260.f;
	constexpr float RowSpacing = 90.f;
	constexpr float PhaseColumnGap = 120.f;
	constexpr float NodeWidth = 200.f;
	constexpr float NodeHeight = 48.f;
	constexpr float ArrowSize = 10.f;
	constexpr float TangentLength = 60.f;
	constexpr float MinTextZoom = 0.35f;
	constexpr float SignalFlashSeconds = 3.0f;

	constexpr ImU32 BackgroundColor = IM_COL32(24, 24, 28, 255);
	constexpr ImU32 InactiveColor = IM_COL32(70, 70, 70, 255);
	constexpr ImU32 ActiveColor = IM_COL32(60, 110, 170, 255);
	constexpr ImU32 SignalColor = IM_COL32(130, 80, 180, 255);
	constexpr ImU32 SignalFlashColor = IM_COL32(250, 170, 60, 255);
	constexpr ImU32 EdgeColor = IM_COL32(120, 120, 130, 200);
	constexpr ImU32 IncomingEdgeColor = IM_COL32(90, 200, 120, 255);
	constexpr ImU32 OutgoingEdgeColor = IM_COL32(230, 150, 60, 255);
	constexpr ImU32 BorderColor = IM_COL32(15, 15, 18, 255);
	constexpr ImU32 SelectedBorderColor = IM_COL32(255, 255, 255, 255);
	constexpr ImU32 HoveredBorderColor = IM_COL32(200, 200, 200, 255);
	constexpr ImU32 UnscheduledBorderColor = IM_COL32(150, 150, 150, 255);
	constexpr ImU32 NodeTextColor = IM_COL32(235, 235, 235, 255);
	constexpr ImU32 DimTextColor = IM_COL32(150, 150, 150, 255);
	constexpr ImU32 PhaseLabelColor = IM_COL32(180, 180, 200, 255);

	const TCHAR* PhaseLabel(const int32 PhaseIndex)
	{
		switch (static_cast<EMassProcessingPhase>(PhaseIndex))
		{
		case EMassProcessingPhase::PrePhysics: return TEXT("PrePhysics");
		case EMassProcessingPhase::StartPhysics: return TEXT("StartPhysics");
		case EMassProcessingPhase::DuringPhysics: return TEXT("DuringPhysics");
		case EMassProcessingPhase::EndPhysics: return TEXT("EndPhysics");
		case EMassProcessingPhase::PostPhysics: return TEXT("PostPhysics");
		case EMassProcessingPhase::FrameEnd: return TEXT("FrameEnd");
		default: return nullptr;
		}
	}

	ImU32 LerpColor(const ImU32 From, const ImU32 To, const float Alpha)
	{
		const ImVec4 FromColor = ImGui::ColorConvertU32ToFloat4(From);
		const ImVec4 ToColor = ImGui::ColorConvertU32ToFloat4(To);
		const float T = FMath::Clamp(Alpha, 0.f, 1.f);
		return ImGui::ColorConvertFloat4ToU32(ImVec4(
			FMath::Lerp(FromColor.x, ToColor.x, T),
			FMath::Lerp(FromColor.y, ToColor.y, T),
			FMath::Lerp(FromColor.z, ToColor.z, T),
			FMath::Lerp(FromColor.w, ToColor.w, T)));
	}

	void AddArrowHead(ImDrawList* DrawList, const ImVec2& Tip, const ImVec2& Direction, const ImU32 Color, const float Size)
	{
		const float Length = FMath::Sqrt(Direction.x * Direction.x + Direction.y * Direction.y);
		if (Length <= UE_SMALL_NUMBER)
		{
			return;
		}

		const ImVec2 Forward(Direction.x / Length, Direction.y / Length);
		const ImVec2 Side(-Forward.y, Forward.x);
		const ImVec2 Left(Tip.x - Forward.x * Size + Side.x * Size * 0.5f, Tip.y - Forward.y * Size + Side.y * Size * 0.5f);
		const ImVec2 Right(Tip.x - Forward.x * Size - Side.x * Size * 0.5f, Tip.y - Forward.y * Size - Side.y * Size * 0.5f);
		DrawList->AddTriangleFilled(Tip, Left, Right, Color);
	}

	void AddDashedLine(ImDrawList* DrawList, const ImVec2& From, const ImVec2& To, const ImU32 Color, const float Thickness, const float DashLength)
	{
		const float DeltaX = To.x - From.x;
		const float DeltaY = To.y - From.y;
		const float Length = FMath::Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
		if (Length <= UE_SMALL_NUMBER || DashLength <= UE_SMALL_NUMBER)
		{
			return;
		}

		const ImVec2 Direction(DeltaX / Length, DeltaY / Length);
		for (float Start = 0.f; Start < Length; Start += DashLength * 2.f)
		{
			const float End = FMath::Min(Start + DashLength, Length);
			DrawList->AddLine(
				ImVec2(From.x + Direction.x * Start, From.y + Direction.y * Start),
				ImVec2(From.x + Direction.x * End, From.y + Direction.y * End),
				Color,
				Thickness);
		}
	}

	void AddDashedRect(ImDrawList* DrawList, const ImVec2& RectMin, const ImVec2& RectMax, const ImU32 Color, const float Thickness, const float DashLength)
	{
		AddDashedLine(DrawList, RectMin, ImVec2(RectMax.x, RectMin.y), Color, Thickness, DashLength);
		AddDashedLine(DrawList, ImVec2(RectMax.x, RectMin.y), RectMax, Color, Thickness, DashLength);
		AddDashedLine(DrawList, RectMax, ImVec2(RectMin.x, RectMax.y), Color, Thickness, DashLength);
		AddDashedLine(DrawList, ImVec2(RectMin.x, RectMax.y), RectMin, Color, Thickness, DashLength);
	}

	void DrawLegendEntry(const char* Id, const char* Label, const ImU32 Color)
	{
		ImGui::ColorButton(Id, ImGui::ColorConvertU32ToFloat4(Color),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(14.f, 14.f));
		ImGui::SameLine();
		ImGui::TextUnformatted(Label);
	}

	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	UMassSimulationSubsystem* GetSimulationSubsystem()
	{
		UWorld* World = GetDebugWorld();
		return World ? World->GetSubsystem<UMassSimulationSubsystem>() : nullptr;
	}

	int32 CountHostedProcessors(const UMassCompositeProcessor& Composite)
	{
		int32 Count = 0;
		for (const TObjectPtr<UMassProcessor>& Child : FMassDebugger::GetHostedProcessors(Composite))
		{
			if (!Child)
			{
				continue;
			}
			++Count;
			if (const UMassCompositeProcessor* ChildComposite = Cast<UMassCompositeProcessor>(Child.Get()))
			{
				Count += CountHostedProcessors(*ChildComposite);
			}
		}
		return Count;
	}
} // namespace Arcx::GameplayDebugger::MassProcessor

void FArcMassProcessorDebugger::Initialize()
{
	Nodes.Reset();
	Edges.Reset();
	GraphDiagnostics.Reset();
	NodeByProcessor.Reset();
	VisitedComposites.Reset();
	ManualPositions.Reset();
	{
		FScopeLock Lock(&SignalTimesCS);
		LastSignalTimes.Reset();
	}
	CachedPhaseRoots.Reset();
	CachedProcessorCount = 0;
	SelectedNodeIndex = INDEX_NONE;
	HoveredNodeIndex = INDEX_NONE;
	DraggedNodeIndex = INDEX_NONE;
	SelectedProcessorKey = TObjectKey<UMassProcessor>();
	CachedMatchedEntities.Reset();
	CachedEntitiesProcessorKey = TObjectKey<UMassProcessor>();
	bMatchedEntitiesValid = false;
	bRebuildPending = true;

	RefreshIfNeeded();
}

void FArcMassProcessorDebugger::Uninitialize()
{
	UnbindSignalHooks();

	Nodes.Reset();
	Edges.Reset();
	GraphDiagnostics.Reset();
	NodeByProcessor.Reset();
	VisitedComposites.Reset();
	ManualPositions.Reset();
	{
		FScopeLock Lock(&SignalTimesCS);
		LastSignalTimes.Reset();
	}
	CachedPhaseRoots.Reset();
	CachedProcessorCount = 0;
	SelectedNodeIndex = INDEX_NONE;
	HoveredNodeIndex = INDEX_NONE;
	DraggedNodeIndex = INDEX_NONE;
	SelectedProcessorKey = TObjectKey<UMassProcessor>();
	CachedMatchedEntities.Reset();
	CachedEntitiesProcessorKey = TObjectKey<UMassProcessor>();
	bMatchedEntitiesValid = false;
	bRebuildPending = true;
}

void FArcMassProcessorDebugger::RefreshIfNeeded()
{
	if (!bRebuildPending && !IsGraphStale())
	{
		return;
	}

	// FSignalDelegate is not thread-safe and SignalEntities broadcasts from worker threads
	UMassSimulationSubsystem* Simulation = Arcx::GameplayDebugger::MassProcessor::GetSimulationSubsystem();
	if (!Simulation || Simulation->IsDuringMassProcessing())
	{
		bRebuildPending = true;
		return;
	}

	UnbindSignalHooks();
	RebuildGraph();
	BindSignalHooks();
	bRebuildPending = false;
}

void FArcMassProcessorDebugger::RebuildGraph()
{
	Nodes.Reset();
	Edges.Reset();
	GraphDiagnostics.Reset();
	NodeByProcessor.Reset();
	VisitedComposites.Reset();
	CachedPhaseRoots.Reset();
	CachedProcessorCount = 0;
	SelectedNodeIndex = INDEX_NONE;
	HoveredNodeIndex = INDEX_NONE;
	DraggedNodeIndex = INDEX_NONE;
	bMatchedEntitiesValid = false;

	UMassSimulationSubsystem* Simulation = Arcx::GameplayDebugger::MassProcessor::GetSimulationSubsystem();
	if (!Simulation)
	{
		return;
	}

	const UE::Mass::FPhaseCoordinator& Coordinator = Simulation->GetPhaseManager().GetPhaseCoordinator();
	for (int32 PhaseIndex = 0; PhaseIndex < Coordinator.GetNumPhases(); ++PhaseIndex)
	{
		const UMassCompositeProcessor* PhaseProcessor = Coordinator.GetPhaseProcessor(Coordinator.GetPhaseHandle(PhaseIndex));
		if (!PhaseProcessor)
		{
			continue;
		}

		CachedPhaseRoots.Emplace(PhaseProcessor);
		CachedProcessorCount += Arcx::GameplayDebugger::MassProcessor::CountHostedProcessors(*PhaseProcessor);

		const int32 NodeCountBeforePhase = Nodes.Num();
		CollectProcessors(*PhaseProcessor, PhaseIndex);

		if (Nodes.Num() > NodeCountBeforePhase && FMassDebugger::GetProcessingGraph(*PhaseProcessor).IsEmpty())
		{
			GraphDiagnostics.Add(FString::Printf(TEXT("phase %d: dependency graph unavailable"), PhaseIndex));
		}
	}

	TSet<TPair<int32, const UClass*>> ClassesByPhase;
	for (const FArcMassProcessorNode& Node : Nodes)
	{
		if (const UMassProcessor* Processor = Node.Processor.Get())
		{
			ClassesByPhase.Add(TPair<int32, const UClass*>(Node.PhaseIndex, Processor->GetClass()));
		}
	}

	TConstArrayView<FMassProcessingPhaseConfig> PhaseConfigs = GetMutableDefault<UMassEntitySettings>()->GetProcessingPhasesConfig();
	for (int32 ConfigIndex = 0; ConfigIndex < PhaseConfigs.Num(); ++ConfigIndex)
	{
		for (const TObjectPtr<UMassProcessor>& ProcessorCDO : PhaseConfigs[ConfigIndex].ProcessorCDOs)
		{
			if (!ProcessorCDO)
			{
				continue;
			}
			if (ClassesByPhase.Contains(TPair<int32, const UClass*>(ConfigIndex, ProcessorCDO->GetClass())))
			{
				continue;
			}

			FArcMassProcessorNode& Node = Nodes.AddDefaulted_GetRef();
			Node.Processor = ProcessorCDO.Get();
			Node.DisplayName = ProcessorCDO->GetProcessorName();
			Node.Name = FName(*Node.DisplayName);
			Node.GroupName = ProcessorCDO->GetExecutionOrder().ExecuteInGroup;
			Node.PhaseIndex = ConfigIndex;
			Node.bScheduled = false;
			Node.bSignalProcessor = ProcessorCDO->IsA<UMassSignalProcessorBase>();
			if (const UMassSignalProcessorBase* SignalProcessor = Cast<UMassSignalProcessorBase>(ProcessorCDO.Get()))
			{
				Node.RegisteredSignals = SignalProcessor->GetRegisteredSignals();
			}

			NodeByProcessor.Add(ProcessorCDO.Get(), Nodes.Num() - 1);
			ClassesByPhase.Add(TPair<int32, const UClass*>(ConfigIndex, ProcessorCDO->GetClass()));
		}
	}

	for (const UMassCompositeProcessor* Composite : VisitedComposites)
	{
		if (Composite)
		{
			CollectEdges(*Composite);
		}
	}

	BuildLayout();

	if (SelectedProcessorKey != TObjectKey<UMassProcessor>())
	{
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			if (TObjectKey<UMassProcessor>(Nodes[NodeIndex].Processor.Get()) == SelectedProcessorKey)
			{
				SelectedNodeIndex = NodeIndex;
				break;
			}
		}
	}
}

void FArcMassProcessorDebugger::CollectProcessors(const UMassCompositeProcessor& Composite, const int32 PhaseIndex)
{
	VisitedComposites.Add(&Composite);

	for (const TObjectPtr<UMassProcessor>& Child : FMassDebugger::GetHostedProcessors(Composite))
	{
		UMassProcessor* Processor = Child.Get();
		if (!Processor)
		{
			continue;
		}

		if (const UMassCompositeProcessor* ChildComposite = Cast<UMassCompositeProcessor>(Processor))
		{
			CollectProcessors(*ChildComposite, PhaseIndex);
			continue;
		}

		if (NodeByProcessor.Contains(Processor))
		{
			continue;
		}

		FArcMassProcessorNode& Node = Nodes.AddDefaulted_GetRef();
		Node.Processor = Processor;
		Node.DisplayName = Processor->GetProcessorName();
		Node.Name = FName(*Node.DisplayName);
		Node.GroupName = Processor->GetExecutionOrder().ExecuteInGroup;
		Node.PhaseIndex = PhaseIndex;
		Node.bScheduled = true;
		Node.bSignalProcessor = Processor->IsA<UMassSignalProcessorBase>();
		if (const UMassSignalProcessorBase* SignalProcessor = Cast<UMassSignalProcessorBase>(Processor))
		{
			Node.RegisteredSignals = SignalProcessor->GetRegisteredSignals();
		}

		NodeByProcessor.Add(Processor, Nodes.Num() - 1);
	}
}

void FArcMassProcessorDebugger::CollectEdges(const UMassCompositeProcessor& Composite)
{
	TConstArrayView<UMassCompositeProcessor::FDependencyNode> Graph = FMassDebugger::GetProcessingGraph(Composite);

	TSet<TPair<int32, int32>> UniqueEdges;
	for (const FArcMassProcessorEdge& Edge : Edges)
	{
		UniqueEdges.Add(TPair<int32, int32>(Edge.FromIndex, Edge.ToIndex));
	}

	for (int32 GraphIndex = 0; GraphIndex < Graph.Num(); ++GraphIndex)
	{
		const UMassProcessor* Successor = Graph[GraphIndex].Processor;
		if (!Successor)
		{
			continue;
		}

		const int32* SuccessorNodeIndex = NodeByProcessor.Find(Successor);
		if (!SuccessorNodeIndex)
		{
			continue;
		}

		for (const int32 DependencyIndex : Graph[GraphIndex].Dependencies)
		{
			if (!Graph.IsValidIndex(DependencyIndex))
			{
				continue;
			}

			const UMassProcessor* Predecessor = Graph[DependencyIndex].Processor;
			if (!Predecessor)
			{
				continue;
			}

			const int32* PredecessorNodeIndex = NodeByProcessor.Find(Predecessor);
			if (!PredecessorNodeIndex || *PredecessorNodeIndex == *SuccessorNodeIndex)
			{
				continue;
			}

			const TPair<int32, int32> Candidate(*PredecessorNodeIndex, *SuccessorNodeIndex);
			if (UniqueEdges.Contains(Candidate))
			{
				continue;
			}
			UniqueEdges.Add(Candidate);

			FArcMassProcessorEdge& Edge = Edges.AddDefaulted_GetRef();
			Edge.FromIndex = Candidate.Key;
			Edge.ToIndex = Candidate.Value;
		}
	}
}

void FArcMassProcessorDebugger::BuildLayout()
{
	using namespace Arcx::GameplayDebugger::MassProcessor;

	for (FArcMassProcessorNode& Node : Nodes)
	{
		Node.Layer = 0;
	}

	for (int32 Iteration = 0; Iteration < Nodes.Num(); ++Iteration)
	{
		bool bChanged = false;
		for (const FArcMassProcessorEdge& Edge : Edges)
		{
			const int32 CandidateLayer = Nodes[Edge.FromIndex].Layer + 1;
			if (Nodes[Edge.ToIndex].Layer < CandidateLayer)
			{
				Nodes[Edge.ToIndex].Layer = CandidateLayer;
				bChanged = true;
			}
		}
		if (!bChanged)
		{
			break;
		}
	}

	TMap<int32, TArray<int32>> NodesByPhase;
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		NodesByPhase.FindOrAdd(Nodes[NodeIndex].PhaseIndex).Add(NodeIndex);
	}

	TArray<int32> PhaseOrder;
	NodesByPhase.GenerateKeyArray(PhaseOrder);
	PhaseOrder.Sort();

	float ColumnOffset = 0.f;
	for (const int32 PhaseIndex : PhaseOrder)
	{
		const TArray<int32>& PhaseNodes = NodesByPhase[PhaseIndex];

		int32 MaxScheduledLayer = 0;
		for (const int32 NodeIndex : PhaseNodes)
		{
			if (Nodes[NodeIndex].bScheduled)
			{
				MaxScheduledLayer = FMath::Max(MaxScheduledLayer, Nodes[NodeIndex].Layer);
			}
		}

		int32 MaxLayer = MaxScheduledLayer;
		for (const int32 NodeIndex : PhaseNodes)
		{
			if (!Nodes[NodeIndex].bScheduled)
			{
				Nodes[NodeIndex].Layer = MaxScheduledLayer + 1;
				MaxLayer = MaxScheduledLayer + 1;
			}
		}

		TMap<int32, int32> RowByLayer;
		for (const int32 NodeIndex : PhaseNodes)
		{
			FArcMassProcessorNode& Node = Nodes[NodeIndex];
			int32& Row = RowByLayer.FindOrAdd(Node.Layer);
			Node.GraphPos = ImVec2(ColumnOffset + Node.Layer * LayerSpacing, Row * RowSpacing);
			++Row;
		}

		ColumnOffset += (MaxLayer + 1) * LayerSpacing + PhaseColumnGap;
	}

	for (FArcMassProcessorNode& Node : Nodes)
	{
		if (const ImVec2* ManualPos = ManualPositions.Find(TObjectKey<UMassProcessor>(Node.Processor.Get())))
		{
			Node.GraphPos = *ManualPos;
		}
	}
}

void FArcMassProcessorDebugger::BindSignalHooks()
{
	UWorld* World = Arcx::GameplayDebugger::MassProcessor::GetDebugWorld();
	if (!World)
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSubsystem)
	{
		return;
	}

	BoundSignalSubsystem = SignalSubsystem;

	for (const FArcMassProcessorNode& Node : Nodes)
	{
		if (!Node.bSignalProcessor || !Node.Processor.IsValid())
		{
			continue;
		}

		const TObjectKey<UMassProcessor> Key(Node.Processor.Get());
		for (const FName SignalName : Node.RegisteredSignals)
		{
			const FDelegateHandle Handle = SignalSubsystem->GetSignalDelegateByName(SignalName).AddLambda(
				[this, Key](FName, TConstArrayView<FMassEntityHandle>)
				{
					FScopeLock Lock(&SignalTimesCS);
					LastSignalTimes.FindOrAdd(Key) = FPlatformTime::Seconds();
				});
			SignalHandles.Emplace(SignalName, Handle);
		}
	}
}

void FArcMassProcessorDebugger::UnbindSignalHooks()
{
	if (UMassSignalSubsystem* SignalSubsystem = BoundSignalSubsystem.Get())
	{
		for (const TPair<FName, FDelegateHandle>& Handle : SignalHandles)
		{
			SignalSubsystem->GetSignalDelegateByName(Handle.Key).Remove(Handle.Value);
		}
	}

	SignalHandles.Reset();
	BoundSignalSubsystem = nullptr;
}

bool FArcMassProcessorDebugger::IsGraphStale() const
{
	UMassSimulationSubsystem* Simulation = Arcx::GameplayDebugger::MassProcessor::GetSimulationSubsystem();
	if (!Simulation)
	{
		return !CachedPhaseRoots.IsEmpty() || !Nodes.IsEmpty();
	}

	const UE::Mass::FPhaseCoordinator& Coordinator = Simulation->GetPhaseManager().GetPhaseCoordinator();

	int32 RootIndex = 0;
	int32 ProcessorCount = 0;
	for (int32 PhaseIndex = 0; PhaseIndex < Coordinator.GetNumPhases(); ++PhaseIndex)
	{
		const UMassCompositeProcessor* PhaseProcessor = Coordinator.GetPhaseProcessor(Coordinator.GetPhaseHandle(PhaseIndex));
		if (!PhaseProcessor)
		{
			continue;
		}

		if (!CachedPhaseRoots.IsValidIndex(RootIndex) || CachedPhaseRoots[RootIndex].Get() != PhaseProcessor)
		{
			return true;
		}

		ProcessorCount += Arcx::GameplayDebugger::MassProcessor::CountHostedProcessors(*PhaseProcessor);
		++RootIndex;
	}

	return RootIndex != CachedPhaseRoots.Num() || ProcessorCount != CachedProcessorCount;
}

ImVec2 FArcMassProcessorDebugger::GraphToScreen(const ImVec2& GraphPos) const
{
	return ImVec2(
		CanvasCenter.x + (GraphPos.x - static_cast<float>(ViewOffset.X)) * Zoom,
		CanvasCenter.y + (GraphPos.y - static_cast<float>(ViewOffset.Y)) * Zoom);
}

FVector2D FArcMassProcessorDebugger::ScreenToGraph(const ImVec2& ScreenPos) const
{
	return FVector2D(
		ViewOffset.X + (ScreenPos.x - CanvasCenter.x) / Zoom,
		ViewOffset.Y + (ScreenPos.y - CanvasCenter.y) / Zoom);
}

bool FArcMassProcessorDebugger::IsNodeVisible(const int32 NodeIndex) const
{
	if (!Nodes.IsValidIndex(NodeIndex))
	{
		return false;
	}

	const FArcMassProcessorNode& Node = Nodes[NodeIndex];
	if (!bShowUnscheduled && !Node.bScheduled)
	{
		return false;
	}

	if (NameFilterBuf[0] != '\0')
	{
		const FString Filter = UTF8_TO_TCHAR(NameFilterBuf);
		if (!Node.DisplayName.Contains(Filter))
		{
			return false;
		}
	}

	return true;
}

int32 FArcMassProcessorDebugger::FindNodeAtScreenPos(const ImVec2& ScreenPos) const
{
	using namespace Arcx::GameplayDebugger::MassProcessor;

	const FVector2D GraphPos = ScreenToGraph(ScreenPos);
	for (int32 NodeIndex = Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
	{
		if (!IsNodeVisible(NodeIndex))
		{
			continue;
		}

		const ImVec2& NodePos = Nodes[NodeIndex].GraphPos;
		if (GraphPos.X >= NodePos.x && GraphPos.X <= NodePos.x + NodeWidth
			&& GraphPos.Y >= NodePos.y && GraphPos.Y <= NodePos.y + NodeHeight)
		{
			return NodeIndex;
		}
	}

	return INDEX_NONE;
}

void FArcMassProcessorDebugger::DrawToolbar()
{
	using namespace Arcx::GameplayDebugger::MassProcessor;

	if (ImGui::Button("Rebuild"))
	{
		bRebuildPending = true;
	}

	ImGui::SameLine();
	ImGui::Checkbox("Show unscheduled", &bShowUnscheduled);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.f);
	ImGui::InputText("Filter", NameFilterBuf, sizeof(NameFilterBuf));

	ImGui::SameLine();
	if (ImGui::Button("Reset view"))
	{
		ViewOffset = FVector2D::ZeroVector;
		Zoom = 1.f;
	}

	ImGui::SameLine();
	ImGui::Text("Nodes: %d  Edges: %d", Nodes.Num(), Edges.Num());

	for (const FString& Diagnostic : GraphDiagnostics)
	{
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Diagnostic));
	}

	DrawLegendEntry("##legend_active", "Active", ActiveColor);
	ImGui::SameLine();
	DrawLegendEntry("##legend_signal", "Signal processor", SignalColor);
	ImGui::SameLine();
	DrawLegendEntry("##legend_flash", "Signal raised", SignalFlashColor);
	ImGui::SameLine();
	DrawLegendEntry("##legend_inactive", "Inactive / unscheduled", InactiveColor);
	ImGui::SameLine();
	DrawLegendEntry("##legend_incoming", "Incoming edge", IncomingEdgeColor);
	ImGui::SameLine();
	DrawLegendEntry("##legend_outgoing", "Outgoing edge", OutgoingEdgeColor);
}

void FArcMassProcessorDebugger::HandleCanvasInput()
{
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bCanvasHovered = ImGui::IsItemHovered();

	if (bCanvasHovered && FMath::Abs(IO.MouseWheel) > 0.f)
	{
		const FVector2D GraphBeforeZoom = ScreenToGraph(IO.MousePos);

		const float ZoomFactor = 1.15f;
		if (IO.MouseWheel > 0.f)
		{
			Zoom = FMath::Min(Zoom * ZoomFactor, MaxZoom);
		}
		else
		{
			Zoom = FMath::Max(Zoom / ZoomFactor, MinZoom);
		}

		const FVector2D GraphAfterZoom = ScreenToGraph(IO.MousePos);
		ViewOffset.X -= (GraphAfterZoom.X - GraphBeforeZoom.X);
		ViewOffset.Y -= (GraphAfterZoom.Y - GraphBeforeZoom.Y);
	}

	HoveredNodeIndex = bCanvasHovered ? FindNodeAtScreenPos(IO.MousePos) : INDEX_NONE;

	if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
	{
		bIsPanning = true;
		PanMouseButton = ImGuiMouseButton_Middle;
		PanStartMouse = IO.MousePos;
		PanStartOffset = ViewOffset;
	}
	else if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (HoveredNodeIndex != INDEX_NONE)
		{
			SelectedNodeIndex = HoveredNodeIndex;
			SelectedProcessorKey = TObjectKey<UMassProcessor>(Nodes[HoveredNodeIndex].Processor.Get());
			DraggedNodeIndex = HoveredNodeIndex;
		}
		else
		{
			SelectedNodeIndex = INDEX_NONE;
			SelectedProcessorKey = TObjectKey<UMassProcessor>();
			bIsPanning = true;
			PanMouseButton = ImGuiMouseButton_Left;
			PanStartMouse = IO.MousePos;
			PanStartOffset = ViewOffset;
		}
	}

	if (DraggedNodeIndex != INDEX_NONE)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && Nodes.IsValidIndex(DraggedNodeIndex))
		{
			FArcMassProcessorNode& Node = Nodes[DraggedNodeIndex];
			Node.GraphPos = ImVec2(
				Node.GraphPos.x + IO.MouseDelta.x / Zoom,
				Node.GraphPos.y + IO.MouseDelta.y / Zoom);
			ManualPositions.FindOrAdd(TObjectKey<UMassProcessor>(Node.Processor.Get())) = Node.GraphPos;
		}
		else
		{
			DraggedNodeIndex = INDEX_NONE;
		}
	}

	if (bIsPanning)
	{
		if (ImGui::IsMouseDown(PanMouseButton))
		{
			const float DeltaX = IO.MousePos.x - PanStartMouse.x;
			const float DeltaY = IO.MousePos.y - PanStartMouse.y;
			ViewOffset.X = PanStartOffset.X - DeltaX / Zoom;
			ViewOffset.Y = PanStartOffset.Y - DeltaY / Zoom;
		}
		else
		{
			bIsPanning = false;
		}
	}
}

void FArcMassProcessorDebugger::DrawGraph()
{
	using namespace Arcx::GameplayDebugger::MassProcessor;

	CanvasPos = ImGui::GetCursorScreenPos();
	CanvasSize = ImGui::GetContentRegionAvail();
	if (CanvasSize.x < 1.f || CanvasSize.y < 1.f)
	{
		return;
	}

	CanvasCenter = ImVec2(CanvasPos.x + CanvasSize.x * 0.5f, CanvasPos.y + CanvasSize.y * 0.5f);
	const ImVec2 CanvasMax(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y);

	ImGui::InvisibleButton("##massproccanvas", CanvasSize,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	HandleCanvasInput();

	TMap<TObjectKey<UMassProcessor>, double> SignalTimes;
	{
		FScopeLock Lock(&SignalTimesCS);
		SignalTimes = LastSignalTimes;
	}
	const double Now = FPlatformTime::Seconds();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasPos, CanvasMax, true);
	DrawList->AddRectFilled(CanvasPos, CanvasMax, BackgroundColor);

	TMap<int32, ImVec2> PhaseAnchors;
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		if (!IsNodeVisible(NodeIndex))
		{
			continue;
		}

		const FArcMassProcessorNode& Node = Nodes[NodeIndex];
		ImVec2* Anchor = PhaseAnchors.Find(Node.PhaseIndex);
		if (!Anchor)
		{
			PhaseAnchors.Add(Node.PhaseIndex, Node.GraphPos);
			continue;
		}

		Anchor->x = FMath::Min(Anchor->x, Node.GraphPos.x);
		Anchor->y = FMath::Min(Anchor->y, Node.GraphPos.y);
	}

	for (const TPair<int32, ImVec2>& Anchor : PhaseAnchors)
	{
		const TCHAR* KnownPhase = PhaseLabel(Anchor.Key);
		const FString Label = KnownPhase
			? FString::Printf(TEXT("Phase %d: %s"), Anchor.Key, KnownPhase)
			: FString::Printf(TEXT("Phase %d"), Anchor.Key);
		DrawList->AddText(GraphToScreen(ImVec2(Anchor.Value.x, Anchor.Value.y - 40.f)),
			PhaseLabelColor, TCHAR_TO_UTF8(*Label));
	}

	for (const FArcMassProcessorEdge& Edge : Edges)
	{
		if (!IsNodeVisible(Edge.FromIndex) || !IsNodeVisible(Edge.ToIndex))
		{
			continue;
		}

		const ImVec2& FromPos = Nodes[Edge.FromIndex].GraphPos;
		const ImVec2& ToPos = Nodes[Edge.ToIndex].GraphPos;
		const ImVec2 Start = GraphToScreen(ImVec2(FromPos.x + NodeWidth, FromPos.y + NodeHeight * 0.5f));
		const ImVec2 End = GraphToScreen(ImVec2(ToPos.x, ToPos.y + NodeHeight * 0.5f));

		ImU32 Color = EdgeColor;
		float Thickness = 1.5f * Zoom;
		if (Edge.ToIndex == SelectedNodeIndex)
		{
			Color = IncomingEdgeColor;
			Thickness = 2.5f * Zoom;
		}
		else if (Edge.FromIndex == SelectedNodeIndex)
		{
			Color = OutgoingEdgeColor;
			Thickness = 2.5f * Zoom;
		}

		const float Tangent = TangentLength * Zoom;
		const ImVec2 StartTangent(Start.x + Tangent, Start.y);
		const ImVec2 EndTangent(End.x - Tangent, End.y);
		DrawList->AddBezierCubic(Start, StartTangent, EndTangent, End, Color, FMath::Max(1.f, Thickness));
		AddArrowHead(DrawList, End, ImVec2(End.x - EndTangent.x, End.y - EndTangent.y), Color,
			FMath::Max(4.f, ArrowSize * Zoom));
	}

	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		if (!IsNodeVisible(NodeIndex))
		{
			continue;
		}

		const FArcMassProcessorNode& Node = Nodes[NodeIndex];
		const UMassProcessor* Processor = Node.Processor.Get();
		const bool bActive = Processor && Processor->IsActive();

		ImU32 FillColor = InactiveColor;
		if (Node.bScheduled && bActive)
		{
			FillColor = Node.bSignalProcessor ? SignalColor : ActiveColor;
		}

		if (Node.bSignalProcessor)
		{
			if (const double* SignalTime = SignalTimes.Find(TObjectKey<UMassProcessor>(Processor)))
			{
				const double Elapsed = Now - *SignalTime;
				if (Elapsed >= 0.0 && Elapsed < SignalFlashSeconds)
				{
					FillColor = LerpColor(SignalFlashColor, FillColor, static_cast<float>(Elapsed) / SignalFlashSeconds);
				}
			}
		}

		const ImVec2 RectMin = GraphToScreen(Node.GraphPos);
		const ImVec2 RectMax = GraphToScreen(ImVec2(Node.GraphPos.x + NodeWidth, Node.GraphPos.y + NodeHeight));
		const float Rounding = 4.f * Zoom;

		DrawList->AddRectFilled(RectMin, RectMax, FillColor, Rounding);

		if (Node.bScheduled)
		{
			DrawList->AddRect(RectMin, RectMax, BorderColor, Rounding, 1.f);
		}
		else
		{
			AddDashedRect(DrawList, RectMin, RectMax, UnscheduledBorderColor, 1.f, FMath::Max(2.f, 6.f * Zoom));
		}

		if (NodeIndex == SelectedNodeIndex)
		{
			DrawList->AddRect(RectMin, RectMax, SelectedBorderColor, Rounding, 2.5f);
		}
		else if (NodeIndex == HoveredNodeIndex)
		{
			DrawList->AddRect(RectMin, RectMax, HoveredBorderColor, Rounding, 2.f);
		}

		if (Zoom >= MinTextZoom)
		{
			const ImVec4 TextClipRect(RectMin.x + 4.f, RectMin.y, RectMax.x - 4.f, RectMax.y);
			DrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * Zoom,
				ImVec2(RectMin.x + 6.f * Zoom, RectMin.y + 6.f * Zoom),
				(Node.bScheduled && bActive) ? NodeTextColor : DimTextColor,
				TCHAR_TO_UTF8(*Node.DisplayName), nullptr, 0.f, &TextClipRect);
		}
	}

	DrawList->PopClipRect();

	if (bCanvasHovered && Nodes.IsValidIndex(HoveredNodeIndex))
	{
		const FArcMassProcessorNode& Node = Nodes[HoveredNodeIndex];
		const UMassProcessor* Processor = Node.Processor.Get();
		const TCHAR* KnownPhase = PhaseLabel(Node.PhaseIndex);

		ImGui::BeginTooltip();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Node.DisplayName));
		ImGui::Separator();
		ImGui::Text("Class: %s", Processor ? TCHAR_TO_UTF8(*Processor->GetClass()->GetName()) : "<stale>");
		ImGui::Text("Phase: %d %s", Node.PhaseIndex, KnownPhase ? TCHAR_TO_UTF8(KnownPhase) : "");
		ImGui::Text("Group: %s", TCHAR_TO_UTF8(*Node.GroupName.ToString()));
		ImGui::Text("Active: %s", (Processor && Processor->IsActive()) ? "yes" : "no");
		ImGui::Text("Owned queries: %d", Processor ? Processor->GetOwnedQueriesNum() : 0);
		if (Node.bSignalProcessor)
		{
			ImGui::Text("Signals: %d", Node.RegisteredSignals.Num());
		}
		if (!Node.bScheduled)
		{
			ImGui::TextUnformatted("not scheduled - pruned by solver");
		}
		ImGui::EndTooltip();
	}
}

void FArcMassProcessorDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(1280, 800), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Mass Processors", &bShow))
	{
		ImGui::End();
		return;
	}

	RefreshIfNeeded();

	DrawToolbar();

	ImGui::Separator();

	const float GraphPanelWidth = ImGui::GetContentRegionAvail().x * 0.72f;
	if (ImGui::BeginChild("MassProcGraph", ImVec2(GraphPanelWidth, 0),
		ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		DrawGraph();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("MassProcDetail", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();
}

#else // WITH_MASSENTITY_DEBUG

void FArcMassProcessorDebugger::Initialize()
{
}

void FArcMassProcessorDebugger::Uninitialize()
{
}

void FArcMassProcessorDebugger::Draw()
{
	if (!ImGui::Begin("Mass Processors", &bShow))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Mass debugger disabled (WITH_MASSENTITY_DEBUG=0)");
	ImGui::End();
}

#endif // WITH_MASSENTITY_DEBUG
