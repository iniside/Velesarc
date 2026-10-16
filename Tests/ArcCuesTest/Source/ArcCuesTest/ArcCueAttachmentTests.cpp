// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Mass/CommandBuffer.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcCues/ArcCueDefinition.h"
#include "ArcCues/ArcCueFunctions.h"
#include "ArcCues/ArcCueSubsystem.h"
#include "ArcMass/Attachment/ArcMassAttachmentFragment.h"

#include "ArcCuesTestHelpers.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcCuesAttachTest_Cue, "ArcCuesTest.Attach.Cue");

TEST_CLASS(ArcCues_Attachment, "ArcCues.Attachment")
{
	FActorTestSpawner Spawner;
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	UArcCueSubsystem* Subsystem = nullptr;
	UArcCueDefinition* RegisteredDef = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		World = &Spawner.GetWorld();
		EM = &ArcCuesTest::GetEntityManager(*World);
		Subsystem = World->GetSubsystem<UArcCueSubsystem>();
		check(Subsystem);
	}

	AFTER_EACH()
	{
		if (RegisteredDef != nullptr)
		{
			Subsystem->UnregisterDefinitionForTests(RegisteredDef);
			ArcCuesTest::ReleaseTransientCueDef(RegisteredDef);
			RegisteredDef = nullptr;
		}
	}

	TEST_METHOD(AttachedCueEntity_FollowsTargetTransform)
	{
		// Build target at origin, follower attaches at LocalOffset.
		const FMassEntityHandle Target = ArcCuesTest::CreateTargetEntity(*EM, FTransform::Identity);

		FArcMassAttachmentFragment AttachFrag;
		AttachFrag.Target = Target;
		AttachFrag.LocalOffset = FTransform(FQuat::Identity, FVector(50.f, 0.f, 0.f));

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(AttachFrag));
		Instances.Add(FInstancedStruct::Make(FTransformFragment{}));
		const FMassEntityHandle Follower = EM->CreateEntity(Instances);

		// Move the target.
		const FTransform NewTargetXform(FQuat::Identity, FVector(100.f, 200.f, 0.f));
		ArcCuesTest::SetEntityTransform(*EM, Target, NewTargetXform);

		ArcCuesTest::RunAttachmentProcessor(*World);

		const FTransformFragment& Resulting = EM->GetFragmentDataChecked<FTransformFragment>(Follower);
		const FVector ResultLoc = Resulting.GetTransform().GetLocation();
		const FVector Expected = (AttachFrag.LocalOffset * NewTargetXform).GetLocation();
		ASSERT_THAT(IsTrue(ResultLoc.Equals(Expected, 0.01f)));
	}

	TEST_METHOD(AttachedCueEntity_TargetDestroyed_FollowerGetsLostTag)
	{
		const FMassEntityHandle Target = ArcCuesTest::CreateTargetEntity(*EM);

		FArcMassAttachmentFragment AttachFrag;
		AttachFrag.Target = Target;
		AttachFrag.LocalOffset = FTransform::Identity;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(AttachFrag));
		Instances.Add(FInstancedStruct::Make(FTransformFragment{}));
		const FMassEntityHandle Follower = EM->CreateEntity(Instances);

		// Tick model: Defer().DestroyEntity queues onto EM's main command buffer.
		// Executor::Run flushes at END of each run, so tick 1 only resolves the
		// destroy; tick 2 is the first one where the attachment processor sees
		// Target invalid and tags the follower lost.
		EM->Defer().DestroyEntity(Target);

		ArcCuesTest::RunAttachmentProcessor(*World); // tick 1: flushes destroy
		ArcCuesTest::RunAttachmentProcessor(*World); // tick 2: tags follower lost

		const FMassEntityView LostView(*EM, Follower);
		ASSERT_THAT(IsTrue(LostView.HasTag<FArcMassAttachmentLostTag>()));
	}

	TEST_METHOD(AttachedCueEntity_LostTag_ReleasesBehaviorWithoutRemovingCue)
	{
		FArcCueBehavior_AttachedTestProbe Behavior;
		Behavior.ProbeName = TEXT("AttachLost");
		Behavior.LocalOffset = FTransform::Identity;

		TArray<FInstancedStruct> Behaviors;
		Behaviors.Add(FInstancedStruct::Make(Behavior));
		RegisteredDef = ArcCuesTest::CreateTransientCueDef(
			TEXT("AttachedProbeDef"), TAG_ArcCuesAttachTest_Cue, FGameplayTagContainer(), MoveTemp(Behaviors));
		Subsystem->RegisterDefinitionForTests(RegisteredDef);

		TestRunner->AddExpectedMessage(TEXT("ATTACH-PROBE/AttachLost/APP"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
		TestRunner->AddExpectedMessage(TEXT("ATTACH-PROBE/AttachLost/LOST handle=[0-9]+ released=1"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
		TestRunner->AddExpectedMessage(TEXT("ATTACH-PROBE/AttachLost/EVENT"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);

		const FMassEntityHandle Target = ArcCuesTest::CreateTargetEntity(*EM);

		FArcCueParameters Params;
		Params.TargetEntity = Target;
		const FArcCueHandle Handle = ArcCues::AddCue(World, TAG_ArcCuesAttachTest_Cue, Params);
		ASSERT_THAT(IsTrue(Handle.IsValid()));

		// Tick model — three runs to converge:
		//   tick 1: AttachmentProcessor sees Target valid (destroy still queued); flush at end resolves destroy.
		//   tick 2: AttachmentProcessor sees Target invalid, defers AddTag<LostTag>; flush at end applies the tag.
		//   tick 3: LostTargetProcessor sweeps lost-tagged cue entities, calls OnCueAttachmentLost (outside chunk
		//           iteration), which fires behavior OnAttachmentLost and defers the cue entity destroy.
		EM->Defer().DestroyEntity(Target);
		ArcCuesTest::RunAttachmentProcessor(*World);
		ArcCuesTest::RunAttachmentProcessor(*World);
		ArcCuesTest::RunLostTargetProcessor(*World);

		const FArcCueEvent Event;
		ArcCues::SendCueEvent(World, Handle, Event);
		ArcCues::RemoveCue(World, Handle);
		ArcCues::SendCueEvent(World, Handle, Event);
	}
};
