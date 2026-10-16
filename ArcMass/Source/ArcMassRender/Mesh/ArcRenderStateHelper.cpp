// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRenderStateHelper.h"

#include "MassEngineTypes.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassSignalSubsystem.h"
#include "Mesh/ArcRenderMeshFragments.h"

#if WITH_EDITOR
#include "ArcDestroyRenderStateContext.h"
#include "ArcPrimitiveRenderStateHelper.h"
#include "Mass/EntityQuery.h"
#include "MassEntitySubsystem.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#endif // WITH_EDITOR

// ----------------------------------------------------------------------//
// FArcRenderStateFragment
//----------------------------------------------------------------------//
void FArcRenderStateFragment::DestroyRenderStateHelper()
{
	RenderStateHelper = nullptr;
}

// ----------------------------------------------------------------------//
// FArcRenderStateHelper
//----------------------------------------------------------------------//
FArcRenderStateHelper::FArcRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager)
	: EntityHandle(InEntityHandle)
	, EntityManager(InEntityManager->AsShared())
{
}

UWorld* FArcRenderStateHelper::GetWorld() const
{
	return GetEntityManager().GetWorld();
}

FSceneInterface* FArcRenderStateHelper::GetScene() const
{
	if (UWorld* World = GetWorld())
	{
		return World->Scene;
	}
	return nullptr;
}

FMassEntityManager& FArcRenderStateHelper::GetEntityManager() const
{
	return EntityManager.Get();
}

const FArcRenderStateFragment& FArcRenderStateHelper::GetRenderStateFragment() const
{
	return GetEntityManager().GetFragmentDataChecked<FArcRenderStateFragment>(EntityHandle);
}

FArcRenderStateFragment& FArcRenderStateHelper::GetMutableRenderStateFragment()
{
	return GetEntityManager().GetFragmentDataChecked<FArcRenderStateFragment>(EntityHandle);
}

bool FArcRenderStateHelper::IsRenderStateCreated() const
{
	return ProxyState == EProxyCreationState::Created;
}

bool FArcRenderStateHelper::ShouldCreateRenderState() const
{
	return false;
}

void FArcRenderStateHelper::CreateRenderState(FRegisterComponentContext* Context)
{
}

void FArcRenderStateHelper::DestroyRenderState(FArcDestroyRenderStateContext* Context)
{
}

void FArcRenderStateHelper::UpdateVisibility()
{
}

void FArcRenderStateHelper::MarkPrecachePSOsRequired()
{
}

void FArcRenderStateHelper::PrecachePSOs()
{
}

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
void FArcRenderStateHelper::OnPrecacheFinished(int32 JobSetThatJustCompleted)
{
}
#endif // !WITH_EDITOR && UE_WITH_PSO_PRECACHING

bool FArcRenderStateHelper::IsRenderStateDelayed() const
{
	return ProxyState == EProxyCreationState::Delayed;
}

bool FArcRenderStateHelper::IsRenderStateDirty() const
{
	return bRenderStateDirty;
}

void FArcRenderStateHelper::MarkRenderStateDirty()
{
	if ((IsRenderStateCreated() || IsRenderStateDelayed()) && !bRenderStateDirty)
	{
		bRenderStateDirty = true;

		if (UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(GetWorld()))
		{
			SignalSubsystem->SignalEntity(UE::Mass::Signals::RenderStateDirty, EntityHandle);
		}
	}
}

