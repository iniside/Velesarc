// Copyright Lukasz Baran. All Rights Reserved.

#include "GameplayDebuggerCategory_ArcMass.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "ArcMassEntityVisualization.h"
#include "ArcMassLifecycle.h"
#include "CanvasItem.h"
#include "GameFramework/PlayerController.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
namespace ArcMassDebugColors
{
	// Cell states
	static const FColor CellActive     = FColor(50, 200, 50, 180);   // green – actor vis
	static const FColor CellInactive   = FColor(200, 200, 50, 140);  // yellow – ISM vis
	static const FColor CellEmpty      = FColor(120, 120, 120, 80);  // grey – entities but no vis
	static const FColor CellFloorActive   = FColor(30, 140, 30, 40);
	static const FColor CellFloorInactive = FColor(140, 140, 30, 30);
	static const FColor CellFloorEmpty    = FColor(80, 80, 80, 20);

	// Entity shapes
	static const FColor EntityActor    = FColor(80, 180, 255);       // blue – actor representation
	static const FColor EntityISM      = FColor(255, 160, 40);       // orange – ISM representation
	static const FColor EntityNoVis    = FColor(180, 180, 180);      // grey – no vis

	// Lifecycle
	static const FColor PhaseStart     = FColor(200, 200, 200);
	static const FColor PhaseGrowing   = FColor(100, 220, 100);
	static const FColor PhaseGrown     = FColor(50, 180, 50);
	static const FColor PhaseDying     = FColor(220, 120, 50);
	static const FColor PhaseDead      = FColor(200, 40, 40);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
	const TCHAR* LifecyclePhaseName(EArcLifecyclePhase Phase)
	{
		switch (Phase)
		{
		case EArcLifecyclePhase::Start:   return TEXT("Start");
		case EArcLifecyclePhase::Growing: return TEXT("Growing");
		case EArcLifecyclePhase::Grown:   return TEXT("Grown");
		case EArcLifecyclePhase::Dying:   return TEXT("Dying");
		case EArcLifecyclePhase::Dead:    return TEXT("Dead");
		default:                          return TEXT("?");
		}
	}

	FColor GetLifecycleColor(EArcLifecyclePhase Phase)
	{
		switch (Phase)
		{
		case EArcLifecyclePhase::Start:   return ArcMassDebugColors::PhaseStart;
		case EArcLifecyclePhase::Growing: return ArcMassDebugColors::PhaseGrowing;
		case EArcLifecyclePhase::Grown:   return ArcMassDebugColors::PhaseGrown;
		case EArcLifecyclePhase::Dying:   return ArcMassDebugColors::PhaseDying;
		case EArcLifecyclePhase::Dead:    return ArcMassDebugColors::PhaseDead;
		default:                          return FColor::White;
		}
	}
}

// ---------------------------------------------------------------------------
// Construction / Factory
// ---------------------------------------------------------------------------

FGameplayDebuggerCategory_ArcMass::FGameplayDebuggerCategory_ArcMass()
{
	bShowOnlyWithDebugActor = false;

	BindKeyPress(EKeys::C.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_ArcMass::OnToggleCells, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::E.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_ArcMass::OnToggleEntities, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::L.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_ArcMass::OnToggleLifecycle, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::T.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_ArcMass::OnToggleDistances, EGameplayDebuggerInputMode::Replicated);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ArcMass::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_ArcMass());
}

// ---------------------------------------------------------------------------
// CollectData
// ---------------------------------------------------------------------------

