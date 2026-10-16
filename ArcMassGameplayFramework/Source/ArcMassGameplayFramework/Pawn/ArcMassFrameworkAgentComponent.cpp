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

#include "Pawn/ArcMassFrameworkAgentComponent.h"

#include "MassAgentComponent.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"

UArcMassFrameworkAgentComponent::UArcMassFrameworkAgentComponent(const FObjectInitializer& ObjectInitializer)
{
}

void UArcMassFrameworkAgentComponent::OnRegister()
{
	// Bypass UMassAgentComponent::OnRegister — UArcMassSpawnerSubsystem owns the lifecycle from
	// BeginPlay. The engine path (UMassAgentSubsystem::RegisterAgentComponent) would call
	// EntityCreationPending() before BeginPlay, then double-spawn via HandlePendingInitialization
	// on the next PrePhysics phase. Server-driven non-puppet only: TemplateID and replication-puppet
	// wiring intentionally skipped — never place this component on a net-simulating client puppet.
	UActorComponent::OnRegister();
}

void UArcMassFrameworkAgentComponent::OnUnregister()
{
	// Mirror of OnRegister bypass. EndPlay clears the entity handle (state → None) before the base
	// OnUnregister would run, so UnregisterWithAgentSubsystem would no-op anyway; override for symmetry.
	UActorComponent::OnUnregister();
}

void UArcMassFrameworkAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	// Checkpoint D: log BeginPlay entry.
	UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] UArcMassFrameworkAgentComponent::BeginPlay: Owner=%ls",
		GetOwner() ? *GetOwner()->GetPathName() : TEXT("<null>"));

	UMassAgentComponent* AsAgentComp = this;
	SpawnedHandle = UArcMassSpawnerSubsystem::SpawnForAgentComponent(*AsAgentComp);

	// Checkpoint D: log SpawnForAgentComponent result.
	UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] UArcMassFrameworkAgentComponent::BeginPlay: SpawnForAgentComponent returned Idx=%d Serial=%d bSpawnerSeeded=%d Owner=%ls",
		SpawnedHandle.Index, SpawnedHandle.SerialNumber, (int32)SpawnedHandle.IsValid(),
		GetOwner() ? *GetOwner()->GetPathName() : TEXT("<null>"));

	if (SpawnedHandle.IsValid())
	{
		bSpawnerSeeded = true;
		// SetEntityHandle fires our override below, which routes to NotifyEntityReadyImmediately
		// because bSpawnerSeeded is now true. All observer-deferred sparse fragments are already
		// attached at this point: FEntityBuilder.Commit ran synchronously outside Mass processing.
		SetEntityHandle(SpawnedHandle);
	}
}

void UArcMassFrameworkAgentComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	UMassAgentComponent* AsAgentComp = this;
	UArcMassSpawnerSubsystem::DestroySpawnedForAgentComponent(*AsAgentComp, SpawnedHandle);
	SpawnedHandle = FMassEntityHandle();
	bSpawnerSeeded = false;
	Super::EndPlay(Reason);
}

void UArcMassFrameworkAgentComponent::SetEntityHandle(const FMassEntityHandle NewHandle)
{
	Super::SetEntityHandle(NewHandle);

	if (!NewHandle.IsValid())
	{
		return;
	}

	UArcMassPawnExtensionComponent* PawnExt = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());

	// Checkpoint D: log SetEntityHandle entry — handle, branch, PawnExt presence.
	if (bSpawnerSeeded)
	{
		UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] UArcMassFrameworkAgentComponent::SetEntityHandle: Idx=%d Serial=%d branch=spawner-seeded PawnExt=%ls Owner=%ls",
			NewHandle.Index, NewHandle.SerialNumber,
			PawnExt ? *PawnExt->GetPathName() : TEXT("<null>"),
			GetOwner() ? *GetOwner()->GetPathName() : TEXT("<null>"));
	}
	else
	{
		UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] UArcMassFrameworkAgentComponent::SetEntityHandle: Idx=%d Serial=%d branch=engine-path PawnExt=%ls Owner=%ls",
			NewHandle.Index, NewHandle.SerialNumber,
			PawnExt ? *PawnExt->GetPathName() : TEXT("<null>"),
			GetOwner() ? *GetOwner()->GetPathName() : TEXT("<null>"));
	}

	if (PawnExt == nullptr)
	{
		return;
	}

	if (bSpawnerSeeded)
	{
		// Spawner path: FEntityBuilder.Commit ran synchronously — observers have already fired
		// and all sparse fragments are attached. Skip the PrePhysics one-shot fence.
		PawnExt->NotifyEntityReadyImmediately();
	}
	else
	{
		// Engine path (UMassAgentSubsystem::HandlePendingInitialization): SetEntityHandle is
		// called while a FEntityCreationContext is still open — observers haven't fired yet.
		// RegisterMassReadyCallback defers DataAvailable -> DataInitialized until OnPhaseEnd(PrePhysics)
		// by which time observer-deferred sparse fragments (e.g. FArcMassItemStoreFragment) have flushed.
		PawnExt->RegisterMassReadyCallback();
		PawnExt->CheckDefaultInitialization();
	}
}