#if WITH_EDITOR
void UE::ArcMassRender::StripWorldRenderStateProxies(UWorld& World)
{
	// Editor world only: that is the crash surface (componentless placed-entity render lives in the
	// editor world). PIE/Game teardown is owned by UArcEntityVisualizationSubsystem::PreDeinitialize;
	// scoping here means the two paths never touch the same entities — no PIE behavioral change.
	if (World.WorldType != EWorldType::Editor)
	{
		return;
	}
	// Scene already released (e.g. a ReleaseScene re-init path) → nothing to strip, and calling
	// DestroyRenderState then would trip its check(Scene) while a live proxy still exists.
	if (World.Scene == nullptr)
	{
		return;
	}
	UMassEntitySubsystem* Subsystem = World.GetSubsystem<UMassEntitySubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}
	FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();

	// Ad-hoc query over every componentless render entity (ISM holders + non-ISM one-offs); at editor
	// teardown all of them must come off the dying scene. Mirrors UArcDestroyRenderStateProcessor's
	// body, but deliberately does NOT call DestroyRenderStateHelper() (see loop comment).
	FMassEntityQuery Query(EntityManager.AsShared());
	Query.AddRequirement<FArcRenderStateFragment>(EMassFragmentAccess::ReadWrite);

	FArcDestroyRenderStateContext BatchingContext(nullptr);
	FMassExecutionContext ExecutionContext(EntityManager, 0.f);
	// Helpers needing a skeletal MeshObject release, collected during the strip pass and drained
	// AFTER BatchingContext.Process() (see second-pass comment below). Includes helpers with no live
	// proxy: a skinned holder preserves its FSkeletalMeshObject across proxy recreations
	// (FArcMassInstancedSkinnedMeshRenderStateHelper::DestroyRenderState), so a holder with
	// IsRenderStateCreated()==false can still own a live FSkeletalMeshUpdateHandle.
	TArray<FArcPrimitiveRenderStateHelper*> HelpersToReleaseMeshObject;
	Query.ForEachEntityChunk(ExecutionContext,
		[&BatchingContext, &HelpersToReleaseMeshObject](FMassExecutionContext& Context)
		{
			const TArrayView<FArcRenderStateFragment>& RenderStateFragments = Context.GetMutableFragmentView<FArcRenderStateFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcRenderStateFragment& RenderStateFragment = RenderStateFragments[*EntityIt];
				if (!RenderStateFragment.HasRenderStateHelper())
				{
					continue;
				}

				FArcPrimitiveRenderStateHelper& Helper = RenderStateFragment.GetRenderStateHelper<FArcPrimitiveRenderStateHelper>();

				// Mesh-object release is independent of proxy state, so queue every helper (no-op for
				// helpers owning no skeletal mesh object). Pointer is stable for the call — fragment
				// storage is not reallocated before the second pass.
				HelpersToReleaseMeshObject.Add(&Helper);

				if (!Helper.IsRenderStateCreated()) // Delayed/None states own no live proxy
				{
					continue;
				}

				if (!BatchingContext.IsInitialized())
				{
					BatchingContext.Initialize(Helper.GetScene());
				}

				// Strip the proxy ONLY. Unlike UArcDestroyRenderStateProcessor::Execute, do NOT call
				// RenderStateFragment.DestroyRenderStateHelper(): nulling the TSharedPtr would make a
				// later GetRenderStateHelper() (PreDeinitialize / ClearHolder) trip its checkf. Leaving
				// the helper alive keeps every subsequent teardown call idempotent.
				Helper.DestroyRenderState(&BatchingContext);
			}
		});

	if (BatchingContext.IsInitialized())
	{
		BatchingContext.Process();
	}

	// Second pass — release skeletal mesh objects only AFTER the batched proxy removals have been
	// flushed, mirroring ClearHolder's proxy-out-before-resource-release ordering: ReleaseResources()
	// frees LOD GPU buffers + the FSkeletalMeshUpdateHandle, and the now-removed proxy can no longer
	// reference them. Without this, the per-FScene FSkeletalMeshUpdateChannel destructor asserts on
	// live handles when the editor quits with GExitPurge skipping the actor's TearDownEditorRenderState.
	for (FArcPrimitiveRenderStateHelper* PendingHelper : HelpersToReleaseMeshObject)
	{
		PendingHelper->ReleaseMeshObjectForTeardown();
	}
}
#endif // WITH_EDITOR
