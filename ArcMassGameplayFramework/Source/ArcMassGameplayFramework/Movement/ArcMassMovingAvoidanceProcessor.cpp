// Copyright Lukasz Baran. All Rights Reserved.

#include "Movement/ArcMassMovingAvoidanceProcessor.h"

#include "Avoidance/MassAvoidanceFragments.h"
#include "Avoidance/MassAvoidanceProcessors.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "Mass/DebugLogging.h"
#include "Mass/Debugger.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassDebuggerWorld.h"
#include "MassMovementFragments.h"
#include "MassNavigationDebug.h"
#include "MassNavigationFragments.h"
#include "MassNavigationSubsystem.h"
#include "MassNavigationTypes.h"
#include "MassNavigationUtils.h"
#include "MassSimulationLOD.h"
#include "Math/Vector2D.h"
#include "Movement/ArcMassMoverMovementFragments.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassMovingAvoidanceProcessor)

namespace ArcMassMovingAvoidance
{
	namespace Tweakables
	{
		bool bEnableEnvironmentAvoidance = true;
		bool bEnableSettingsForExtendingColliders = true;
		bool bUseAdjacentCorridors = true;
		bool bUseDrawDebugHelpers = false;
		bool bEnableDetailedDebug = false;
	}

	FAutoConsoleVariableRef Vars[] =
	{
		FAutoConsoleVariableRef(TEXT("arc.mass.avoidance.EnableEnvironmentAvoidance"), Tweakables::bEnableEnvironmentAvoidance, TEXT("Set to false to disable avoidance forces for environment (for debug purposes)."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("arc.mass.avoidance.EnableSettingsForExtendingColliders"), Tweakables::bEnableSettingsForExtendingColliders, TEXT("Set to false to disable using different settings for extending obstacles (for debug purposes)."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("arc.mass.avoidance.UseAdjacentCorridors"), Tweakables::bUseAdjacentCorridors, TEXT("Set to false to disable usage of adjacent lane width."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("arc.mass.avoidance.UseDrawDebugHelpers"), Tweakables::bUseDrawDebugHelpers, TEXT("Use debug draw helpers in addition to visual logs."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("arc.mass.avoidance.EnableDetailedDebug"), Tweakables::bEnableDetailedDebug, TEXT("Display additional avoidance debug information."), ECVF_Cheat)
	};

	constexpr int32 MaxExpectedAgentsPerCell = 6;
	constexpr int32 MinTouchingCellCount = 4;
	constexpr int32 MaxObstacleResults = MaxExpectedAgentsPerCell * MinTouchingCellCount;

	static void FindCloseObstacles(const FVector& Center, const FVector::FReal SearchRadius, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid,
									TArray<FMassNavigationObstacleItem, TFixedAllocator<MaxObstacleResults>>& OutCloseEntities, const int32 MaxResults)
	{
		OutCloseEntities.Reset();
		const FVector Extent(SearchRadius, SearchRadius, 0.);
		const FBox QueryBox = FBox(Center - Extent, Center + Extent);

		struct FSortingCell
		{
			int32 X;
			int32 Y;
			int32 Level;
			FVector::FReal SqDist;
		};
		TArray<FSortingCell, TInlineAllocator<64>> Cells;
		const FVector QueryCenter = QueryBox.GetCenter();

		for (int32 Level = 0; Level < AvoidanceObstacleGrid.NumLevels; Level++)
		{
			const FVector::FReal CellSize = AvoidanceObstacleGrid.GetCellSize(Level);
			const FNavigationObstacleHashGrid2D::FCellRect Rect = AvoidanceObstacleGrid.CalcQueryBounds(QueryBox, Level);

			for (int64 Y = Rect.MinY; Y <= Rect.MaxY; Y++)
			{
				for (int64 X = Rect.MinX; X <= Rect.MaxX; X++)
				{
					const FVector::FReal CenterX = (X + 0.5) * CellSize;
					const FVector::FReal CenterY = (Y + 0.5) * CellSize;
					const FVector::FReal DX = CenterX - QueryCenter.X;
					const FVector::FReal DY = CenterY - QueryCenter.Y;
					const FVector::FReal SqDist = DX * DX + DY * DY;
					FSortingCell SortCell;
					SortCell.X = X;
					SortCell.Y = Y;
					SortCell.Level = Level;
					SortCell.SqDist = SqDist;
					Cells.Add(SortCell);
				}
			}
		}

		Cells.Sort([](const FSortingCell& A, const FSortingCell& B) { return A.SqDist < B.SqDist; });

		for (const FSortingCell& SortedCell : Cells)
		{
			if (const FNavigationObstacleHashGrid2D::FCell* Cell = AvoidanceObstacleGrid.FindCell(SortedCell.X, SortedCell.Y, SortedCell.Level))
			{
				const TSparseArray<FNavigationObstacleHashGrid2D::FItem>&  Items = AvoidanceObstacleGrid.GetItems();
				for (int32 Idx = Cell->First; Idx != INDEX_NONE; Idx = Items[Idx].Next)
				{
					OutCloseEntities.Add(Items[Idx].ID);
					if (OutCloseEntities.Num() >= MaxResults)
					{
						return;
					}
				}
			}
		}
	}

	static FVector::FReal ComputeClosestPointOfApproach(const FVector2D Pos, const FVector2D Vel, const FVector::FReal Rad, const FVector2D SegStart, const FVector2D SegEnd, const FVector::FReal TimeHoriz)
	{
		const FVector2D SegDir = SegEnd - SegStart;
		const FVector2D RelPos = Pos - SegStart;
		const FVector::FReal VelSq = FVector2D::DotProduct(Vel, Vel);
		const FVector::FReal SegDirSq = FVector2D::DotProduct(SegDir, SegDir);
		const FVector::FReal DirVelSq = FVector2D::DotProduct(SegDir, Vel);
		const FVector::FReal DirRelPosSq = FVector2D::DotProduct(SegDir, RelPos);
		const FVector::FReal VelRelPosSq = FVector2D::DotProduct(Vel, RelPos);
		const FVector::FReal RelPosSq = FVector2D::DotProduct(RelPos, RelPos);
		const FVector::FReal A = SegDirSq * VelSq - DirVelSq * DirVelSq;
		const FVector::FReal B = SegDirSq * VelRelPosSq - DirRelPosSq * DirVelSq;
		const FVector::FReal C = SegDirSq * RelPosSq - DirRelPosSq * DirRelPosSq - FMath::Square(Rad) * SegDirSq;
		const FVector::FReal H = FMath::Max(0., B*B - A*C);
		const FVector::FReal T = FMath::Abs(A) > SMALL_NUMBER ? (-B - FMath::Sqrt(H)) / A : 0.;
		const FVector::FReal Y = DirRelPosSq + T * DirVelSq;

		if (Y > 0. && Y < SegDirSq)
		{
			return FMath::Clamp(T, 0., TimeHoriz);
		}
		else
		{
			const FVector2D CapRelPos = (Y <= 0.) ? RelPos : Pos - SegEnd;
			const FVector::FReal Cb = FVector2D::DotProduct(Vel, CapRelPos);
			const FVector::FReal Cc = FVector2D::DotProduct(CapRelPos, CapRelPos) - FMath::Square(Rad);
			const FVector::FReal Ch = FMath::Max(0., Cb * Cb - VelSq * Cc);
			const FVector::FReal T1 = VelSq > SMALL_NUMBER ? (-Cb - FMath::Sqrt(Ch)) / VelSq : 0.;
			return FMath::Clamp(T1, 0., TimeHoriz);
		}
	}

	static FVector::FReal ComputeClosestPointOfApproach(const FVector RelPos, const FVector RelVel, const FVector::FReal TotalRadius, const FVector::FReal TimeHoriz)
	{
		const FVector::FReal A = FVector::DotProduct(RelVel, RelVel);
		const FVector::FReal Inv2A = A > SMALL_NUMBER ? 1. / (2. * A) : 0.;
		const FVector::FReal B = FMath::Min(0., 2. * FVector::DotProduct(RelVel, RelPos));
		const FVector::FReal C = FVector::DotProduct(RelPos, RelPos) - FMath::Square(TotalRadius);
		const FVector::FReal Discr = FMath::Sqrt(FMath::Max(0., B * B - 4. * A * C));
		const FVector::FReal T = (-B - Discr) * Inv2A;
		return FMath::Clamp(T, 0., TimeHoriz);
	}

#if WITH_MASSGAMEPLAY_DEBUG
	using namespace UE::MassNavigation::Debug;

	static constexpr FColor Amber(255,179,0);
	static constexpr FColor Orange(251,140,0);
	static constexpr FColor OrangeRed(244,81,30);
	static constexpr FColor Cyan(0,172,193);
	static constexpr FColor Blue(2,155,229);
	static constexpr FColor Indigo(57,73,171);
	static constexpr FColor Yellow(253, 216, 53);
	static constexpr FColor Teal(0, 137, 123);
	static constexpr FColor Lime(172, 222, 51);

	static constexpr FColor CurrentAgentColor = Lime;
	static const	 FColor VelocityColor = FColor::Black;
	static constexpr FColor DesiredVelocityColor = Yellow;
	static constexpr FColor FinalSteeringForceColor = Teal;

	static constexpr FColor AgentsColor = Amber;
	static constexpr FColor AgentsNotMovingColor = Blue;
	static constexpr FColor AgentAvoidForceColor = Orange;
	static constexpr FColor AgentsNotMovingSeparationColor = Blue;
	static constexpr FColor AgentSeparationForceColor = OrangeRed;

	static constexpr FColor ObstacleColor = Cyan;
	static constexpr FColor ObstacleAvoidForceColor = Blue;
	static constexpr FColor ObstacleSeparationForceColor = Indigo;
	static const	FColor ObstacleContactNormalColor = FColor::Silver;

	static const FColor GhostColor = FColor::Silver;
	static const FColor GhostSteeringForceColor = FColor::Silver;

	static constexpr float AvoidThickness = 3.f;
	static constexpr float SeparationThickness = 3.f;
	static constexpr float SummedForcesThickness = 5.f;

	static constexpr float SteeringThickness = 8.f;
	static constexpr float SteeringArrowHeadSize = 12.f;

	static const FVector DebugAgentHeightOffset = FVector(0., 0., 175.);
	static const FVector DebugInputForceHeight = FVector(0., 0., 181.);
	static const FVector DebugAgentAvoidHeightOffset = FVector(0., 0., 182.);
	static const FVector DebugAgentSeparationHeightOffset = FVector(0., 0., 183.);
	static const FVector DebugOutputForcesHeight = FVector(0., 0., 184.);
	static const FVector DebugLowCylinderOffset = FVector(0., 0., 20.);

	static void DebugDrawVelocity(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color)
	{
		if (!Context.ShouldLogEntity())
		{
			return;
		}

		constexpr float Thickness = 3.f;
		constexpr FVector::FReal Pointyness = 1.8;
		const FVector Line = End - Start;
		const FVector UnitV = Line.GetSafeNormal();
		const FVector Perp = FVector::CrossProduct(UnitV, FVector::UpVector);
		const FVector Left = Perp - (Pointyness * UnitV);
		const FVector Right = -Perp - (Pointyness * UnitV);
		const FVector::FReal HeadSize = 0.08 * Line.Size();
		const UObject* LogOwner = Context.GetLogOwner();
		UE_VLOG_SEGMENT_THICK(LogOwner, Context.Category, Log, Start, End, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(LogOwner, Context.Category, Log, End, End + HeadSize * Left, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(LogOwner, Context.Category, Log, End, End + HeadSize * Right, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(LogOwner, Context.Category, Log, End + HeadSize * Left, End + HeadSize * Right, Color, (int16)Thickness, TEXT(""));

		if (UseDrawDebugHelper() && Context.World)
		{
			DrawDebugLine(Context.World, Start, End, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End, End + HeadSize * Left, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End, End + HeadSize * Right, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End + HeadSize * Left, End + HeadSize * Right, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
		}
	}

	static void DebugDrawForce(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color, const float Thickness, const FString& Text = FString())
	{
		DebugDrawArrow(Context, Start, End, Color, /*HeadSize*/4.f, Thickness, Text);
	}

	static void DebugDrawSummedForce(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color)
	{
		DebugDrawArrow(Context, Start + FVector(0.,0.,1.), End + FVector(0., 0., 1.), Color, /*HeadSize*/8.f, SummedForcesThickness);
	}

	static void DebugDrawHitPosition(const FDebugContext& Context, const FVector& Location, const FVector& Velocity,
		const FVector::FReal CPA, const FVector::FReal Radius, const FColor Color)
	{
		if (!Context.ShouldLogEntity())
		{
			return;
		}

		const FVector HitPosition = Location + (Velocity * CPA);
		const FVector LeftOffset = Radius * UE::MassNavigation::GetLeftDirection(Velocity.GetSafeNormal(), FVector::UpVector);
		ArcMassMovingAvoidance::DebugDrawLine(Context, Location + ArcMassMovingAvoidance::DebugAgentHeightOffset + LeftOffset,
			HitPosition + ArcMassMovingAvoidance::DebugAgentHeightOffset + LeftOffset, Color, /*Thickness*/1.5f);
		ArcMassMovingAvoidance::DebugDrawLine(Context, Location + ArcMassMovingAvoidance::DebugAgentHeightOffset - LeftOffset,
			HitPosition + ArcMassMovingAvoidance::DebugAgentHeightOffset - LeftOffset, Color, /*Thickness*/1.5f);
		ArcMassMovingAvoidance::DebugDrawCylinder(Context, HitPosition,
			HitPosition + ArcMassMovingAvoidance::DebugAgentHeightOffset, Radius, Color);
	}

#endif // WITH_MASSGAMEPLAY_DEBUG

}

UArcMassMovingAvoidanceProcessor::UArcMassMovingAvoidanceProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);
	ExecutionOrder.ExecuteBefore.Add(TEXT("MassStandingAvoidanceProcessor"));
}

void UArcMassMovingAvoidanceProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassMovementLimitsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassNavigationEdgesFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAvoidanceEntitiesToIgnoreFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassMediumLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassAvoidancePreventBackwardMovementTag>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FMassMovingAvoidanceParameters>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	EntityQuery.AddSubsystemRequirement<UMassNavigationSubsystem>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddIndirectFragmentRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddIndirectFragmentRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddIndirectFragmentRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddIndirectFragmentRequirement<FMassAvoidanceColliderFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddIndirectFragmentRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);

#if WITH_MASSGAMEPLAY_DEBUG
	EntityQuery.DebugEnableEntityOwnerLogging();
#endif
}

void UArcMassMovingAvoidanceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UArcMassMovingAvoidanceProcessor);

	UWorld* DebugDrawWorld = UE::Mass::Debug::GetDebugDrawWorld(Context);
	UWorld* World = Context.GetWorld();
	check(World);
	const double CurrentTime = World->GetTimeSeconds();
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [FallbackLogOwner = this, &EntityManager, DebugDrawWorld, CurrentTime, DeltaTime](FMassExecutionContext& Context)
	{
		const UMassNavigationSubsystem* NavigationSubsystem = Context.GetSubsystem<UMassNavigationSubsystem>();
		if (!NavigationSubsystem)
		{
			return;
		}
		const TArrayView<FArcMassForceFragment> ForceList = Context.GetMutableFragmentView<FArcMassForceFragment>();
		const TConstArrayView<FArcMassMovementLimitsFragment> LimitsList = Context.GetFragmentView<FArcMassMovementLimitsFragment>();
		const TArrayView<FMassDesiredMovementFragment> DesiredMovementList = Context.GetMutableFragmentView<FMassDesiredMovementFragment>();
		const bool bHasDesiredMovementFragments = !DesiredMovementList.IsEmpty();
		const TConstArrayView<FMassNavigationEdgesFragment> NavEdgesList = Context.GetFragmentView<FMassNavigationEdgesFragment>();
		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FMassAvoidanceEntitiesToIgnoreFragment> EntitiesToIgnoreList = Context.GetFragmentView<FMassAvoidanceEntitiesToIgnoreFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
		const FMassMovingAvoidanceParameters& MovingAvoidanceParams = Context.GetConstSharedFragment<FMassMovingAvoidanceParameters>();
		const bool bPreventBackwardMovement = Context.DoesArchetypeHaveTag<FMassAvoidancePreventBackwardMovementTag>();

		const FVector::FReal InvPredictiveAvoidanceTime = 1. / MovingAvoidanceParams.PredictiveAvoidanceTime;
		const FVector::FReal InvEnvironmentPredictiveAvoidanceTime = 1. / MovingAvoidanceParams.EnvironmentPredictiveAvoidanceTime;

		TArray<FMassNavigationObstacleItem, TFixedAllocator<ArcMassMovingAvoidance::MaxObstacleResults>> CloseEntities;

		struct FSortedObstacle
		{
			FVector LocationCached;
			FVector Forward;
			FMassNavigationObstacleItem ObstacleItem;
			FVector::FReal SqDist;
		};
		TArray<FSortedObstacle, TFixedAllocator<ArcMassMovingAvoidance::MaxObstacleResults>> ClosestObstacles;

		struct FEnvironmentContact
		{
			FVector Position = FVector::ZeroVector;
			FVector Normal = FVector::ZeroVector;
			FVector::FReal Distance = 0.;
			bool bAlongTheEdge = false;
			bool bBehindTheEdge = false;
		};
		TArray<FEnvironmentContact, TInlineAllocator<16>> Contacts;

		struct FCollider
		{
			FVector Location = FVector::ZeroVector;
			FVector Velocity = FVector::ZeroVector;
			float Radius = 0.f;
			bool bCanAvoid = true;
			bool bIsMoving = false;
		};
		TArray<FCollider, TInlineAllocator<16>> Colliders;

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIt];
			if (MoveTarget.GetCurrentAction() == EMassMovementAction::Animate || MoveTarget.GetCurrentAction() == EMassMovementAction::Stand)
			{
				continue;
			}

			FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			const FMassNavigationEdgesFragment& NavEdges = NavEdgesList[EntityIt];
			const FTransformFragment& Location = LocationList[EntityIt];
			const FMassVelocityFragment& Velocity = VelocityList[EntityIt];
			const FAgentRadiusFragment& RadiusFragment = RadiusList[EntityIt];

			TConstArrayView<FMassEntityHandle> EntitiesToIgnore =
				EntitiesToIgnoreList.IsEmpty() ?
				TConstArrayView<FMassEntityHandle>() :
					EntitiesToIgnoreList[EntityIt].EntitiesToIgnore;

			FArcMassForceFragment& Force = ForceList[EntityIt];
			const FArcMassMovementLimitsFragment& Limits = LimitsList[EntityIt];

			const FVector::FReal MaxSteerAccel = Limits.MaxAcceleration;
			const FVector::FReal MaximumSpeed = Limits.MaxSpeed;

			const FVector AgentLocation = Location.GetTransform().GetTranslation();
			const FVector AgentVelocity = FVector(Velocity.Value.X, Velocity.Value.Y, 0.);

			const FVector::FReal AgentRadius = RadiusFragment.Radius;

			FVector SteeringForce = Force.Value;

			FVector::FReal NearStartFade = 1.;
			FVector::FReal NearEndFade = 1.;

			if (MoveTarget.GetPreviousAction() != EMassMovementAction::Move)
			{
				NearStartFade = FMath::Min((CurrentTime - MoveTarget.GetCurrentActionStartTime()) / MovingAvoidanceParams.StartOfPathDuration, 1.);
			}

			if (MoveTarget.IntentAtGoal == EMassMovementAction::Stand)
			{
				const FVector::FReal ApproachDistance = FMath::Max<FVector::FReal>(1., MovingAvoidanceParams.EndOfPathDuration * MoveTarget.DesiredSpeed.Get() * Limits.SpeedScale);
				NearEndFade = FMath::Clamp(MoveTarget.DistanceToGoal / ApproachDistance, 0., 1.);
			}

			const FVector::FReal NearStartScaling = FMath::Lerp<FVector::FReal>(MovingAvoidanceParams.StartOfPathAvoidanceScale, 1., NearStartFade);
			const FVector::FReal NearEndScaling = FMath::Lerp<FVector::FReal>(MovingAvoidanceParams.EndOfPathAvoidanceScale, 1., NearEndFade);

#if WITH_MASSGAMEPLAY_DEBUG
			const ArcMassMovingAvoidance::FDebugContext BaseDebugContext(Context, FallbackLogOwner, LogAvoidance, DebugDrawWorld, Entity, EntityIt);
			const ArcMassMovingAvoidance::FDebugContext VelocitiesDebugContext(Context, FallbackLogOwner, LogAvoidanceVelocities, DebugDrawWorld, Entity, EntityIt);
			const ArcMassMovingAvoidance::FDebugContext ObstacleDebugContext(Context, FallbackLogOwner, LogAvoidanceObstacles, DebugDrawWorld, Entity, EntityIt);
			const ArcMassMovingAvoidance::FDebugContext AgentDebugContext(Context, FallbackLogOwner, LogAvoidanceAgents, DebugDrawWorld, Entity, EntityIt);

			FColor EntityColor = FColor::White;
			if (BaseDebugContext.ShouldLogEntity(&EntityColor))
			{
				const FString Text = FString::Printf(TEXT("%i"), Entity.Index);
				DebugDrawCylinder(BaseDebugContext, AgentLocation, AgentLocation + ArcMassMovingAvoidance::DebugAgentHeightOffset, (AgentRadius+1.),
					ArcMassMovingAvoidance::CurrentAgentColor, Text);

				DebugDrawSphere(BaseDebugContext, AgentLocation, 10.f, ArcMassMovingAvoidance::CurrentAgentColor);

				const FVector ZOffset(0,0,25);
				UE_VLOG_WIRECIRCLE_THICK(FallbackLogOwner, LogMassNavigation, Log, AgentLocation + ZOffset, FVector::UpVector, AgentRadius, EntityColor, /*Thickness*/4,
					TEXT("%s"), *Entity.DebugGetDescription(), TEXT("%s"), *Entity.DebugGetDescription());

				ArcMassMovingAvoidance::DebugDrawVelocity(VelocitiesDebugContext, AgentLocation + ArcMassMovingAvoidance::DebugInputForceHeight,
				                                     AgentLocation + ArcMassMovingAvoidance::DebugInputForceHeight + AgentVelocity, ArcMassMovingAvoidance::VelocityColor);

				DebugDrawArrow(BaseDebugContext, AgentLocation + ArcMassMovingAvoidance::DebugInputForceHeight,
					AgentLocation + ArcMassMovingAvoidance:: DebugInputForceHeight + SteeringForce,
					ArcMassMovingAvoidance::CurrentAgentColor, ArcMassMovingAvoidance::SteeringArrowHeadSize, ArcMassMovingAvoidance::SteeringThickness);

				DebugDrawSphere(BaseDebugContext, AgentLocation, /*Radius*/2.f, ArcMassMovingAvoidance::CurrentAgentColor);
			}
#endif // WITH_MASSGAMEPLAY_DEBUG

			FVector OldSteeringForce = FVector::ZeroVector;

			if (!MoveTarget.bOffBoundaries && ArcMassMovingAvoidance::Tweakables::bEnableEnvironmentAvoidance)
			{
				const FVector::FReal EnvironmentSeparationAgentRadius = (RadiusFragment.Radius * MovingAvoidanceParams.SeparationRadiusScale) - (NavEdges.bExtrudedEdges ? AgentRadius : 0);
				const FVector::FReal EnvironmentPredictiveAvoidanceAgentRadius = (RadiusFragment.Radius * MovingAvoidanceParams.PredictiveAvoidanceRadiusScale) - (NavEdges.bExtrudedEdges ? AgentRadius : 0);

				const FVector DesiredAcceleration = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel);
				const FVector DesiredVelocity = UE::MassNavigation::ClampVector(AgentVelocity + DesiredAcceleration * DeltaTime, MaximumSpeed);

#if WITH_MASSGAMEPLAY_DEBUG
				ArcMassMovingAvoidance::DebugDrawVelocity(VelocitiesDebugContext, AgentLocation + ArcMassMovingAvoidance::DebugInputForceHeight,
					AgentLocation + ArcMassMovingAvoidance::DebugInputForceHeight + DesiredVelocity, ArcMassMovingAvoidance::DesiredVelocityColor);
#endif // WITH_MASSGAMEPLAY_DEBUG

				OldSteeringForce = SteeringForce;
				Contacts.Reset();

				const FVector::FReal AgentRadiusForEnvironment = NavEdges.bExtrudedEdges ? 0 : AgentRadius;

				int32 EdgeIndex = 0;

				for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
				{
					const FVector EdgeDiff = Edge.End - Edge.Start;
					FVector EdgeDir = FVector::ZeroVector;
					FVector::FReal EdgeLength = 0.;
					EdgeDiff.ToDirectionAndLength(EdgeDir, EdgeLength);

					const FVector EdgeStartToAgent = AgentLocation - Edge.Start;
					const FVector::FReal DistAlongEdge = FVector::DotProduct(EdgeDir, EdgeStartToAgent);
					const FVector::FReal DistAwayFromEdge = FVector::DotProduct(Edge.LeftDir, EdgeStartToAgent);

					FVector ConPos = FVector::ZeroVector;
					bool bBehindTheEdge = false;
					bool bAlongTheEdge = false;

					if (DistAwayFromEdge < 0)
					{
						bBehindTheEdge = true;

						if (DistAlongEdge < 0)
						{
							ConPos = Edge.Start;
						}
						else if (DistAlongEdge > EdgeLength)
						{
							ConPos = Edge.End;
						}
						else
						{
							bAlongTheEdge = true;
							ConPos = Edge.Start + EdgeDir * DistAlongEdge;
						}
					}
					else
					{
						if (DistAlongEdge < 0)
						{
							ConPos = Edge.Start;
						}
						else if (DistAlongEdge > EdgeLength)
						{
							ConPos = Edge.End;
						}
						else
						{
							bAlongTheEdge = true;
							ConPos = Edge.Start + EdgeDir * DistAlongEdge;
						}
					}

					FEnvironmentContact Contact;
					Contact.Position = ConPos;
					Contact.Normal = Edge.LeftDir;
					Contact.Distance = DistAwayFromEdge;
					Contact.bAlongTheEdge = bAlongTheEdge;
					Contact.bBehindTheEdge = bBehindTheEdge;
					Contacts.Add(Contact);

#if WITH_MASSGAMEPLAY_DEBUG
					if (ArcMassMovingAvoidance::Tweakables::bEnableDetailedDebug && ObstacleDebugContext.ShouldLogEntity())
					{
						if (bAlongTheEdge)
						{
							FVector ZOffset = FVector(0., 0., 7.);
							DebugDrawLine(ObstacleDebugContext, ZOffset + Edge.Start, ZOffset + Edge.End, FColor::Yellow, /*Thickness=*/3.f,
								/*bPersistent*/false, *LexToString(EdgeIndex));

							const FVector SeparationOffset = MovingAvoidanceParams.EnvironmentSeparationDistance * Edge.LeftDir;
							DebugDrawLine(ObstacleDebugContext, ZOffset + Edge.Start + SeparationOffset, ZOffset + Edge.End + SeparationOffset, FColor::Yellow, /*Thickness=*/1.f);
						}

						if (bBehindTheEdge)
						{
							FVector Offset = FVector(0., 0., 9.);
							DebugDrawLine(ObstacleDebugContext, Offset + Edge.Start, Offset + Edge.End, FColor::Silver, /*Thickness=*/2.f);
						}
					}
#endif // WITH_MASSGAMEPLAY_DEBUG

					if (!bBehindTheEdge)
					{

#if WITH_MASSGAMEPLAY_DEBUG
						if (ArcMassMovingAvoidance::Tweakables::bEnableDetailedDebug && ObstacleDebugContext.ShouldLogEntity())
						{
							FVector ZOffset = FVector(0., 0., 7.);
							const FVector AvoidanceOffset = MovingAvoidanceParams.EnvironmentPredictiveAvoidanceDistance * Edge.LeftDir;
							DebugDrawLine(ObstacleDebugContext, ZOffset + Edge.Start + AvoidanceOffset, ZOffset + Edge.End + AvoidanceOffset,
								ArcMassMovingAvoidance::ObstacleAvoidForceColor, /*Thickness=*/1.f);
						}
#endif // WITH_MASSGAMEPLAY_DEBUG

						const FVector::FReal CPA = ArcMassMovingAvoidance::ComputeClosestPointOfApproach(
							FVector2D(AgentLocation), FVector2D(DesiredVelocity), AgentRadiusForEnvironment,
							FVector2D(Edge.Start), FVector2D(Edge.End), MovingAvoidanceParams.EnvironmentPredictiveAvoidanceTime);
						const FVector HitAgentPos = AgentLocation + DesiredVelocity * CPA;
						const FVector::FReal EdgeT = UE::MassNavigation::ProjectPtSeg(FVector2D(HitAgentPos), FVector2D(Edge.Start), FVector2D(Edge.End));
						const FVector HitObPos = FMath::Lerp(Edge.Start, Edge.End, EdgeT);

						FVector AvoidRelPos = HitAgentPos - HitObPos;
						AvoidRelPos.Z = 0.;
						const FVector::FReal AvoidDist = AvoidRelPos.Size();
						const FVector AvoidNormal = AvoidDist > UE_KINDA_SMALL_NUMBER ? (AvoidRelPos / AvoidDist) : Edge.LeftDir;

						const FVector::FReal AvoidPen = (EnvironmentPredictiveAvoidanceAgentRadius + MovingAvoidanceParams.EnvironmentPredictiveAvoidanceDistance) - AvoidDist;
						FVector::FReal AvoidMag = 1.;

						if (MovingAvoidanceParams.EnvironmentPredictiveAvoidanceDistance != 0.)
						{
							AvoidMag = FMath::Square(FMath::Clamp(AvoidPen / MovingAvoidanceParams.EnvironmentPredictiveAvoidanceDistance, 0., 1.));
						}

						const FVector::FReal AvoidMagDist = 1. + FMath::Square(1. - CPA * InvEnvironmentPredictiveAvoidanceTime);

						const FVector AvoidForce = AvoidNormal * AvoidMag * AvoidMagDist * MovingAvoidanceParams.EnvironmentPredictiveAvoidanceStiffness * NearEndScaling;

						SteeringForce += AvoidForce;

#if WITH_MASSGAMEPLAY_DEBUG
						if (!AvoidForce.IsNearlyZero())
						{
							if (ArcMassMovingAvoidance::Tweakables::bEnableDetailedDebug)
							{
								DebugDrawArrow(ObstacleDebugContext, ConPos, ConPos + (Contact.Distance * Contact.Normal), ArcMassMovingAvoidance::ObstacleContactNormalColor, /*HeadSize=*/ 5.f);
								DebugDrawSphere(ObstacleDebugContext, ConPos, 2.5f, ArcMassMovingAvoidance::ObstacleContactNormalColor);
							}

							DebugDrawSphere(ObstacleDebugContext, HitAgentPos, 1.f, ArcMassMovingAvoidance::ObstacleAvoidForceColor);
							DebugDrawCircle(ObstacleDebugContext, HitAgentPos, AgentRadius, ArcMassMovingAvoidance::ObstacleAvoidForceColor);
							DebugDrawLine(ObstacleDebugContext, AgentLocation, HitAgentPos, ArcMassMovingAvoidance::ObstacleAvoidForceColor);

							ArcMassMovingAvoidance::DebugDrawForce(ObstacleDebugContext, HitObPos, HitObPos + AvoidForce,
								ArcMassMovingAvoidance::ObstacleAvoidForceColor, ArcMassMovingAvoidance::AvoidThickness);
						}
#endif // WITH_MASSGAMEPLAY_DEBUG
					}

					EdgeIndex++;

				}

#if WITH_MASSGAMEPLAY_DEBUG
				const FVector EnvironmentAvoidSteeringForce = SteeringForce - OldSteeringForce;
				ArcMassMovingAvoidance::DebugDrawSummedForce(ObstacleDebugContext,
					AgentLocation + ArcMassMovingAvoidance::DebugAgentAvoidHeightOffset,
					AgentLocation + ArcMassMovingAvoidance::DebugAgentAvoidHeightOffset + EnvironmentAvoidSteeringForce,
					ArcMassMovingAvoidance::ObstacleAvoidForceColor);

				if (ArcMassMovingAvoidance::Tweakables::bEnableDetailedDebug)
				{
					for (const FEnvironmentContact& Contact : Contacts)
					{
						DebugDrawSphere(ObstacleDebugContext, Contact.Position + FVector(0.f, 0.f, Contact.bBehindTheEdge ? 10.f : 0.f),
							5.f, Contact.bBehindTheEdge ? FColor::Black: FColor::Cyan);
					}
				}
#endif // WITH_MASSGAMEPLAY_DEBUG

				const FVector SteeringForceBeforeSeparation = SteeringForce;

				const FVector::FReal ValidBehindDistance = MovingAvoidanceParams.EnvironmentSeparationBehindEdgeDistanceScale * AgentRadius;

				for (const FEnvironmentContact& Contact : Contacts)
				{
					if (Contact.bAlongTheEdge && Contact.Distance > -ValidBehindDistance)
					{
						const FVector::FReal SeparationPenalty = (EnvironmentSeparationAgentRadius + MovingAvoidanceParams.EnvironmentSeparationDistance) - Contact.Distance;

						FVector::FReal SeparationMag;

						const FVector::FReal BehindDistanceThreshold = 0.25*AgentRadius;
						if (Contact.Distance < -BehindDistanceThreshold)
						{
							SeparationMag = 10.;
						}
						else if (MovingAvoidanceParams.EnvironmentSeparationDistance != 0.)
						{
							SeparationMag = UE::MassNavigation::Smooth(FMath::Clamp(SeparationPenalty / MovingAvoidanceParams.EnvironmentSeparationDistance, 0., 1.));
						}
						else
						{
							SeparationMag = 1.;
						}
						const FVector SeparationForce = Contact.Normal * MovingAvoidanceParams.EnvironmentSeparationStiffness * SeparationMag;

						SteeringForce += SeparationForce;

	#if WITH_MASSGAMEPLAY_DEBUG
						UE_VLOG(ObstacleDebugContext.GetLogOwner(), ObstacleDebugContext.Category, Log, TEXT("EnvironmentSeparationAgentRadius: %f, EnvironmentSeparationDistanc: %f, Distance: %f, SeparationPenalty: %f, SeparationMag: %f, SeparationForce: %s"),
							EnvironmentSeparationAgentRadius,
							MovingAvoidanceParams.EnvironmentSeparationDistance,
							Contact.Distance,
							SeparationPenalty,
							SeparationMag,
							*SeparationForce.ToCompactString()
							);

						if (!SeparationForce.IsNearlyZero())
						{
							const FVector ZOffset = FVector(0., 0., 7.);
							ArcMassMovingAvoidance::DebugDrawForce(ObstacleDebugContext, Contact.Position + ZOffset,
								Contact.Position + SeparationForce + ZOffset,
								ArcMassMovingAvoidance::ObstacleSeparationForceColor, ArcMassMovingAvoidance::SeparationThickness);
						}
	#endif // WITH_MASSGAMEPLAY_DEBUG
					}
				}

#if WITH_MASSGAMEPLAY_DEBUG
				const FVector TotalSeparationForce = SteeringForce - SteeringForceBeforeSeparation;
				ArcMassMovingAvoidance::DebugDrawSummedForce(ObstacleDebugContext,
					AgentLocation + ArcMassMovingAvoidance::DebugAgentSeparationHeightOffset,
					AgentLocation + ArcMassMovingAvoidance::DebugAgentSeparationHeightOffset + TotalSeparationForce,
					ArcMassMovingAvoidance::ObstacleSeparationForceColor);

				if (ObstacleDebugContext.ShouldLogEntity())
				{
					for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
					{
						FVector Offset = FVector(0., 0., 5.);
						DebugDrawLine(ObstacleDebugContext, Offset + Edge.Start, Offset + Edge.End, ArcMassMovingAvoidance::ObstacleColor, /*Thickness=*/2.f);

						const FVector Middle = Offset + 0.5f * (Edge.Start + Edge.End);
						DebugDrawArrow(ObstacleDebugContext, Middle, Middle + 10. * Edge.LeftDir, ArcMassMovingAvoidance::ObstacleColor, /*HeadSize=*/2.f);
					}
				}
#endif // WITH_MASSGAMEPLAY_DEBUG
			}

			const FVector::FReal SeparationAgentRadius = RadiusFragment.Radius * MovingAvoidanceParams.SeparationRadiusScale;
			const FVector::FReal PredictiveAvoidanceAgentRadius = RadiusFragment.Radius * MovingAvoidanceParams.PredictiveAvoidanceRadiusScale;

			const FVector DesAcc = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel);
			const FVector DesVel = UE::MassNavigation::ClampVector(AgentVelocity + DesAcc * DeltaTime, MaximumSpeed);