void FGameplayDebuggerCategory_ArcMass::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		AddTextLine(TEXT("{red}MassEntitySubsystem not found"));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const UArcEntityVisualizationSubsystem* VisSub = World->GetSubsystem<UArcEntityVisualizationSubsystem>();

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	GetViewPoint(OwnerPC, ViewLocation, ViewDirection);

	// --- Header info ---
	AddTextLine(FString::Printf(TEXT("{green}ArcMass Entity Visualization")));
	AddTextLine(FString::Printf(TEXT("{green}Entities: {white}%d"), EntityManager.DebugGetEntityCount()));

	if (VisSub)
	{
		const FArcVisualizationGrid& Grid = VisSub->GetGrid();
		const int32 TotalCells = Grid.CellEntities.Num();
		const float CellSize = Grid.CellSize;
		const FIntVector PlayerCell = VisSub->GetLastPlayerCell();
		const int32 SwapRadiusCells = VisSub->GetSwapRadiusCells();

		AddTextLine(FString::Printf(TEXT("{green}Grid Cells: {white}%d  {green}Cell Size: {white}%.0f"), TotalCells, CellSize));
		AddTextLine(FString::Printf(TEXT("{green}Player Cell: {white}(%d, %d, %d)  {green}Swap Radius: {white}%.0f ({white}%d cells)"),
			PlayerCell.X, PlayerCell.Y, PlayerCell.Z, VisSub->GetSwapRadius(), SwapRadiusCells));

		// Count active / inactive / empty cells
		int32 ActiveCells = 0;
		int32 InactiveCells = 0;
		int32 EmptyCells = 0;

		for (const auto& CellPair : Grid.CellEntities)
		{
			const bool bIsActive = VisSub->IsActiveCellCoord(CellPair.Key);
			const int32 EntityCount = CellPair.Value.Num();

			if (EntityCount == 0)
			{
				EmptyCells++;
			}
			else if (bIsActive)
			{
				ActiveCells++;
			}
			else
			{
				InactiveCells++;
			}
		}

		AddTextLine(FString::Printf(TEXT("{green}Active: {white}%d  {yellow}Inactive: {white}%d  {grey}Empty: {white}%d"),
			ActiveCells, InactiveCells, EmptyCells));

		EntityLabels.Reset();

		if (bShowCells)
		{
			CollectCellData(*VisSub, EntityManager, ViewLocation);

			// --- Swap radius rings around player ---
			// The swap radius is the distance at which entities switch between
			// actor (close) and ISM (far) representation. Draw concentric
			// circles on the ground plane so the boundary is clearly visible.
			const float SwapRadius = VisSub->GetSwapRadius();
			const FVector PlayerWorldPos(
				PlayerCell.X * CellSize + CellSize * 0.5f,
				PlayerCell.Y * CellSize + CellSize * 0.5f,
				ViewLocation.Z);
			constexpr float RingZ = 5.f;
			const FVector RingCenter = FVector(PlayerWorldPos.X, PlayerWorldPos.Y, ViewLocation.Z + RingZ);

			// Inner ring: swap radius (actor ↔ ISM boundary)
			AddShape(FGameplayDebuggerShape::MakeCircle(RingCenter, FVector::UpVector, SwapRadius, 3.f,
				FColor(50, 200, 50, 200), TEXT("Actor Vis")));

			// Outer ring at 1.5× swap radius for context
			AddShape(FGameplayDebuggerShape::MakeCircle(RingCenter, FVector::UpVector, SwapRadius * 1.5f, 2.f,
				FColor(200, 200, 50, 140), TEXT("ISM Vis")));

			// Small center marker at player cell
			AddShape(FGameplayDebuggerShape::MakePoint(RingCenter, 12.f, FColor(255, 255, 255, 220)));
		}

		if (bShowEntities)
		{
			CollectEntityData(EntityManager, ViewLocation, ViewDirection, *VisSub);
		}
	}
	else
	{
		AddTextLine(TEXT("{yellow}ArcEntityVisualizationSubsystem not found"));
	}
}

// ---------------------------------------------------------------------------
// CollectCellData - draw cells with color-coded states
// ---------------------------------------------------------------------------

