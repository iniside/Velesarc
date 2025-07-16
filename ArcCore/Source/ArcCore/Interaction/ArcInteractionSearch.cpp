/**
 * This file is part of ArcX.
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

#include "ArcInteractionSearch.h"

#include "ArcInteractionLevelPlacedComponent.h"
#include "ArcInteractionReceiverComponent.h"
#include "Interaction/ArcInteractionRepresentation.h"
#include "TimerManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void FArcInteractionSearch_AsyncSphere::BeginSearch(UArcInteractionReceiverComponent* InReceiverComponent) const
{
	InteractableSearchDelegate.BindRaw(this, &FArcInteractionSearch_AsyncSphere::HandleTraceFinished, InReceiverComponent);
	
	MakeTrace(InReceiverComponent);
}

void FArcInteractionSearch_AsyncSphere::MakeTrace(UArcInteractionReceiverComponent* InReceiverComponent) const
{
	APawn* P = Cast<APawn>(InReceiverComponent->GetOwner());
	FCollisionObjectQueryParams ObjectQueryParams;
	const ECollisionChannel& Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false
		, InteractableObjectType);

	ObjectQueryParams.AddObjectTypesToQuery(Channel);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(P);
	static const FName TraceTag("UArcInteractionReceiverComponent::InteractableTrace");
	Params.OwnerTag = TraceTag;
	Params.bSkipNarrowPhase = true;

	InReceiverComponent->GetWorld()->AsyncSweepByObjectType(EAsyncTraceType::Multi
		, P->GetActorLocation()
		, P->GetActorLocation() + FVector(0
			  , 0
			  , 1)
		, FQuat::Identity
		, ObjectQueryParams
		, FCollisionShape::MakeSphere(SearchRadius)
		, Params
		, &InteractableSearchDelegate);
}

void FArcInteractionSearch_AsyncSphere::HandleTraceFinished(const FTraceHandle& TraceHandle
												, FTraceDatum& Datum
												, UArcInteractionReceiverComponent* InReceiverComponent) const
{
	TArray<AActor*> AdvertisedActors;
	AdvertisedActors.Reserve(32);
	TArray<TPair<FHitResult, TScriptInterface<IArcInteractionObjectInterface>>> Locations;
	Locations.Reserve(Datum.OutHits.Num());
	
	TSet<FGuid> FoundInteractions;
	FoundInteractions.Reserve(Datum.OutHits.Num());
	
	for (const FHitResult& Hit : Datum.OutHits)
	{
		AActor* ManaginActor = Hit.GetHitObjectHandle().GetManagingActor();
		
		TScriptInterface<IArcInteractionObjectInterface> Interface = ManaginActor;
		
		if (Interface == nullptr)
		{
			Interface = Hit.GetActor();
			if (Interface == nullptr)
			{
				TArray<UActorComponent*> ActorComponents = Hit.GetActor()->GetComponentsByInterface(UArcInteractionObjectInterface::StaticClass());
				if (ActorComponents.Num() > 0)
				{
					Interface = ActorComponents[0];
				}		
			}	
		}

		if (Interface)
		{
			FGuid Id = Interface->GetInteractionId(Hit);
			if (FoundInteractions.Contains(Id) == false)
			{
				Locations.Add( {Hit, Interface});
				FoundInteractions.Add(Id);
			}
			
		}
	}

	FilterTargets(Locations, InReceiverComponent);

	FTimerHandle Unused;
	InReceiverComponent->GetWorld()->GetTimerManager().SetTimer(Unused
		, FTimerDelegate::CreateRaw(this
		, &FArcInteractionSearch_AsyncSphere::MakeTrace, InReceiverComponent)
		, SearchInterval
		, false);
}

void FArcInteractionSearch_AsyncSphere::FilterTargets(const TArray<TPair<FHitResult, TScriptInterface<IArcInteractionObjectInterface>>>& InLocations
	, UArcInteractionReceiverComponent* InReceiverComponent) const
{
	APawn* Pawn = Cast<APawn>(InReceiverComponent->GetOwner());
	const FVector PLocation = Pawn->GetActorLocation();
	double Distance = 9999999999999999999999999999999.0;
	float Dot = -1;
	
	FVector EyeLocation = FVector::ZeroVector;
	FRotator EyeRotation = FRotator::ZeroRotator;
	Pawn->GetActorEyesViewPoint(EyeLocation
		, EyeRotation);

	float ConeHalfAngleDot = FMath::Cos(FMath::DegreesToRadians(20.f));
	float DotDifference = 1;

	FVector Center = Pawn->GetActorLocation(); // +(InPawn->GetActorForwardVector() * 85);

	FVector Extent = Pawn->GetActorForwardVector();
	Extent.Y = 150;
	Extent.Z = 100;
	Extent.X = 50;
	FBox B = FBox::BuildAABB(Center
		, Extent);

	FSphere::FReal ConeAngle = 25;
	FSphere::FReal CosConeAngle = FMath::Cos(ConeAngle);
	FSphere::FReal SinConeAngle = FMath::Sin(ConeAngle);

	FVector EyeLoc;
	FRotator EyeRot;
	Pawn->GetActorEyesViewPoint(EyeLoc
		, EyeRot);

	UE::Math::TSphere<double> Sphere = FMath::ComputeBoundingSphereForCone(Pawn->GetActorLocation()
		, EyeRot.Vector()
		, 190.0
		, CosConeAngle
		, SinConeAngle);

	TArray<int32> DotFiltered;

	int32 NewTarget = INDEX_NONE;
	for (int32 Idx = 0; Idx < InLocations.Num(); Idx++)
	{
		FVector InteractionLocation = InLocations[Idx].Value->GetInteractionLocation(InLocations[Idx].Key);
		bool bInside = Sphere.IsInside(InteractionLocation);
		if (bInside)
		{
			DotFiltered.Add(Idx);
		}
	}
	
	for (int32 Idx : DotFiltered)
	{
		// TODO:: Interactions - add claims.
		FVector InteractionLocation = InLocations[Idx].Value->GetInteractionLocation(InLocations[Idx].Key);
		double NewDistance = FVector::DistSquared(PLocation, InteractionLocation);
	
		if (NewDistance < Distance)
		{
			Distance = NewDistance;
	
			NewTarget = Idx;
		}
	}

	const FGuid Current = InReceiverComponent->GetCurrentInteractionId();
	const FGuid New = NewTarget != INDEX_NONE ? InLocations[NewTarget].Value->GetInteractionId(InLocations[NewTarget].Key) : FGuid();
	if (NewTarget != INDEX_NONE
		&& Current != New)
	{
		InReceiverComponent->StartInteraction(InLocations[NewTarget].Value, InLocations[NewTarget].Key);
	}
}
