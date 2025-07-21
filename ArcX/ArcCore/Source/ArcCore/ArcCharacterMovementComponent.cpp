/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "Curves/BezierUtilities.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "Net/UnrealNetwork.h"
#include "Player/ArcCoreCharacter.h"

UArcCharacterMovementAttributeSet::UArcCharacterMovementAttributeSet()
{
	MaxVelocityMultiplier.SetBaseValue(1.f);
	MaxAccelerationMultiplier.SetBaseValue(1.f);
	MaxDecclerationMultiplier.SetBaseValue(1.f);
	MaxBrakingMultiplier.SetBaseValue(1.f);

	ARC_REGISTER_ATTRIBUTE_HANDLER(MaxVelocityMultiplier)
}

void UArcCharacterMovementAttributeSet::OnRep_MaxVelocityMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcCharacterMovementAttributeSet, MaxVelocityMultiplier, OldValue);
}

void UArcCharacterMovementAttributeSet::OnRep_MaxAccelerationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcCharacterMovementAttributeSet, MaxAccelerationMultiplier, OldValue);
}

void UArcCharacterMovementAttributeSet::OnRep_MaxDecclerationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcCharacterMovementAttributeSet, MaxDecclerationMultiplier, OldValue);
}

void UArcCharacterMovementAttributeSet::OnRep_MaxBrakingMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcCharacterMovementAttributeSet, MaxBrakingMultiplier, OldValue);
}

void UArcCharacterMovementAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_None;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcCharacterMovementAttributeSet, MaxVelocityMultiplier, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcCharacterMovementAttributeSet, MaxAccelerationMultiplier, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcCharacterMovementAttributeSet, MaxDecclerationMultiplier, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcCharacterMovementAttributeSet, MaxBrakingMultiplier, Params);
}

ARC_DECLARE_ATTRIBUTE_HANDLER(UArcCharacterMovementAttributeSet, MaxVelocityMultiplier)
{
	return [&](const struct FGameplayEffectModCallbackData& Data)
	{
		const float CurentMaxVeloityMultiplier = GetMaxVelocityMultiplier();
		const float NewValue = FMath::Clamp(0, 2.f, CurentMaxVeloityMultiplier);
		UE_LOG(LogTemp, Warning, TEXT("CurentMaxVeloityMultiplier: %f new %f"),CurentMaxVeloityMultiplier, NewValue);
		SetMaxVelocityMultiplier(NewValue);
	};
}

void FArcSavedMove_Character::Clear()
{
	Super::Clear();
	SavedMaxVelocityMultiplier = 1.f;
}

uint8 FArcSavedMove_Character::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	return Result;
}

bool FArcSavedMove_Character::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FArcSavedMove_Character::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	UArcCharacterMovementComponent* FPSMov = Cast<UArcCharacterMovementComponent>(Character->GetCharacterMovement());
	if (FPSMov)
	{
		SavedMaxVelocityMultiplier = FPSMov->MaxVelocityMultiplier;
	}
}

void FArcSavedMove_Character::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	UArcCharacterMovementComponent* FPSMov = Cast<UArcCharacterMovementComponent>(Character->GetCharacterMovement());
	if (FPSMov)
	{
		FPSMov->MaxVelocityMultiplier = SavedMaxVelocityMultiplier;
	}
}

FSavedMovePtr FArcNetworkPredictionData_Client_Character::AllocateNewMove()
{
	return FSavedMovePtr(new FArcSavedMove_Character());
}

void UArcCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds
	, const FVector& OldLocation
	, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds
		, OldLocation
		, OldVelocity);

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	if (!ArcASC || !ArcASC->GetAvatarActor())
	{
		return;
	}

	FVector::FReal Accel = GetCurrentAcceleration().Size();
	if (FMath::IsNearlyZero(Accel, 0.1))
	{
			ArcASC->RemoveLooseGameplayTag(IsMovingTag);
	}
	else
	{
		if (!ArcASC->HasMatchingGameplayTag(IsMovingTag))
		{
			ArcASC->AddLooseGameplayTag(IsMovingTag);
		}
	}
}