void FGameplayDebuggerCategory_ArcMass::CollectCellData(
	const UArcEntityVisualizationSubsystem& VisSub,
	FMassEntityManager& EntityManager,
	const FVector& ViewLocation)
{
	const FArcVisualizationGrid& Grid = VisSub.GetGrid();
	const float CellSize = Grid.CellSize;
	constexpr float MaxDrawDistance = 30000.f;
	const float MaxDrawDistanceSq = MaxDrawDistance * MaxDrawDistance;
	constexpr float CornerPostHeight = 250.f;
	constexpr float FloorZ = 2.f;           // slight offset to avoid z-fighting
	constexpr float EdgeThickness = 3.f;
	constexpr float CornerPostThickness = 4.f;

	for (const auto& CellPair : Grid.CellEntities)
	{
		const FIntVector& CellCoord = CellPair.Key;
		const TArray<FMassEntityHandle>& Entities = CellPair.Value;

		// Cell world-space corners at ground level
		const FVector C00(CellCoord.X * CellSize,              CellCoord.Y * CellSize,              CellCoord.Z * CellSize);
		const FVector C10(CellCoord.X * CellSize + CellSize,   CellCoord.Y * CellSize,              CellCoord.Z * CellSize);
		const FVector C11(CellCoord.X * CellSize + CellSize,   CellCoord.Y * CellSize + CellSize,   CellCoord.Z * CellSize);
		const FVector C01(CellCoord.X * CellSize,              CellCoord.Y * CellSize + CellSize,   CellCoord.Z * CellSize);
		const FVector CellCenter = (C00 + C11) * 0.5f;

		// Distance cull
		const float DistSq = FVector::DistSquared(ViewLocation, CellCenter);
		if (DistSq > MaxDrawDistanceSq)
		{
			continue;
		}

		const bool bIsActive = VisSub.IsActiveCellCoord(CellCoord);
		const int32 EntityCount = Entities.Num();

		// --- Determine cell state ---
		FColor EdgeColor;
		FColor FloorColor;
		FColor PostColor;
		FString StateLabel;

		if (EntityCount == 0)
		{
			EdgeColor  = ArcMassDebugColors::CellEmpty;
			FloorColor = ArcMassDebugColors::CellFloorEmpty;
			PostColor  = FColor(100, 100, 100, 120);
			StateLabel = TEXT("Empty");
		}
		else if (bIsActive)
		{
			EdgeColor  = ArcMassDebugColors::CellActive;
			FloorColor = ArcMassDebugColors::CellFloorActive;
			PostColor  = FColor(30, 180, 30, 200);
			StateLabel = TEXT("Active");
		}
		else
		{
			EdgeColor  = ArcMassDebugColors::CellInactive;
			FloorColor = ArcMassDebugColors::CellFloorInactive;
			PostColor  = FColor(180, 180, 30, 180);
			StateLabel = TEXT("Inactive");
		}

		const FVector ZFloor(0.f, 0.f, FloorZ);

		// --- Floor shading (two filled triangles forming a quad) ---
		// MakePolygon builds a flat index list {0,1,2,...} which DrawDebugMesh
		// consumes as triangle indices in groups of 3. A 4-vert polygon would
		// leave a dangling index and crash the line batcher, so we split the
		// quad into two explicit triangles.
		const FVector F00 = C00 + ZFloor;
		const FVector F10 = C10 + ZFloor;
		const FVector F11 = C11 + ZFloor;
		const FVector F01 = C01 + ZFloor;
		AddShape(FGameplayDebuggerShape::MakePolygon({F00, F10, F11}, FloorColor));
		AddShape(FGameplayDebuggerShape::MakePolygon({F00, F11, F01}, FloorColor));

		// --- Ground-level edges (thick segments forming cell boundary) ---
		const FVector ZEdge(0.f, 0.f, FloorZ + 1.f);
		AddShape(FGameplayDebuggerShape::MakeSegment(C00 + ZEdge, C10 + ZEdge, EdgeThickness, EdgeColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C10 + ZEdge, C11 + ZEdge, EdgeThickness, EdgeColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C11 + ZEdge, C01 + ZEdge, EdgeThickness, EdgeColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C01 + ZEdge, C00 + ZEdge, EdgeThickness, EdgeColor));

		// --- Corner posts (vertical segments at each corner for height) ---
		const FVector ZTop(0.f, 0.f, CornerPostHeight);
		AddShape(FGameplayDebuggerShape::MakeSegment(C00 + ZEdge, C00 + ZTop, CornerPostThickness, PostColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C10 + ZEdge, C10 + ZTop, CornerPostThickness, PostColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C11 + ZEdge, C11 + ZTop, CornerPostThickness, PostColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C01 + ZEdge, C01 + ZTop, CornerPostThickness, PostColor));

		// --- Top edges (wire frame at post height to close the cell) ---
		AddShape(FGameplayDebuggerShape::MakeSegment(C00 + ZTop, C10 + ZTop, EdgeThickness * 0.5f, EdgeColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C10 + ZTop, C11 + ZTop, EdgeThickness * 0.5f, EdgeColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C11 + ZTop, C01 + ZTop, EdgeThickness * 0.5f, EdgeColor));
		AddShape(FGameplayDebuggerShape::MakeSegment(C01 + ZTop, C00 + ZTop, EdgeThickness * 0.5f, EdgeColor));

		// --- Cell label at top center ---
		const FVector LabelPos = CellCenter + FVector(0.f, 0.f, CornerPostHeight + 50.f);

		FString CellInfo;
		if (bIsActive)
		{
			CellInfo = FString::Printf(TEXT("{green}[%d,%d] %s\n{white}%d entities (Actor)"),
				CellCoord.X, CellCoord.Y, *StateLabel, EntityCount);
		}
		else if (EntityCount > 0)
		{
			CellInfo = FString::Printf(TEXT("{yellow}[%d,%d] %s\n{white}%d entities (ISM)"),
				CellCoord.X, CellCoord.Y, *StateLabel, EntityCount);
		}
		else
		{
			CellInfo = FString::Printf(TEXT("{grey}[%d,%d] %s"),
				CellCoord.X, CellCoord.Y, *StateLabel);
		}

		if (bShowDistances)
		{
			const float Dist = FMath::Sqrt(DistSq);
			CellInfo += FString::Printf(TEXT("\n{lightgrey}%.0fm"), Dist / 100.f);
		}

		EntityLabels.Emplace(FEntityLabel{static_cast<float>(DistSq), LabelPos, CellInfo});
	}
}