			const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid = NavigationSubsystem->GetObstacleGrid();

			ArcMassMovingAvoidance::FindCloseObstacles(AgentLocation, MovingAvoidanceParams.ObstacleDetectionDistance,
				AvoidanceObstacleGrid, CloseEntities, ArcMassMovingAvoidance::MaxObstacleResults);

			const FVector::FReal DistanceCutOffSqr = FMath::Square(MovingAvoidanceParams.ObstacleDetectionDistance);
			ClosestObstacles.Reset();
			for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : CloseEntities)
			{
				if (OtherEntity.Entity == Entity)
				{
					continue;
				}

				if (!EntityManager.IsEntityValid(OtherEntity.Entity))
				{
					UE_LOGF(LogAvoidanceObstacles, VeryVerbose, "Close entity is invalid, skipped.");
					continue;
				}

				const FTransform& Transform = EntityManager.GetFragmentDataChecked<FTransformFragment>(OtherEntity.Entity).GetTransform();
				const FVector OtherLocation = Transform.GetLocation();

				const FVector::FReal SqDist = FVector::DistSquared(AgentLocation, OtherLocation);
				if (SqDist > DistanceCutOffSqr)
				{
					continue;
				}

				if (UNLIKELY(!EntitiesToIgnore.IsEmpty()) && EntitiesToIgnore.Contains(OtherEntity.Entity))
				{
					continue;
				}

				FSortedObstacle Obstacle;
				Obstacle.LocationCached = OtherLocation;
				Obstacle.Forward = Transform.GetRotation().GetForwardVector();
				Obstacle.ObstacleItem = OtherEntity;
				Obstacle.SqDist = SqDist;
				ClosestObstacles.Add(Obstacle);
			}
			ClosestObstacles.Sort([](const FSortedObstacle& A, const FSortedObstacle& B) { return A.SqDist < B.SqDist; });

			OldSteeringForce = SteeringForce;
			FVector TotalAgentSeparationForce = FVector::ZeroVector;

			Colliders.Reset();
			for (int32 Index = 0; Index < ClosestObstacles.Num(); Index++)
			{
				constexpr int32 MaxColliders = 6;
				if (Colliders.Num() >= MaxColliders)
				{
					break;
				}

				FSortedObstacle& Obstacle = ClosestObstacles[Index];
				FMassEntityView OtherEntityView(EntityManager, Obstacle.ObstacleItem.Entity);

				const FMassVelocityFragment* OtherVelocityFragment = OtherEntityView.GetFragmentDataPtr<FMassVelocityFragment>();
				const FVector OtherVelocity = OtherVelocityFragment != nullptr ? OtherVelocityFragment->Value : FVector::ZeroVector;

				const FMassMoveTargetFragment* OtherMoveTarget = OtherEntityView.GetFragmentDataPtr<FMassMoveTargetFragment>();
				const bool bCanAvoid = OtherMoveTarget != nullptr;
				const bool bOtherIsMoving = OtherMoveTarget ? OtherMoveTarget->GetCurrentAction() == EMassMovementAction::Move : true;

				if (EnumHasAnyFlags(Obstacle.ObstacleItem.ItemFlags, EMassNavigationObstacleFlags::HasColliderData))
				{
					if (const FMassAvoidanceColliderFragment* ColliderFragment = OtherEntityView.GetFragmentDataPtr<FMassAvoidanceColliderFragment>())
					{
						if (ColliderFragment->Type == EMassColliderType::Circle)
						{
							const FMassCircleCollider Circle = ColliderFragment->GetCircleCollider();

							FCollider& Collider = Colliders.Add_GetRef(FCollider{});
							Collider.Velocity = OtherVelocity;
							Collider.bCanAvoid = bCanAvoid;
							Collider.bIsMoving = bOtherIsMoving;
							Collider.Radius = Circle.Radius;
							Collider.Location = Obstacle.LocationCached;
						}
						else if (ColliderFragment->Type == EMassColliderType::Pill)
						{
							const FMassPillCollider Pill = ColliderFragment->GetPillCollider();

							FCollider& Collider = Colliders.Add_GetRef(FCollider{});
							Collider.Velocity = OtherVelocity;
							Collider.bCanAvoid = bCanAvoid;
							Collider.bIsMoving = bOtherIsMoving;
							Collider.Radius = Pill.Radius;
							Collider.Location = Obstacle.LocationCached + (Pill.HalfLength * Obstacle.Forward);

							if (Colliders.Num() < MaxColliders)
							{
								FCollider& Collider2 = Colliders.Add_GetRef(FCollider{});
								Collider2.Velocity = OtherVelocity;
								Collider2.bCanAvoid = bCanAvoid;
								Collider2.bIsMoving = bOtherIsMoving;
								Collider2.Radius = Pill.Radius;
								Collider2.Location = Obstacle.LocationCached + (-Pill.HalfLength * Obstacle.Forward);
							}
						}
					}
				}
				else
				{
					FCollider& Collider = Colliders.Add_GetRef(FCollider{});
					Collider.Location = Obstacle.LocationCached;
					Collider.Velocity = OtherVelocity;
					Collider.Radius = OtherEntityView.GetFragmentData<FAgentRadiusFragment>().Radius;
					Collider.bCanAvoid = bCanAvoid;
					Collider.bIsMoving = bOtherIsMoving;
				}
			}

			for (const FCollider& Collider : Colliders)
			{
				bool bHasForcedNormal = false;
				FVector ForcedNormal = FVector::ZeroVector;

				if (Collider.bCanAvoid == false)
				{
					const FVector::FReal MinClearance = 2. * AgentRadius * MovingAvoidanceParams.StaticObstacleClearanceScale;

					FVector::FReal MaxDist = -1.;
					FVector ClosestPoint = FVector::ZeroVector;
					for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
					{
						const FVector Point = FMath::ClosestPointOnSegment(Collider.Location, Edge.Start, Edge.End);
						const FVector Offset = Collider.Location - Point;
						if (FVector::DotProduct(Offset, Edge.LeftDir) < 0.)
						{
							continue;
						}

						const FVector::FReal OffsetLength = Offset.Length();
						const bool bTooNarrow = (OffsetLength - Collider.Radius) < MinClearance;
						if (bTooNarrow)
						{
							if (OffsetLength > MaxDist)
							{
								MaxDist = OffsetLength;
								ClosestPoint = Point;
							}
						}
					}

					if (MaxDist != -1.)
					{
						ForcedNormal = (Collider.Location - ClosestPoint).GetSafeNormal();
						bHasForcedNormal = true;
					}
				}

				FVector RelPos = AgentLocation - Collider.Location;
				RelPos.Z = 0.;
				const FVector RelVel = DesVel - Collider.Velocity;
				const FVector::FReal ConDist = RelPos.Size();
				const FVector ConNorm = ConDist > 0. ? RelPos / ConDist : FVector::ForwardVector;

				FVector SeparationNormal = ConNorm;
				if (bHasForcedNormal)
				{
					const FVector RelVelNorm = RelVel.GetSafeNormal();
					const FVector::FReal Blend = FMath::Max(0., -FVector::DotProduct(ConNorm, RelVelNorm));
					SeparationNormal = FMath::Lerp(ConNorm, ForcedNormal, Blend).GetSafeNormal();
				}

				const FVector::FReal StandingScaling = Collider.bIsMoving ? 1. : MovingAvoidanceParams.StandingObstacleAvoidanceScale;

				const FVector::FReal PenSep = (SeparationAgentRadius + Collider.Radius + MovingAvoidanceParams.ObstacleSeparationDistance) - ConDist;
				const FVector::FReal SeparationMag = FMath::Square(FMath::Clamp(PenSep / MovingAvoidanceParams.ObstacleSeparationDistance, 0., 1.));
				const FVector SepForce = SeparationNormal * MovingAvoidanceParams.ObstacleSeparationStiffness;
				const FVector SeparationForce = SepForce * SeparationMag * StandingScaling;

				SteeringForce += SeparationForce;
				TotalAgentSeparationForce += SeparationForce;

				const FVector::FReal CPA = ArcMassMovingAvoidance::ComputeClosestPointOfApproach(RelPos, RelVel, PredictiveAvoidanceAgentRadius + Collider.Radius, MovingAvoidanceParams.PredictiveAvoidanceTime);

				const FVector AvoidRelPos = RelPos + RelVel * CPA;
				const FVector::FReal AvoidDist = AvoidRelPos.Size();
				const FVector AvoidConNormal = AvoidDist > UE_KINDA_SMALL_NUMBER ? (AvoidRelPos / AvoidDist) : FVector::ForwardVector;

				FVector AvoidNormal = AvoidConNormal;
				if (bHasForcedNormal)
				{
					const FVector RelVelNorm = RelVel.GetSafeNormal();
					const FVector::FReal Blend = FMath::Max(0., -FVector::DotProduct(AvoidConNormal, RelVelNorm));
					AvoidNormal = FMath::Lerp(AvoidConNormal, ForcedNormal, Blend).GetSafeNormal();
				}

				const FVector::FReal AvoidPenetration = (PredictiveAvoidanceAgentRadius + Collider.Radius + MovingAvoidanceParams.PredictiveAvoidanceDistance) - AvoidDist;
				const FVector::FReal AvoidMag = FMath::Square(FMath::Clamp(AvoidPenetration / MovingAvoidanceParams.PredictiveAvoidanceDistance, 0., 1.));
				const FVector::FReal AvoidMagDist = (1. - (CPA * InvPredictiveAvoidanceTime));
				const FVector AvoidForce = AvoidNormal * AvoidMag * AvoidMagDist * MovingAvoidanceParams.ObstaclePredictiveAvoidanceStiffness * StandingScaling;

				SteeringForce += AvoidForce;

#if WITH_MASSGAMEPLAY_DEBUG
				ArcMassMovingAvoidance::DebugDrawCylinder(AgentDebugContext, Collider.Location,
					Collider.Location + ArcMassMovingAvoidance::DebugLowCylinderOffset, Collider.Radius, ArcMassMovingAvoidance::AgentsColor);

				if (bHasForcedNormal)
				{
					ArcMassMovingAvoidance::DebugDrawCylinder(BaseDebugContext, Collider.Location,
						Collider.Location + ArcMassMovingAvoidance::DebugAgentHeightOffset, Collider.Radius, FColor::Red);
				}

				if (!SeparationForce.IsNearlyZero())
				{
					ArcMassMovingAvoidance::DebugDrawForce(AgentDebugContext,
						Collider.Location + ArcMassMovingAvoidance::DebugAgentSeparationHeightOffset,
						Collider.Location + ArcMassMovingAvoidance::DebugAgentSeparationHeightOffset + SeparationForce,
						Collider.bIsMoving ? ArcMassMovingAvoidance::AgentSeparationForceColor : ArcMassMovingAvoidance::AgentsNotMovingSeparationColor,
						ArcMassMovingAvoidance::SeparationThickness);
				}

				if (ArcMassMovingAvoidance::Tweakables::bEnableDetailedDebug)
				{
					ArcMassMovingAvoidance::DebugDrawCircle(AgentDebugContext, Collider.Location, Collider.Radius + MovingAvoidanceParams.PredictiveAvoidanceDistance, ArcMassMovingAvoidance::AgentsColor);
					ArcMassMovingAvoidance::DebugDrawCircle(AgentDebugContext, Collider.Location, Collider.Radius + MovingAvoidanceParams.ObstacleSeparationDistance, ArcMassMovingAvoidance::AgentSeparationForceColor);
				}

				if (AvoidForce.Size() > 0.)
				{
					ArcMassMovingAvoidance::DebugDrawHitPosition(AgentDebugContext, AgentLocation, DesVel, CPA, PredictiveAvoidanceAgentRadius, ArcMassMovingAvoidance::CurrentAgentColor);

					const FColor ColliderColor = Collider.bIsMoving ? ArcMassMovingAvoidance::AgentsColor : ArcMassMovingAvoidance::AgentsNotMovingColor;
					ArcMassMovingAvoidance::DebugDrawHitPosition(AgentDebugContext, Collider.Location, Collider.Velocity, CPA, Collider.Radius, ColliderColor);

					const FVector OtherHitPosition = Collider.Location + (Collider.Velocity * CPA);

					ArcMassMovingAvoidance::DebugDrawCylinder(AgentDebugContext, Collider.Location, Collider.Location + ArcMassMovingAvoidance::DebugAgentHeightOffset,
						AgentRadius, ColliderColor);

					ArcMassMovingAvoidance::DebugDrawForce(AgentDebugContext,
						OtherHitPosition + ArcMassMovingAvoidance::DebugAgentAvoidHeightOffset,
						OtherHitPosition + ArcMassMovingAvoidance::DebugAgentAvoidHeightOffset + AvoidForce,
						ArcMassMovingAvoidance::AgentAvoidForceColor, ArcMassMovingAvoidance::AvoidThickness);
				}
#endif // WITH_MASSGAMEPLAY_DEBUG
			}

			FVector AccumulatedAvoidanceForce = SteeringForce - Force.Value;
			AccumulatedAvoidanceForce *= NearStartScaling * NearEndScaling;
			SteeringForce = Force.Value + AccumulatedAvoidanceForce;

			if (bPreventBackwardMovement && bHasDesiredMovementFragments)
			{
				FMassDesiredMovementFragment& DesiredMovement = DesiredMovementList[EntityIt];

				const FVector TempDesiredVelocity  = DesiredMovement.DesiredVelocity + (UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel) * DeltaTime);
				const FVector PredictedDesiredVelocity = TempDesiredVelocity.GetClampedToMaxSize(Limits.MaxSpeed);

				constexpr FVector::FReal Cos45 = 0.707;
				const FVector::FReal Dot = PredictedDesiredVelocity.GetSafeNormal().Dot(MoveTarget.Forward);
				if (Dot < Cos45)
				{
					const FVector::FReal ForwardComponent = FMath::Abs(SteeringForce.Dot(MoveTarget.Forward));
					SteeringForce = ForwardComponent * MoveTarget.Forward;
				}

				constexpr FVector::FReal MinSpeed = 20;
				const bool bSlowing = PredictedDesiredVelocity.SizeSquared() < DesiredMovement.DesiredVelocity.SizeSquared();
				if (bSlowing && PredictedDesiredVelocity.Size() < MinSpeed)
				{
					DesiredMovement.DesiredVelocity = FVector::ZeroVector;
					SteeringForce = FVector::ZeroVector;
				}
			}

			Force.Value = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel);