float UArcCharacterMovementComponent::GetMaxSpeed() const
{
	const float CurrentMaxSpeed = Super::GetMaxSpeed();
	float NewMaxSpeed = CurrentMaxSpeed;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	if (!ArcASC || !ArcASC->GetAvatarActor() || GetOwnerRole() == ROLE_SimulatedProxy)
	{
		return CurrentMaxSpeed;
	}
	FGameplayEffectAttributeCaptureSpec CaptureSpec = UArcCharacterMovementAttributeSet::GetMaxVelocityMultiplierCaptureSpecSource(ArcASC);
	FAggregatorEvaluateParameters Params;
	Params.IncludePredictiveMods = true;
	
	float OutMultiplier = 1.0f;
	CaptureSpec.AttemptCalculateAttributeMagnitude(Params, OutMultiplier);
	//UE_LOG(LogTemp, Warning, TEXT("OutMultiplier: %f"), OutMultiplier);
	MaxVelocityMultiplier = OutMultiplier;
	
	return CurrentMaxSpeed * OutMultiplier;
}

void UArcCharacterMovementComponent::CalcVelocity(float DeltaTime
	, float Friction
	, bool bFluid
	, float BrakingDeceleration)
{
	Super::CalcVelocity(DeltaTime
		, Friction
		, bFluid
		, BrakingDeceleration);

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	if (!ArcASC || !ArcASC->GetAvatarActor() || GetOwnerRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	FGameplayEffectAttributeCaptureSpec CaptureSpec = UArcCharacterMovementAttributeSet::GetMaxVelocityMultiplierCaptureSpecSource(ArcASC);
	FAggregatorEvaluateParameters Params;
	Params.IncludePredictiveMods = true;
	
	float OutMultiplier = 1.0f;
	CaptureSpec.AttemptCalculateAttributeMagnitude(Params, OutMultiplier);

	//Velocity *= OutMultiplier;
}

float UArcCharacterMovementComponent::GetMaxAcceleration() const
{
	float Accel = Super::GetMaxAcceleration();

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	if (!ArcASC || !ArcASC->GetAvatarActor()|| GetOwnerRole() == ROLE_SimulatedProxy)
	{
		return Accel;
	}
	FGameplayEffectAttributeCaptureSpec CaptureSpec = UArcCharacterMovementAttributeSet::GetMaxAccelerationMultiplierCaptureSpecSource(ArcASC);
	FAggregatorEvaluateParameters Params;
	Params.IncludePredictiveMods = true;
	
	float OutMultiplier = 1.0f;
	CaptureSpec.AttemptCalculateAttributeMagnitude(Params, OutMultiplier);


	return Accel * OutMultiplier;
	
}

float UArcCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	float Value = Super::GetMaxBrakingDeceleration();
	
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	if (!ArcASC || !ArcASC->GetAvatarActor() || GetOwnerRole() == ROLE_SimulatedProxy)
	{
		return Value;
	}
	FGameplayEffectAttributeCaptureSpec CaptureSpec = UArcCharacterMovementAttributeSet::GetMaxDecclerationMultiplierCaptureSpecSource(ArcASC);
	FAggregatorEvaluateParameters Params;
	Params.IncludePredictiveMods = true;
	
	float OutMultiplier = 1.0f;
	CaptureSpec.AttemptCalculateAttributeMagnitude(Params, OutMultiplier);

	return Value * OutMultiplier;
}

void UArcCharacterMovementComponent::ApplyVelocityBraking(float DeltaTime
	, float Friction
	, float BrakingDeceleration)
{
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	if (!ArcASC || !ArcASC->GetAvatarActor() || GetOwnerRole() == ROLE_SimulatedProxy)
	{
		Super::ApplyVelocityBraking(DeltaTime
		, Friction
		, BrakingDeceleration);
		return;
	}
	FGameplayEffectAttributeCaptureSpec CaptureSpec = UArcCharacterMovementAttributeSet::GetMaxBrakingMultiplierCaptureSpecSource(ArcASC);
	FAggregatorEvaluateParameters Params;
	Params.IncludePredictiveMods = true;
	
	float OutMultiplier = 1.0f;
	CaptureSpec.AttemptCalculateAttributeMagnitude(Params, OutMultiplier);
	
	Super::ApplyVelocityBraking(DeltaTime
		, Friction * OutMultiplier
		, BrakingDeceleration);
}

UArcSmoothNavPathDebugComponent::UArcSmoothNavPathDebugComponent(const FObjectInitializer& ObjectInitializ)
	: Super(ObjectInitializ)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
}