// ---------------------------------------------------------------------------
// CollectEntityData - draw per-entity markers with representation & lifecycle
// ---------------------------------------------------------------------------

void FGameplayDebuggerCategory_ArcMass::CollectEntityData(
	FMassEntityManager& EntityManager,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	const UArcEntityVisualizationSubsystem& VisSub)
{
	constexpr float MaxEntityDrawDistance = 20000.f;
	const float MaxEntityDrawDistSq = MaxEntityDrawDistance * MaxEntityDrawDistance;
	constexpr float MinViewDirDot = 0.5f; // ~60 deg FOV

	FMassEntityQuery EntityQuery(EntityManager.AsShared());
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	FMassExecutionContext Context(EntityManager, 0.f);
	EntityQuery.ForEachEntityChunk(Context,
		[this, &ViewLocation, &ViewDirection, MaxEntityDrawDistSq, MinViewDirDot, &VisSub](FMassExecutionContext& ExecContext)
		{
			const int32 NumEntities = ExecContext.GetNumEntities();
			const auto TransformList = ExecContext.GetFragmentView<FTransformFragment>();
			const auto VisRepList = ExecContext.GetFragmentView<FArcVisRepresentationFragment>();
			const auto LifecycleList = ExecContext.GetFragmentView<FArcLifecycleFragment>();
			const bool bHasLifecycle = LifecycleList.Num() > 0;

			for (int32 i = 0; i < NumEntities; i++)
			{
				const FVector EntityLocation = TransformList[i].GetTransform().GetLocation();

				// Distance cull
				const float DistSq = FVector::DistSquared(ViewLocation, EntityLocation);
				if (DistSq > MaxEntityDrawDistSq)
				{
					continue;
				}

				// View direction cull
				const FVector DirToEntity = (EntityLocation - ViewLocation).GetSafeNormal();
				if (FVector::DotProduct(DirToEntity, ViewDirection) < MinViewDirDot)
				{
					continue;
				}

				const FArcVisRepresentationFragment& VisRep = VisRepList[i];
				const bool bIsActor = VisRep.bIsActorRepresentation;
				const FMassEntityHandle Entity = ExecContext.GetEntity(i);

				// --- Shape based on representation type ---
				constexpr float ShapeSize = 30.f;
				const FVector ShapePos = EntityLocation + FVector(0.f, 0.f, 20.f);

				if (bIsActor)
				{
					// Diamond / capsule for actor representation
					AddShape(FGameplayDebuggerShape::MakeCapsule(
						ShapePos + FVector(0, 0, ShapeSize),
						ShapeSize * 0.4f, ShapeSize,
						ArcMassDebugColors::EntityActor));
				}
				else if (VisRep.ISMInstanceId != INDEX_NONE)
				{
					// Box for ISM representation
					AddShape(FGameplayDebuggerShape::MakeBox(
						ShapePos + FVector(0, 0, ShapeSize * 0.5f),
						FVector(ShapeSize * 0.4f),
						ArcMassDebugColors::EntityISM));
				}
				else
				{
					// Small point for entities with no visual
					AddShape(FGameplayDebuggerShape::MakePoint(
						ShapePos,
						8.f,
						ArcMassDebugColors::EntityNoVis));
				}

				// --- Build label text ---
				const float Dist = FMath::Sqrt(DistSq);
				// Only show detailed labels for closer entities
				if (Dist > 10000.f)
				{
					continue;
				}

				FString Label;

				// Entity ID and representation
				Label += FString::Printf(TEXT("{orange}%s"), *Entity.DebugGetDescription());
				if (bIsActor)
				{
					Label += TEXT(" {cyan}[Actor]");
				}
				else if (VisRep.ISMInstanceId != INDEX_NONE)
				{
					Label += FString::Printf(TEXT(" {yellow}[ISM:%d]"), VisRep.ISMInstanceId);
				}
				else
				{
					Label += TEXT(" {grey}[NoVis]");
				}

				// Cell coords
				Label += FString::Printf(TEXT("\n{lightgrey}Cell(%d,%d,%d)"),
					VisRep.GridCoords.X, VisRep.GridCoords.Y, VisRep.GridCoords.Z);

				// Lifecycle info
				if (bShowLifecycle && bHasLifecycle)
				{
					const FArcLifecycleFragment& Lifecycle = LifecycleList[i];
					const TCHAR* PhaseName = LifecyclePhaseName(Lifecycle.CurrentPhase);
					const FColor PhaseColor = GetLifecycleColor(Lifecycle.CurrentPhase);

					Label += FString::Printf(TEXT("\n{white}Phase: "));

					// Color-code the phase name
					switch (Lifecycle.CurrentPhase)
					{
					case EArcLifecyclePhase::Start:   Label += FString::Printf(TEXT("{lightgrey}%s"), PhaseName); break;
					case EArcLifecyclePhase::Growing: Label += FString::Printf(TEXT("{green}%s"), PhaseName); break;
					case EArcLifecyclePhase::Grown:   Label += FString::Printf(TEXT("{green}%s"), PhaseName); break;
					case EArcLifecyclePhase::Dying:   Label += FString::Printf(TEXT("{yellow}%s"), PhaseName); break;
					case EArcLifecyclePhase::Dead:    Label += FString::Printf(TEXT("{red}%s"), PhaseName); break;
					default:                          Label += FString::Printf(TEXT("{white}%s"), PhaseName); break;
					}

					Label += FString::Printf(TEXT(" {lightgrey}%.1fs"), Lifecycle.PhaseTimeElapsed);
				}

				// Distance
				if (bShowDistances)
				{
					Label += FString::Printf(TEXT("\n{lightgrey}%.1fm"), Dist / 100.f);
				}

				const FVector LabelPos = EntityLocation + FVector(0.f, 0.f, 80.f);
				EntityLabels.Emplace(FEntityLabel{DistSq, LabelPos, Label});
			}
		});
}

