// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCuesTestHelpers.h"

#include "Engine/World.h"
#include "Mass/CommandBuffer.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcCues/ArcCueEntityFragment.h"
#include "ArcCues/ArcCueLostTargetProcessor.h"
#include "ArcCues/ArcCueSubsystem.h"
#include "ArcCues/ArcCuesModule.h"
#include "ArcMass/Attachment/ArcMassAttachmentFragment.h"
#include "ArcMass/Attachment/ArcMassAttachmentProcessor.h"

// ---------------------------------------------------------------------------
// FArcCueBehavior_TestProbe
// ---------------------------------------------------------------------------

void FArcCueBehavior_TestProbe::OnExecute(const FArcCueContext& Context) const
{
	UE_LOGF(LogArcCues, Display,"PROBE/%ls/EXEC handle=%u target=(%d:%d)",
		*ProbeName.ToString(),
		Context.Handle.GetId(),
		Context.Parameters.TargetEntity.Index, Context.Parameters.TargetEntity.SerialNumber);
}

TInstancedStruct<FArcCueBehaviorState> FArcCueBehavior_TestProbe::OnApplication(const FArcCueContext& Context) const
{
	UE_LOGF(LogArcCues, Display,"PROBE/%ls/APP handle=%u target=(%d:%d)",
		*ProbeName.ToString(),
		Context.Handle.GetId(),
		Context.Parameters.TargetEntity.Index, Context.Parameters.TargetEntity.SerialNumber);

	if (!bPersistent)
	{
		return {};
	}

	return TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_TestProbe>();
}

void FArcCueBehavior_TestProbe::OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const
{
	UE_LOGF(LogArcCues, Display,"PROBE/%ls/EVENT handle=%u",
		*ProbeName.ToString(), Context.Handle.GetId());
}

void FArcCueBehavior_TestProbe::OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const
{
	UE_LOGF(LogArcCues, Display,"PROBE/%ls/REM handle=%u",
		*ProbeName.ToString(), Context.Handle.GetId());
}

// ---------------------------------------------------------------------------
// FArcCueBehavior_AttachedTestProbe
// ---------------------------------------------------------------------------

TInstancedStruct<FArcCueBehaviorState> FArcCueBehavior_AttachedTestProbe::OnApplication(const FArcCueContext& Context) const
{
	UE_LOGF(LogArcCues, Display,"ATTACH-PROBE/%ls/APP handle=%u target=(%d:%d)",
		*ProbeName.ToString(),
		Context.Handle.GetId(),
		Context.Parameters.TargetEntity.Index, Context.Parameters.TargetEntity.SerialNumber);

	FMassEntityManager* EntityManager = Context.EntityManager;
	if (EntityManager == nullptr)
	{
		return {};
	}

	FArcCueEntityFragment CueFrag;
	CueFrag.OwningCue = Context.Handle;
	CueFrag.BehaviorIndex = Context.BehaviorIndex;

	FArcMassAttachmentFragment AttachFrag;
	AttachFrag.Target = Context.Parameters.TargetEntity;
	AttachFrag.LocalOffset = LocalOffset;

	FTransformFragment TransformFrag;
	TransformFrag.GetMutableTransform() = LocalOffset * Context.Parameters.Location;

	TArray<FInstancedStruct> Instances;
	Instances.Add(FInstancedStruct::Make(CueFrag));
	Instances.Add(FInstancedStruct::Make(AttachFrag));
	Instances.Add(FInstancedStruct::Make(TransformFrag));

	const FMassEntityHandle SpawnedEntity = EntityManager->CreateEntity(Instances);
	if (!SpawnedEntity.IsValid())
	{
		return {};
	}

	FArcCueBehaviorState_TestProbe State;
	State.SpawnedEntity = SpawnedEntity;
	return TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_TestProbe>(State);
}

void FArcCueBehavior_AttachedTestProbe::OnAttachmentLost(
	const FArcCueContext& Context,
	FMassEntityHandle AttachmentEntity,
	TInstancedStruct<FArcCueBehaviorState>& State) const
{
	const FArcCueBehaviorState_TestProbe* Typed = State.GetPtr<FArcCueBehaviorState_TestProbe>();
	const FMassEntityHandle SpawnedEntity = (Typed != nullptr) ? Typed->SpawnedEntity : FMassEntityHandle();
	const bool bReleased = SpawnedEntity.IsValid() && SpawnedEntity == AttachmentEntity;

	UE_LOGF(LogArcCues, Display,"ATTACH-PROBE/%ls/LOST handle=%u released=%d lost=(%d:%d) spawned=(%d:%d)",
		*ProbeName.ToString(), Context.Handle.GetId(), bReleased ? 1 : 0,
		AttachmentEntity.Index, AttachmentEntity.SerialNumber,
		SpawnedEntity.Index, SpawnedEntity.SerialNumber);

	if (bReleased)
	{
		State.Reset();
	}
}