FBoxSphereBounds UArcSmoothNavPathDebugComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const FVector ActorPosition = LocalToWorld.GetTranslation();
	FBox NewBounds(ActorPosition, ActorPosition);

	if (Path)
	{
		const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();
		for (const FNavPathPoint& Point : PathPoints)
		{
			NewBounds += Point.Location;
		}
	}

	// Add some padding to make the whole path visible.
	NewBounds = NewBounds.ExpandBy(512 * 0.5);
	
	return FBoxSphereBounds(NewBounds);
}

#if WITH_EDITOR
void UArcSmoothNavPathDebugComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateTest();
	UpdateTestJump();
}
#endif

//namespace Arcx
//{
//	// Subdivide the spline into linear segments, adapting to its curvature (more curvy means more linear segments)
//	void SubdivideSpline(TArray<FVector>& OutSubdivisions, const USplineComponent& Spline, const float SubdivisionThreshold)
//	{
//		// Sample at least 2 points
//		const int32 NumSplinePoints = FMath::Max(Spline.GetNumberOfSplinePoints(), 2);
//
//		// The USplineComponent's Hermite spline tangents are 3 times larger than Bezier tangents and we need to convert before tessellation
//		constexpr double HermiteToBezierFactor = 3.0;
//
//		// Tessellate the spline segments
//		int32 PrevIndex = Spline.IsClosedLoop() ? (NumSplinePoints - 1) : INDEX_NONE;
//		for (int32 SplinePointIndex = 0; SplinePointIndex < NumSplinePoints; SplinePointIndex++)
//		{
//			if (PrevIndex >= 0)
//			{
//				const FSplinePoint PrevSplinePoint = Spline.GetSplinePointAt(PrevIndex, ESplineCoordinateSpace::World);
//				const FSplinePoint CurrSplinePoint = Spline.GetSplinePointAt(SplinePointIndex, ESplineCoordinateSpace::World);
//
//				// The first point of the segment is appended before tessellation since UE::CubicBezier::Tessellate does not add it
//				OutSubdivisions.Add(PrevSplinePoint.Position);
//
//				// Convert this segment of the spline from Hermite to Bezier and subdivide it 
//				UE::CubicBezier::Tessellate(OutSubdivisions,
//					PrevSplinePoint.Position,
//					PrevSplinePoint.Position + PrevSplinePoint.LeaveTangent / HermiteToBezierFactor,
//					CurrSplinePoint.Position - CurrSplinePoint.ArriveTangent / HermiteToBezierFactor,
//					CurrSplinePoint.Position,
//					SubdivisionThreshold);
//			}
//
//			PrevIndex = SplinePointIndex;
//		}
//	}
//}

namespace Arcx
{
	float PerpendicularDistance(const FVector& Point, const FVector& LineStart, const FVector& LineEnd)
	{
		FVector Line = LineEnd - LineStart;
		FVector PointLine = Point - LineStart;
		float Area = FVector::CrossProduct(Line, PointLine).Size();
		float Len = Line.Size();
		return Area / Len;
	}

	void DecimatePoints(const TArray<FNavPathPoint>& Points, int32 StartIndex
		, int32 EndIndex, float Epsilon, TArray<FNavPathPoint>& OutPoints)
	{
		if (EndIndex <= StartIndex + 1)
		{
			// No points to simplify, add the end-point and return
			OutPoints.Add(Points[StartIndex]);
			return;
		}

		// Find the point with the maximum distance
		float MaxDistance = 0.0f;
		int32 FarthestIndex = StartIndex;

		for (int32 i = StartIndex + 1; i < EndIndex; ++i)
		{
			float Distance = PerpendicularDistance(Points[i], Points[StartIndex], Points[EndIndex]);
			if (Distance > MaxDistance)
			{
				MaxDistance = Distance;
				FarthestIndex = i;
			}
		}

		// If max distance is greater than epsilon, recursively simplify
		if (MaxDistance > Epsilon)
		{
			DecimatePoints(Points, StartIndex, FarthestIndex, Epsilon, OutPoints);
			DecimatePoints(Points, FarthestIndex, EndIndex, Epsilon, OutPoints);
		}
		else
		{
			// Add the start-point since no fitting farthest point was found
			OutPoints.Add(Points[StartIndex]);
		}
		
	}

