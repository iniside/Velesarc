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

#pragma once

#include "Components/ActorComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include "ArcMassNavMoverInputComponent.generated.h"

class UNavMoverComponent;

UCLASS(ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCMASSMOVER_API UArcMassNavMoverInputComponent
		: public UActorComponent
		, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	/** Only DirectionalIntent and Velocity are meaningful; any other value behaves as DirectionalIntent. */
	UPROPERTY(EditAnywhere, Category = "Mover")
	EMoveInputType MoveInputType = EMoveInputType::DirectionalIntent;

	UArcMassNavMoverInputComponent(const FObjectInitializer& ObjectInitializer);

	virtual void ProduceInput_Implementation(int32 SimTimeMs
											 , FMoverInputCmdContext& InputCmdResult) override;

	UNavMoverComponent* GetNavMoverComponent() const;

private:
	UPROPERTY(Transient)
	mutable TObjectPtr<UNavMoverComponent> NavMoverComponent;
};
