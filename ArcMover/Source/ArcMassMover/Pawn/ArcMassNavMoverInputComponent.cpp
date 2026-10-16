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

#include "Pawn/ArcMassNavMoverInputComponent.h"

#include "DefaultMovementSet/NavMoverComponent.h"
#include "GameFramework/Actor.h"

UArcMassNavMoverInputComponent::UArcMassNavMoverInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UArcMassNavMoverInputComponent::ProduceInput_Implementation(int32 SimTimeMs
																 , FMoverInputCmdContext& InputCmdResult)
{
	FVector NavIntent = FVector::ZeroVector;
	FVector NavVelocity = FVector::ZeroVector;
	if (UNavMoverComponent* NavMover = GetNavMoverComponent())
	{
		NavMover->ConsumeNavMovementData(NavIntent, NavVelocity);
	}

	FCharacterDefaultInputs& CharacterInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	if (MoveInputType == EMoveInputType::Velocity)
	{
		CharacterInputs.SetMoveInput(EMoveInputType::Velocity, NavVelocity);
	}
	else
	{
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, NavVelocity.GetSafeNormal());
	}

	const FVector HorizontalDirection = FVector(NavVelocity.X, NavVelocity.Y, 0.0).GetSafeNormal();
	if (!HorizontalDirection.IsZero())
	{
		CharacterInputs.OrientationIntent = HorizontalDirection;
	}
}

UNavMoverComponent* UArcMassNavMoverInputComponent::GetNavMoverComponent() const
{
	if (NavMoverComponent)
	{
		return NavMoverComponent;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return nullptr;
	}

	NavMoverComponent = Owner->FindComponentByClass<UNavMoverComponent>();
	return NavMoverComponent;
}