	TArray<FNavPathPoint> CalculateControlPoints(const TArray<FNavPathPoint>& Points)
	{
		TArray<FNavPathPoint> ControlPoints;

		if (Points.Num() < 2) {
			// Not enough points to calculate control points
			return ControlPoints;
		}

		// Extrapolate the first control point
		FVector FirstControlPoint = Points[0].Location - (Points[1].Location - Points[0].Location);
		ControlPoints.Add(FirstControlPoint);

		// Add all the original points
		for (const FNavPathPoint& Point : Points)
		{
			ControlPoints.Add(Point);
		}

		// Extrapolate the last control point
		FVector LastControlPoint = Points.Last().Location + (Points.Last().Location - Points[Points.Num() - 2].Location);
		ControlPoints.Add(LastControlPoint);

		return ControlPoints;
	}

	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
	{
		float T2 = T * T;
		float T3 = T2 * T;

		float X = 0.5f * ((2 * P1.X) + 
						  (-P0.X + P2.X) * T + 
						  (2 * P0.X - 5 * P1.X + 4 * P2.X - P3.X) * T2 + 
						  (-P0.X + 3 * P1.X - 3 * P2.X + P3.X) * T3);
    
		float Y = 0.5f * ((2 * P1.Y) + 
						  (-P0.Y + P2.Y) * T + 
						  (2 * P0.Y - 5 * P1.Y + 4 * P2.Y - P3.Y) * T2 + 
						  (-P0.Y + 3 * P1.Y - 3 * P2.Y + P3.Y) * T3);

		float Z = 0.5f * ((2 * P1.Z) + 
						  (-P0.Z + P2.Z) * T + 
						  (2 * P0.Z - 5 * P1.Z + 4 * P2.Z - P3.Z) * T2 + 
						  (-P0.Z + 3 * P1.Z - 3 * P2.Z + P3.Z) * T3);

		return FVector(X, Y, Z);
	}

	TArray<FNavPathPoint> GenerateCatmullRomSpline(const TArray<FNavPathPoint>& Points, int32 NumPoints = 100)
	{
		TArray<FNavPathPoint> SplinePoints;

		for (int32 i = 0; i < Points.Num() - 3; ++i) {
			const FVector& P0 = Points[i];
			const FVector& P1 = Points[i + 1];
			const FVector& P2 = Points[i + 2];
			const FVector& P3 = Points[i + 3];

			for (int32 j = 0; j < NumPoints; ++j) {
				float T = (float)j / (NumPoints - 1);
				SplinePoints.Add(CatmullRom(P0, P1, P2, P3, T));
			}
		}

		return SplinePoints;
	}
	
	FVector Interpolate(const FVector& P0, const FVector& P1, float T)
	{
		return FMath::Lerp(P0, P1, T);
	}

	TArray<FNavPathPoint> SmoothPoints(const TArray<FNavPathPoint>& Points
		, int32 WindowSize, float MinDsitance, TArray<bool>& HighCurvatureFlags)
	{
		TArray<FNavPathPoint> SmoothedPoints;

		for (int32 i = 0; i < Points.Num(); ++i)
		{
			if (HighCurvatureFlags[i])
			{
				FVector SmoothedPoint = FVector::ZeroVector;
				int32 Count = 0;

				// Smoothing with a simple moving average
				for (int32 j = -WindowSize; j <= WindowSize; ++j)
				{
					int32 Index = i + j;
					if (Index >= 0 && Index < Points.Num())
					{
						//FVector::FReal Dist = FVector::DistSquared(Points[Index], Points[i]);
						//if (Dist < (MinDsitance * MinDsitance))
						{
							SmoothedPoint += Points[Index];
							Count++;	
						}
					}
				}

				SmoothedPoint /= Count;
				SmoothedPoints.Add(SmoothedPoint);
			}
			else
			{
				SmoothedPoints.Add(Points[i]);
			}
		}

		return SmoothedPoints;
	}