// ---------------------------------------------------------------------------
// DrawData - HUD text and world labels
// ---------------------------------------------------------------------------

void FGameplayDebuggerCategory_ArcMass::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	// --- Hotkey legend ---
	CanvasContext.Printf(TEXT("\n[{yellow}%s{white}] %s Cells"), *GetInputHandlerDescription(0), bShowCells ? TEXT("Hide") : TEXT("Show"));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Entities"), *GetInputHandlerDescription(1), bShowEntities ? TEXT("Hide") : TEXT("Show"));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Lifecycle"), *GetInputHandlerDescription(2), bShowLifecycle ? TEXT("Hide") : TEXT("Show"));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Distances"), *GetInputHandlerDescription(3), bShowDistances ? TEXT("Hide") : TEXT("Show"));

	// --- Legend ---
	CanvasContext.Printf(TEXT(""));
	CanvasContext.Printf(TEXT("{green}[Active]{white} = Actor vis  {yellow}[Inactive]{white} = ISM vis  {grey}[Empty]{white} = No vis"));
	CanvasContext.Printf(TEXT("{cyan}Capsule{white} = Actor  {yellow}Box{white} = ISM  {grey}Point{white} = None"));

	// --- Render world-space labels ---
	// Sort by distance (closest first = most important)
	EntityLabels.Sort([](const FEntityLabel& A, const FEntityLabel& B) { return A.Score < B.Score; });

	constexpr int32 MaxLabels = 30;
	const int32 NumLabels = FMath::Min(EntityLabels.Num(), MaxLabels);

	struct FLayoutRect
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		int32 Index = INDEX_NONE;
		float Alpha = 1.f;
	};
	TArray<FLayoutRect> Layout;

	for (int32 Idx = 0; Idx < NumLabels; Idx++)
	{
		const FEntityLabel& Label = EntityLabels[Idx];
		if (Label.Text.IsEmpty() || !CanvasContext.IsLocationVisible(Label.Location))
		{
			continue;
		}

		float SizeX = 0.f, SizeY = 0.f;
		const FVector2D ScreenPos = CanvasContext.ProjectLocation(Label.Location);
		CanvasContext.MeasureString(Label.Text, SizeX, SizeY);

		FLayoutRect Rect;
		Rect.Min = ScreenPos + FVector2D(0.f, -SizeY * 0.5f);
		Rect.Max = Rect.Min + FVector2D(SizeX, SizeY);
		Rect.Index = Idx;
		Rect.Alpha = 0.f;

		// Overlap-based transparency
		const double Area = FMath::Max(0.0, Rect.Max.X - Rect.Min.X) * FMath::Max(0.0, Rect.Max.Y - Rect.Min.Y);
		const double InvArea = Area > KINDA_SMALL_NUMBER ? 1.0 / Area : 0.0;
		double Coverage = 0.0;

		for (const FLayoutRect& Other : Layout)
		{
			const double MinX = FMath::Max(Rect.Min.X, Other.Min.X);
			const double MinY = FMath::Max(Rect.Min.Y, Other.Min.Y);
			const double MaxX = FMath::Min(Rect.Max.X, Other.Max.X);
			const double MaxY = FMath::Min(Rect.Max.Y, Other.Max.Y);
			const double Intersection = FMath::Max(0.0, MaxX - MinX) * FMath::Max(0.0, MaxY - MinY);
			Coverage += (Intersection * InvArea) * Other.Alpha;
		}

		Rect.Alpha = static_cast<float>(FMath::Square(1.0 - FMath::Min(Coverage, 1.0)));
		if (Rect.Alpha > KINDA_SMALL_NUMBER)
		{
			Layout.Add(Rect);
		}
	}

	// Render back-to-front (most important on top)
	const FVector2D Padding(4.f, 3.f);
	for (int32 Idx = Layout.Num() - 1; Idx >= 0; Idx--)
	{
		const FLayoutRect& Rect = Layout[Idx];
		const FEntityLabel& Label = EntityLabels[Rect.Index];

		const FVector2D BgPos = Rect.Min - Padding;
		FCanvasTileItem Background(BgPos, Rect.Max - Rect.Min + Padding * 2.f, FLinearColor(0.f, 0.f, 0.f, 0.4f * Rect.Alpha));
		Background.BlendMode = SE_BLEND_TranslucentAlphaOnly;
		CanvasContext.DrawItem(Background, static_cast<float>(BgPos.X), static_cast<float>(BgPos.Y));
		CanvasContext.PrintAt(static_cast<float>(Rect.Min.X), static_cast<float>(Rect.Min.Y), FColor::White, Rect.Alpha, Label.Text);
	}

	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

#endif // WITH_GAMEPLAY_DEBUGGER