void FArcCueBehavior_AttachedTestProbe::OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const
{
	UE_LOGF(LogArcCues, Display,"ATTACH-PROBE/%ls/EVENT handle=%u",
		*ProbeName.ToString(), Context.Handle.GetId());
}

void FArcCueBehavior_AttachedTestProbe::OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const
{
	UE_LOGF(LogArcCues, Display,"ATTACH-PROBE/%ls/REM handle=%u",
		*ProbeName.ToString(), Context.Handle.GetId());

	FArcCueBehaviorState_TestProbe* Typed = static_cast<FArcCueBehaviorState_TestProbe*>(&State);
	if (Typed == nullptr)
	{
		return;
	}

	FMassEntityManager* EntityManager = Context.EntityManager;
	if (EntityManager != nullptr && Typed->SpawnedEntity.IsValid()
		&& EntityManager->IsEntityValid(Typed->SpawnedEntity))
	{
		EntityManager->Defer().DestroyEntity(Typed->SpawnedEntity);
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace ArcCuesTest
{
	UArcCueDefinition* CreateTransientCueDef(
		FName Name,
		FGameplayTag CueTag,
		const FGameplayTagContainer& FilterTags,
		TArray<FInstancedStruct> Behaviors)
	{
		UArcCueDefinition* Def = NewObject<UArcCueDefinition>(GetTransientPackage(), Name, RF_Transient);
		Def->CueTag = CueTag;
		Def->FilterTags = FilterTags;
		Def->Behaviors = MoveTemp(Behaviors);
		// Tests construct defs ad-hoc; root them so GC running between AddCue and a
		// later cascade (which dereferences behaviors via FInstancedStruct memory on
		// the def) cannot collect the def underneath an active cue.
		Def->AddToRoot();
		return Def;
	}

	void ReleaseTransientCueDef(UArcCueDefinition* Def)
	{
		if (Def != nullptr && Def->IsRooted())
		{
			Def->RemoveFromRoot();
		}
	}

	UArcCueDefinition* CreateProbeDef(
		FName Name,
		FGameplayTag CueTag,
		const FGameplayTagContainer& FilterTags,
		FName ProbeName,
		bool bPersistent)
	{
		FArcCueBehavior_TestProbe Probe;
		Probe.ProbeName = ProbeName;
		Probe.bPersistent = bPersistent;

		TArray<FInstancedStruct> Behaviors;
		Behaviors.Add(FInstancedStruct::Make(Probe));
		return CreateTransientCueDef(Name, CueTag, FilterTags, MoveTemp(Behaviors));
	}

	FMassEntityManager& GetEntityManager(UWorld& World)
	{
		UMassEntitySubsystem* MES = World.GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		return MES->GetMutableEntityManager();
	}

	UArcCueSubsystem& GetCueSubsystem(UWorld& World)
	{
		UArcCueSubsystem* Sub = World.GetSubsystem<UArcCueSubsystem>();
		check(Sub);
		return *Sub;
	}

	void RunAttachmentProcessor(UWorld& World)
	{
		FMassEntityManager& EM = GetEntityManager(World);
		UArcMassAttachmentProcessor* Processor = NewObject<UArcMassAttachmentProcessor>(&World);
		Processor->CallInitialize(EM.GetOwner(), EM.AsShared());

		UE::Mass::FProcessingContext Context(EM, /*DeltaTime=*/0.0166f);
		UE::Mass::Executor::Run(*Processor, Context);
	}

	void RunLostTargetProcessor(UWorld& World)
	{
		FMassEntityManager& EM = GetEntityManager(World);
		UArcCueLostTargetProcessor* Processor = NewObject<UArcCueLostTargetProcessor>(&World);
		Processor->CallInitialize(EM.GetOwner(), EM.AsShared());

		UE::Mass::FProcessingContext Context(EM, /*DeltaTime=*/0.0166f);
		UE::Mass::Executor::Run(*Processor, Context);
	}

	FMassEntityHandle CreateTargetEntity(FMassEntityManager& EntityManager, const FTransform& InitialTransform)
	{
		FTransformFragment Transform;
		Transform.GetMutableTransform() = InitialTransform;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(Transform));

		return EntityManager.CreateEntity(Instances);
	}

	void SetEntityTransform(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FTransform& NewTransform)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			return;
		}
		FTransformFragment& Frag = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entity);
		Frag.GetMutableTransform() = NewTransform;
	}
}
