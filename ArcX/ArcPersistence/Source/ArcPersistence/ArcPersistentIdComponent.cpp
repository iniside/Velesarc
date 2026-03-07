/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "ArcPersistentIdComponent.h"

#include "ArcWorldPersistenceSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectSaveContext.h"

UArcPersistentIdComponent::UArcPersistentIdComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

void UArcPersistentIdComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!PersistenceId.IsValid())
	{
#if WITH_EDITORONLY_DATA
		PersistenceId = Owner->GetActorInstanceGuid();
#endif
		if (!PersistenceId.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ArcPersistentIdComponent: No valid PersistenceId on %s. Skipping persistence registration."),
				*Owner->GetName());
			return;
		}
	}

	UGameInstance* GI = Owner->GetGameInstance();
	if (!GI)
	{
		return;
	}

	UArcWorldPersistenceSubsystem* WorldPersistence = GI->GetSubsystem<UArcWorldPersistenceSubsystem>();
	if (WorldPersistence)
	{
		WorldPersistence->RegisterActor(Owner, PersistenceId.ToString());
	}
}

#if WITH_EDITOR
void UArcPersistentIdComponent::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	if (ObjectSaveContext.IsCooking())
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			PersistenceId = Owner->GetActorInstanceGuid();
		}
	}
}
#endif