	//TArray<FVector> SmoothAndTessellate(const TArray<FVector>& Points, int32 TessellationPoints, int32 SmoothWindowSize)
	//{
	//	TArray<FVector> SmoothedPoints = SmoothPoints(Points, SmoothWindowSize);
	//	TArray<FVector> TessellatedPoints;
	//
	//	// Tessellate the smoothed points
	//	for (int32 i = 0; i < SmoothedPoints.Num() - 1; ++i)
	//	{
	//		const FVector& P0 = SmoothedPoints[i];
	//		const FVector& P1 = SmoothedPoints[i + 1];
	//
	//		for (int32 j = 0; j < TessellationPoints; ++j)
	//		{
	//			float T = j / (float)(TessellationPoints - 1);
	//			FVector Point = Interpolate(P0, P1, T);
	//			TessellatedPoints.Add(Point);
	//		}
	//	}
	//
	//	return TessellatedPoints;
	//}

	float CalculateCurvature(const FVector& PrevPoint, const FVector& CurrentPoint, const FVector& NextPoint)
	{
		FVector V1 = CurrentPoint - PrevPoint;
		FVector V2 = NextPoint - CurrentPoint;

		V1.Normalize();
		V2.Normalize();

		float DotProduct = FVector::DotProduct(V1, V2);
		return FMath::Acos(DotProduct);
	}
	
	auto SmoothPathFunc (const TArray<FNavPathPoint>& PathPoints
	, TArray<FNavPathPoint>& OutPathPoints
	, int32 NumInterp
	, int32& CurrentInterp
	, TArray<FNavPathPoint>& CurrentPathPoints
	, int32 WindowSize
	, float MinDistance
	, float DotThreshold)
	{
		if (PathPoints.IsEmpty())
		{
			return;
		}
		
		TArray<bool> HighCurvatureFlags;
		HighCurvatureFlags.SetNum(PathPoints.Num());
		
		for (int32 i = 1; i < PathPoints.Num() - 1; ++i)
		{
			float Curvature = CalculateCurvature(PathPoints[i - 1], PathPoints[i], PathPoints[i + 1]);
			if (Curvature > DotThreshold)
			{
				HighCurvatureFlags[i] = true;
			}
			else
			{
				HighCurvatureFlags[i] = false;
			}
		}
		
		TArray<FNavPathPoint> SmoothedPoints = SmoothPoints(PathPoints, WindowSize, MinDistance, HighCurvatureFlags);

		//TArray<FNavPathPoint> SmoothedPoints = SmoothPoints(PathPoints, WindowSize, MinDistance);

		TArray<FNavPathPoint> Subdivisions;
		
		for (int32 Idx = 0; Idx < SmoothedPoints.Num() - 1; Idx++)
		{
			Subdivisions.Add(SmoothedPoints[Idx]);
			//OutPathPoints.Add(PathPoints[Idx]);
			//for (int32 SmoothIdx = 1; SmoothIdx < NumInterp; SmoothIdx++)
			if (HighCurvatureFlags[Idx])
			{
				if (SmoothedPoints.IsValidIndex(Idx + 1))
				{
					const float Alpha = (float)CurrentInterp / ((float)NumInterp + 1);
					const FVector interpolatedPoint = Interpolate(SmoothedPoints[Idx], SmoothedPoints[Idx + 1], Alpha);

					Subdivisions.Add(interpolatedPoint);	
				}
			}
		}

		Subdivisions.Add(SmoothedPoints[PathPoints.Num() - 1]);
		
		CurrentInterp++;
		if (CurrentInterp >= NumInterp)
		{
			OutPathPoints.Append(Subdivisions);
			return;
		}
		else
		{
			SmoothPathFunc(Subdivisions, OutPathPoints, NumInterp
				, CurrentInterp, Subdivisions, WindowSize, MinDistance, DotThreshold);
		}
		//OutPathPoints.Add(PathPoints[PathPoints.Num() - 1]);
		//for (const FVector& Subdiv : Subdivisions)
		//{
		//	OutPathPoints.Add(Subdiv);
		//}
	};

}