#if WITH_MASSGAMEPLAY_DEBUG
			const FVector AgentAvoidSteeringForce = SteeringForce - OldSteeringForce;

			ArcMassMovingAvoidance::DebugDrawSummedForce(AgentDebugContext,
				AgentLocation + ArcMassMovingAvoidance::DebugAgentSeparationHeightOffset,
				AgentLocation + ArcMassMovingAvoidance::DebugAgentSeparationHeightOffset + TotalAgentSeparationForce,
				ArcMassMovingAvoidance::AgentSeparationForceColor);

			ArcMassMovingAvoidance::DebugDrawSummedForce(AgentDebugContext,
				AgentLocation + ArcMassMovingAvoidance::DebugAgentAvoidHeightOffset,
				AgentLocation + ArcMassMovingAvoidance::DebugAgentAvoidHeightOffset + AgentAvoidSteeringForce,
				ArcMassMovingAvoidance::AgentAvoidForceColor);

			ArcMassMovingAvoidance::DebugDrawArrow(BaseDebugContext,
				AgentLocation + ArcMassMovingAvoidance::DebugOutputForcesHeight,
				AgentLocation + ArcMassMovingAvoidance::DebugOutputForcesHeight + Force.Value,
				ArcMassMovingAvoidance::FinalSteeringForceColor, ArcMassMovingAvoidance::SteeringArrowHeadSize, ArcMassMovingAvoidance::SteeringThickness);
#endif // WITH_MASSGAMEPLAY_DEBUG
		}
	});
}