void UArcSmoothNavPathDebugComponent::UpdateTest()
{
	//UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
//
//	if (NavSys == nullptr)
//	{
//		return;
//	}
//	
//	const FVector ActorPosition = GetOwner()->GetActorLocation();
//	NavData = NavSys->GetNavDataForProps(NavAgentProps, ActorPosition);
//	
//	if (TargetActor == nullptr)
//	{
//		return;
//	}
//
//	const ARecastNavMesh* RecastNavData = Cast<ARecastNavMesh>(NavData);
//
//	FRecastDebugGeometry navGeo;
//	navGeo.bGatherNavMeshEdges = true;
//
//	//RecastNavData->BeginBatchQuery();
//	//RecastNavData->GetDebugGeometryForTile(navGeo, NavmeshTileIndex);
//	//RecastNavData->FinishBatchQuery();
//	
//	Path = nullptr;
//	const FVector TargetLocation = TargetActor->GetActorLocation();
//	const FVector CurrentLocation = GetOwner()->GetActorLocation();
//
//	const FPathFindingQuery PathQuery(this, *NavData, CurrentLocation, TargetLocation, UNavigationQueryFilter::GetQueryFilter(*NavData, this, FilterClass));
//	const FSharedConstNavQueryFilter NavQueryFilter = PathQuery.QueryFilter ? PathQuery.QueryFilter : NavData->GetDefaultQueryFilter();
//	const FPathFindingResult PathResult = NavSys->FindPathSync(NavAgentProps, PathQuery);
//	Path = PathResult.Path;
//	SmoothPath = MakeShared<FNavMeshPath>();
//
//	DecimatedPath = MakeShared<FNavMeshPath>();
//	
//	const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();
//
//	TArray<FVector> PathLocations;
//	for (const FNavPathPoint& Subdiv : PathPoints)
//	{
//		PathLocations.Add(Subdiv);
//	}
//	
//	TArray<FNavPathPoint>& DecimatedPathPoints = DecimatedPath->GetPathPoints();
//	Arcx::DecimatePoints(PathPoints, 0, PathPoints.Num() - 1, MaxDistance, DecimatedPathPoints);
//
//	TArray<FNavPathPoint>& SmoothPathPoints = SmoothPath->GetPathPoints();
//	TArray<FNavPathPoint> CurrentSmoothPathPoints;
//	int32 CurrentInterp = 1;
//
//	TArray<FNavPathPoint> ControlPoints = Arcx::CalculateControlPoints(DecimatedPathPoints);
//	TArray<FNavPathPoint> SmoothedPoints = Arcx::GenerateCatmullRomSpline(ControlPoints, NumPoints);
//	SmoothPathPoints.Append(SmoothedPoints);
//	
//	//Arcx::SmoothPathFunc(DecimatedPathPoints, SmoothPathPoints, NumInterpolation, CurrentInterp
//	//	, CurrentSmoothPathPoints, SmoothWindow, MinDistance, DotThreshold);
//	
//	MarkRenderStateDirty();
}

void UArcSmoothNavPathDebugComponent::UpdateTestJump()
{
	//JumpPath = MakeShared<FNavMeshPath>();
	//
	//if (!Path)
	//{
	//	return;
	//}
	//TArray<FNavPathPoint>& JumpPoints = JumpPath->GetPathPoints();
	//JumpPoints.Reserve(Path->GetPathPoints().Num());
	//
	//for (const FNavPathPoint& Subdiv : Path->GetPathPoints())
	//{
	//	JumpPoints.Add(Subdiv);
	//	if (Subdiv.CustomNavLinkId != FNavLinkId::Invalid)
	//	{
	//		
	//	}
	//}
}

void UArcSmoothNavPathDebugComponent::TickComponent(float DeltaTime
													, enum ELevelTick TickType
													, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime
		, TickType
		, ThisTickFunction);

	UpdateTest();
	UpdateTestJump();
}

FDebugRenderSceneProxy* UArcSmoothNavPathDebugComponent::CreateDebugSceneProxy()
{
	class FNavDebugRenderSceneProxy : public FDebugRenderSceneProxy
	{
	public:
		FNavDebugRenderSceneProxy(const UPrimitiveComponent* InComponent)
			: FDebugRenderSceneProxy(InComponent)
		{
		}
		
		virtual SIZE_T GetTypeHash() const override 
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override 
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bDynamicRelevance = true;
			// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
			Result.bSeparateTranslucency = Result.bNormalTranslucency = IsShown(View) && GIsEditor;
			return Result;
		}
	};

	FNavDebugRenderSceneProxy* DebugProxy = new FNavDebugRenderSceneProxy(this);
	check(DebugProxy);

	if (Path)
	{
		TSharedPtr<FNavMeshPath> NavMeshPath = StaticCastSharedPtr<FNavMeshPath>(Path);

		const TArray<FNavigationPortalEdge>& Corridos = NavMeshPath->GetPathCorridorEdges();
		const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();
		
		for (int32 PointIndex = 0; PointIndex < PathPoints.Num(); PointIndex++)
		{
			const FNavPathPoint& PathPoint = PathPoints[PointIndex];
			CurrentJumpPathTime += GetWorld()->DeltaTimeSeconds;
			if (PathPoint.CustomNavLinkId != FNavLinkId::Invalid)
			{
				
				
				if (CurrentJumpPathTime > 10.f)
				{
					CurrentJumpPathTime = 0.f;
					const FNavPathPoint& NextPathPoint = PathPoints[PointIndex + 1];
				
					Dir = NextPathPoint.Location - PathPoint.Location;
				
					RightCross = FVector::CrossProduct(FVector::UpVector, Dir.GetSafeNormal());
					LeftCross = FVector::CrossProduct(Dir.GetSafeNormal(), FVector::UpVector);
				
					EndLoc = PathPoint.Location + (RightCross.GetSafeNormal() * 50.f);
					LeftEndLoc = PathPoint.Location + (LeftCross.GetSafeNormal() * 50.f);

					Points.Reset(8);
					
					for (int32 i = 0; i < 8; i++)
					{
						Points.Add(FNavigationProjectionWork(PathPoint.Location + (RightCross.GetSafeNormal() * (50.f * (float)i))));
					}

					UpperPonts.Reset(8);

					for (int32 i = 0; i < 8; i++)
					{
						UpperPonts.Add(FNavigationProjectionWork(NextPathPoint.Location + (RightCross.GetSafeNormal() * (50.f * (float)i))));
					}
				
					FBoxSphereBounds BB(PathPoint.Location, FVector(400.0, 400.0, 50.0), 400.0);

					FBoxSphereBounds BB2(NextPathPoint.Location, FVector(400.0, 400.0, 50.0), 400.0);
					NavData->BatchProjectPoints(Points, BB.BoxExtent);
					NavData->BatchProjectPoints(UpperPonts, BB2.BoxExtent);

					int32 MaxIdx = Points.Num() > UpperPonts.Num() ? UpperPonts.Num() : Points.Num();
					SelctedIdx = FMath::RandRange(0, MaxIdx - 1);


				
					{
						UpperIdx = INDEX_NONE;
						if (SelctedIdx < MaxIdx - 1 && SelctedIdx >= 1)
						{
							UpperIdx = FMath::RandRange(SelctedIdx - 1, SelctedIdx + 1);
						}
						else
						{
							UpperIdx = SelctedIdx;
						}
						if (Points[SelctedIdx].bResult && UpperPonts[UpperIdx].bResult)
						{
							FVector Vel;
							UGameplayStatics::SuggestProjectileVelocity_CustomArc(GetOwner(), Vel
								, Points[SelctedIdx].OutLocation, UpperPonts[UpperIdx].OutLocation, 0, 0.4);

							FPredictProjectilePathParams Params;
							Params.LaunchVelocity = Vel;
							Params.StartLocation = Points[SelctedIdx].OutLocation;
						
							
						
							UGameplayStatics::PredictProjectilePath(GetOwner(), Params,  Result);

							
						}
					}
				
				//for (int32 PIdx = 0; PIdx < MaxIdx; PIdx++)
				//{
				//	if (Points[PIdx].bResult && UpperPonts[PIdx].bResult)
				//	{
				//		FVector Vel;
				//		UGameplayStatics::SuggestProjectileVelocity_CustomArc(GetOwner(), Vel
				//			, Points[PIdx].OutLocation, UpperPonts[PIdx].OutLocation, 0, 0.4);
				//
				//		FPredictProjectilePathParams Params;
				//		Params.LaunchVelocity = Vel;
				//		Params.StartLocation = Points[PIdx].OutLocation;
				//		
				//		FPredictProjectilePathResult Result;
				//		
				//		UGameplayStatics::PredictProjectilePath(GetOwner(), Params,  Result);
				//
				//		for (const FPredictProjectilePathPointData& Point : Result.PathData)
				//		{
				//			DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(10.f, Point.Location, FColor::Magenta));	
				//		}
				//		
				//		DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(20.f, Points[PIdx].OutLocation, FColor::Magenta));
				//		DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(20.f, UpperPonts[PIdx].OutLocation, FColor::Magenta));
				//	}
				//}
				
				}

				for (const FPredictProjectilePathPointData& Point : Result.PathData)
				{
					DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(10.f, Point.Location, FColor::Magenta));	
				}

				if (SelctedIdx != INDEX_NONE && UpperIdx != INDEX_NONE)
				{
					DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(20.f, Points[SelctedIdx].OutLocation, FColor::Magenta));
					DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(20.f, UpperPonts[UpperIdx].OutLocation, FColor::Magenta));

					DebugProxy->ArrowLines.Add(FDebugRenderSceneProxy::FArrowLine(PathPoint.Location, EndLoc, FColor::Magenta, 9.f));
					DebugProxy->ArrowLines.Add(FDebugRenderSceneProxy::FArrowLine(PathPoint.Location, LeftEndLoc, FColor::Magenta, 9.f));
				
					DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(20.f, PathPoint.Location, FColor::Blue));
				}
			}
			
			if ((PointIndex + 1) < PathPoints.Num())
			{
				const FNavPathPoint& NextPathPoint = PathPoints[PointIndex + 1];
				DebugProxy->ArrowLines.Add(FDebugRenderSceneProxy::FArrowLine(PathPoint.Location, NextPathPoint.Location, FColor::Yellow, 9.f));
				DebugProxy->Lines.Add(FDebugRenderSceneProxy::FDebugLine(PathPoint.Location, NextPathPoint.Location, FColor(255,0,0), 6.f));
			}
	
			DebugProxy->Texts.Add(FDebugRenderSceneProxy::FText3d(FString::Printf(TEXT("%d-%d"), PointIndex, FNavMeshNodeFlags(Path->GetPathPoints()[PointIndex].Flags).AreaFlags), PathPoint.Location, FColor::White));
		}
	}

	if (JumpPath)
	{
		
	}
	
	//
	//if (SmoothPath)
	//{
	//	const TArray<FNavPathPoint>& PathPoints = SmoothPath->GetPathPoints();
	//	for (int32 PointIndex = 0; PointIndex < PathPoints.Num(); PointIndex++)
	//	{
	//		const FNavPathPoint& PathPoint = PathPoints[PointIndex];
	//		DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(20.f, PathPoint.Location, FColor::Blue));
	//		
	//		if ((PointIndex + 1) < PathPoints.Num())
	//		{
	//			const FNavPathPoint& NextPathPoint = PathPoints[PointIndex + 1];
	//			DebugProxy->Lines.Add(FDebugRenderSceneProxy::FDebugLine(PathPoint.Location, NextPathPoint.Location, FColor(0,255,0), 6.f));
	//		}
	//
	//		DebugProxy->Texts.Add(FDebugRenderSceneProxy::FText3d(FString::Printf(TEXT("%d-%d"), PointIndex, FNavMeshNodeFlags(SmoothPath->GetPathPoints()[PointIndex].Flags).AreaFlags), PathPoint.Location, FColor::White));
	//	}
	//}
	//
	//
	//if (DecimatedPath)
	//{
	//	const TArray<FNavPathPoint>& PathPoints = DecimatedPath->GetPathPoints();
	//	for (int32 PointIndex = 0; PointIndex < PathPoints.Num(); PointIndex++)
	//	{
	//		const FNavPathPoint& PathPoint = PathPoints[PointIndex];
	//		DebugProxy->Spheres.Add(FDebugRenderSceneProxy::FSphere(10.f, PathPoint.Location, FColor::Magenta));
	//		
	//		if ((PointIndex + 1) < PathPoints.Num())
	//		{
	//			const FNavPathPoint& NextPathPoint = PathPoints[PointIndex + 1];
	//			DebugProxy->Lines.Add(FDebugRenderSceneProxy::FDebugLine(PathPoint.Location, NextPathPoint.Location, FColor::Yellow, 6.f));
	//		}
	//
	//		DebugProxy->Texts.Add(FDebugRenderSceneProxy::FText3d(FString::Printf(TEXT("%d-%d"), PointIndex, FNavMeshNodeFlags(DecimatedPath->GetPathPoints()[PointIndex].Flags).AreaFlags), PathPoint.Location, FColor::White));
	//	}
	//}

	
	return DebugProxy;
}
