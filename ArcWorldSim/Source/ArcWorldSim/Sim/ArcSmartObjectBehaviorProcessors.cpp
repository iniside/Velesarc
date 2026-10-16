// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcSmartObjectBehaviorProcessors.h"

#include "Engine/World.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcNeedBands.h"
#include "Fragments/ArcNeedFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Mass/ExecutionContext.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectTypes.h"
#include "Operations/ArcMassItemOperations.h"
#include "Sim/ArcBodyCommandLogic.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBodyItemTransfer.h"
#include "Sim/ArcBodyProcessor.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcEatLogic.h"
#include "Sim/ArcItemInteractionFragment.h"
#include "Sim/ArcSmartObjectBehaviorFragments.h"
#include "Processors/ArcNeedProcessors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSmartObjectBehaviorProcessors)

namespace ArcSmartObjectBehaviorProcessorsPrivate
{
void ConfigureTimedQuery(
	FMassEntityQuery& Query,
	const TSharedRef<FMassEntityManager>& EntityManager,
	const UScriptStruct* BehaviorFragment)
{
	Query.Initialize(EntityManager);
	Query.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadWrite);
	Query.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadWrite);
	Query.AddSparseRequirement<FArcSmartObjectBehaviorFragment>(EMassFragmentPresence::All);
	Query.AddSparseRequirement(BehaviorFragment, EMassFragmentPresence::All);
	Query.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

bool IsCurrentInteraction(
	const FArcBodyFragment& Body,
	const FMassSmartObjectUserFragment& User,
	const FArcSmartObjectBehaviorFragment& Lifecycle,
	const uint32 BehaviorSerial)
{
	return Body.State == EArcBodyState::Interaction
		&& Body.Command.IsActive()
		&& Body.Command.Serial == Lifecycle.CommandSerial
		&& Lifecycle.CommandSerial == BehaviorSerial
		&& User.InteractionStatus == EMassSmartObjectInteractionStatus::InProgress;
}

bool IsUseTimeComplete(FArcSmartObjectBehaviorFragment& Lifecycle, const float DeltaTime)
{
	Lifecycle.UseElapsed += DeltaTime;
	return Lifecycle.UseDuration <= 0.0f || Lifecycle.UseElapsed >= Lifecycle.UseDuration;
}

void QueueCompletion(
	FArcBodyFragment& Body,
	FMassSmartObjectUserFragment& User,
	FArcSmartObjectBehaviorFragment& Lifecycle,
	const EArcBodyCommandResult Result,
	const FMassEntityHandle Entity,
	TArray<FMassEntityHandle>& EntitiesToSignal)
{
	if (Lifecycle.bCompletionQueued
		|| !ArcBodyCommand::SetBehaviorResult(Body, Lifecycle.CommandSerial, Result))
	{
		return;
	}

	Lifecycle.bCompletionQueued = true;
	User.InteractionStatus = EMassSmartObjectInteractionStatus::BehaviorCompleted;
	EntitiesToSignal.Add(Entity);
}

int32 GatherItem(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const FMassEntityHandle GatherArea,
	const FArcItemInteractionFragment& Interaction)
{
	if (!EntityManager.IsEntityValid(GatherArea)
		|| !Interaction.Item.IsValid()
		|| Interaction.RequestedQuantity <= 0)
	{
		return 0;
	}

	const FMassEntityView GatherAreaView(EntityManager, GatherArea);
	const FArcGatherAreaConfigFragment* Config =
		GatherAreaView.GetConstSharedFragmentDataPtr<FArcGatherAreaConfigFragment>();
	if (!Config
		|| Config->ItemDefinition != Interaction.Item
		|| Interaction.RequestedQuantity > Config->AmountPerVisit)
	{
		return 0;
	}

	const FArcItemSpec Spec = FArcItemSpec::NewItem(
		Interaction.Item,
		1,
		Interaction.RequestedQuantity);
	const FArcItemId AddedId = ArcMassItems::AddItem(
		EntityManager,
		Entity,
		FArcMassItemStoreFragment::StaticStruct(),
		Spec);
	return AddedId.IsValid() ? Interaction.RequestedQuantity : 0;
}

int32 CountAllItems(const FArcMassItemStoreFragment& Store)
{
	int32 Count = 0;
	for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data)
		{
			Count += static_cast<int32>(Data->GetStacks());
		}
	}
	return Count;
}
}

UArcSmartObjectWorkBehaviorProcessor::UArcSmartObjectWorkBehaviorProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcSmartObjectWorkBehaviorProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSparseRequirement<FArcSmartObjectBehaviorFragment>(EMassFragmentPresence::All);
	EntityQuery.AddSparseRequirement<FArcSmartObjectWorkBehaviorFragment>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcSmartObjectWorkBehaviorProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& ChunkContext)
		{
			const TConstArrayView<FArcBodyFragment> Bodies = ChunkContext.GetFragmentView<FArcBodyFragment>();
			const TConstArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetFragmentView<FMassSmartObjectUserFragment>();
			const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcSmartObjectBehaviorFragment* Lifecycle =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectBehaviorFragment>(EntityIt);
				FArcSmartObjectWorkBehaviorFragment* Work =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectWorkBehaviorFragment>(EntityIt);
				if (!Lifecycle || !Work
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsCurrentInteraction(
						Bodies[EntityIt], Users[EntityIt], *Lifecycle, Work->CommandSerial))
				{
					continue;
				}

				Work->WorkShiftElapsed += DeltaTime;
				if (Work->WorkShiftDuration <= 0.0f || Work->WorkShiftElapsed >= Work->WorkShiftDuration)
				{
					Work->WorkShiftElapsed = 0.0f;
				}

				Work->WorkLoopElapsed += DeltaTime;
				if (Work->WorkLoopDuration <= 0.0f || Work->WorkLoopElapsed >= Work->WorkLoopDuration)
				{
					Work->WorkLoopElapsed = 0.0f;
				}
			}
		});
}

UArcSmartObjectPickupBehaviorProcessor::UArcSmartObjectPickupBehaviorProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcSmartObjectPickupBehaviorProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ArcSmartObjectBehaviorProcessorsPrivate::ConfigureTimedQuery(
		EntityQuery, EntityManager, FArcSmartObjectPickupBehaviorFragment::StaticStruct());
	EntityQuery.AddSparseRequirement<FArcItemInteractionFragment>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcSmartObjectPickupBehaviorProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> EntitiesToSignal;
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &EntitiesToSignal](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcSmartObjectBehaviorFragment* Lifecycle =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectBehaviorFragment>(EntityIt);
				FArcSmartObjectPickupBehaviorFragment* Pickup =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectPickupBehaviorFragment>(EntityIt);
				FArcItemInteractionFragment* Interaction =
					ChunkContext.GetMutableSparseElement<FArcItemInteractionFragment>(EntityIt);
				if (!Lifecycle || !Pickup || !Interaction
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsCurrentInteraction(
						Bodies[EntityIt], Users[EntityIt], *Lifecycle, Pickup->CommandSerial)
					|| Interaction->CommandSerial != Lifecycle->CommandSerial
					|| Lifecycle->bEffectApplied || Lifecycle->bCompletionQueued
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsUseTimeComplete(*Lifecycle, DeltaTime))
				{
					continue;
				}

				Lifecycle->bEffectApplied = true;
				FArcBodyFragment& Body = Bodies[EntityIt];
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				const int32 Taken = ArcBodyItemTransfer::Pickup(
					EntityManager,
					Entity,
					Body.Command.TargetEntity,
					Interaction->Item,
					Interaction->RequestedQuantity);
				Interaction->ProcessedQuantity = Taken;
				ArcSmartObjectBehaviorProcessorsPrivate::QueueCompletion(
					Body,
					Users[EntityIt],
					*Lifecycle,
					Taken > 0 ? EArcBodyCommandResult::Succeeded : EArcBodyCommandResult::Failed,
					Entity,
					EntitiesToSignal);
			}
		});

	if (!EntitiesToSignal.IsEmpty())
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
		SignalSubsystem.SignalEntities(UE::Mass::Signals::SmartObjectInteractionDone, EntitiesToSignal);
	}
}

UArcSmartObjectDropBehaviorProcessor::UArcSmartObjectDropBehaviorProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcSmartObjectDropBehaviorProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ArcSmartObjectBehaviorProcessorsPrivate::ConfigureTimedQuery(
		EntityQuery, EntityManager, FArcSmartObjectDropBehaviorFragment::StaticStruct());
	EntityQuery.AddSparseRequirement<FArcItemInteractionFragment>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcSmartObjectDropBehaviorProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> EntitiesToSignal;
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &EntitiesToSignal](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcSmartObjectBehaviorFragment* Lifecycle =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectBehaviorFragment>(EntityIt);
				FArcSmartObjectDropBehaviorFragment* Drop =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectDropBehaviorFragment>(EntityIt);
				FArcItemInteractionFragment* Interaction =
					ChunkContext.GetMutableSparseElement<FArcItemInteractionFragment>(EntityIt);
				if (!Lifecycle || !Drop
					|| !Interaction
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsCurrentInteraction(
						Bodies[EntityIt], Users[EntityIt], *Lifecycle, Drop->CommandSerial)
					|| Interaction->CommandSerial != Lifecycle->CommandSerial
					|| Lifecycle->bEffectApplied || Lifecycle->bCompletionQueued
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsUseTimeComplete(*Lifecycle, DeltaTime))
				{
					continue;
				}

				Lifecycle->bEffectApplied = true;
				FArcBodyFragment& Body = Bodies[EntityIt];
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				const int32 Delivered = ArcBodyItemTransfer::Drop(
					EntityManager,
					Entity,
					Body.Command.TargetEntity,
					Interaction->Item,
					Interaction->RequestedQuantity);
				Interaction->ProcessedQuantity = Delivered;
				ArcSmartObjectBehaviorProcessorsPrivate::QueueCompletion(
					Body,
					Users[EntityIt],
					*Lifecycle,
					Delivered > 0 ? EArcBodyCommandResult::Succeeded : EArcBodyCommandResult::Failed,
					Entity,
					EntitiesToSignal);
			}
		});

	if (!EntitiesToSignal.IsEmpty())
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
		SignalSubsystem.SignalEntities(UE::Mass::Signals::SmartObjectInteractionDone, EntitiesToSignal);
	}
}

UArcSmartObjectGatherBehaviorProcessor::UArcSmartObjectGatherBehaviorProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcSmartObjectGatherBehaviorProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ArcSmartObjectBehaviorProcessorsPrivate::ConfigureTimedQuery(
		EntityQuery, EntityManager, FArcSmartObjectGatherBehaviorFragment::StaticStruct());
	EntityQuery.AddSparseRequirement<FArcItemInteractionFragment>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcSmartObjectGatherBehaviorProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> EntitiesToSignal;
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &EntitiesToSignal](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcSmartObjectBehaviorFragment* Lifecycle =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectBehaviorFragment>(EntityIt);
				FArcSmartObjectGatherBehaviorFragment* Gather =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectGatherBehaviorFragment>(EntityIt);
				FArcItemInteractionFragment* Interaction =
					ChunkContext.GetMutableSparseElement<FArcItemInteractionFragment>(EntityIt);
				if (!Lifecycle || !Gather || !Interaction
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsCurrentInteraction(
						Bodies[EntityIt], Users[EntityIt], *Lifecycle, Gather->CommandSerial)
					|| Interaction->CommandSerial != Lifecycle->CommandSerial
					|| Lifecycle->bEffectApplied || Lifecycle->bCompletionQueued
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsUseTimeComplete(*Lifecycle, DeltaTime))
				{
					continue;
				}

				Lifecycle->bEffectApplied = true;
				FArcBodyFragment& Body = Bodies[EntityIt];
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				const int32 Gathered = ArcSmartObjectBehaviorProcessorsPrivate::GatherItem(
					EntityManager,
					Entity,
					Body.Command.TargetEntity,
					*Interaction);
				Interaction->ProcessedQuantity = Gathered;
				ArcSmartObjectBehaviorProcessorsPrivate::QueueCompletion(
					Body,
					Users[EntityIt],
					*Lifecycle,
					Gathered > 0 ? EArcBodyCommandResult::Succeeded : EArcBodyCommandResult::Failed,
					Entity,
					EntitiesToSignal);
			}
		});

	if (!EntitiesToSignal.IsEmpty())
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
		SignalSubsystem.SignalEntities(UE::Mass::Signals::SmartObjectInteractionDone, EntitiesToSignal);
	}
}

UArcSmartObjectDrawBehaviorProcessor::UArcSmartObjectDrawBehaviorProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcSmartObjectDrawBehaviorProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ArcSmartObjectBehaviorProcessorsPrivate::ConfigureTimedQuery(
		EntityQuery, EntityManager, FArcSmartObjectDrawBehaviorFragment::StaticStruct());
	EntityQuery.AddRequirement<FArcCarryCapabilityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSparseRequirement<FArcMassItemStoreFragment>(EMassFragmentPresence::All);
	EntityQuery.AddSparseRequirement<FArcItemInteractionFragment>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcSmartObjectDrawBehaviorProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> EntitiesToSignal;
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &EntitiesToSignal](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			const TConstArrayView<FArcCarryCapabilityFragment> Capabilities =
				ChunkContext.GetFragmentView<FArcCarryCapabilityFragment>();
			const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcSmartObjectBehaviorFragment* Lifecycle =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectBehaviorFragment>(EntityIt);
				FArcSmartObjectDrawBehaviorFragment* Draw =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectDrawBehaviorFragment>(EntityIt);
				FArcMassItemStoreFragment* BodyStore =
					ChunkContext.GetMutableSparseElement<FArcMassItemStoreFragment>(EntityIt);
				FArcItemInteractionFragment* Interaction =
					ChunkContext.GetMutableSparseElement<FArcItemInteractionFragment>(EntityIt);
				if (!Lifecycle || !Draw || !BodyStore || !Interaction
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsCurrentInteraction(
						Bodies[EntityIt], Users[EntityIt], *Lifecycle, Draw->CommandSerial)
					|| Interaction->CommandSerial != Lifecycle->CommandSerial
					|| Lifecycle->bEffectApplied || Lifecycle->bCompletionQueued
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsUseTimeComplete(*Lifecycle, DeltaTime))
				{
					continue;
				}

				Lifecycle->bEffectApplied = true;
				FArcBodyFragment& Body = Bodies[EntityIt];
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				int32 Added = 0;
				if (EntityManager.IsEntityValid(Body.Command.TargetEntity)
					&& Interaction->Item.IsValid() && Interaction->RequestedQuantity > 0)
				{
					const FMassEntityView SourceView(EntityManager, Body.Command.TargetEntity);
					const FArcItemSourceConfigFragment* Config =
						SourceView.GetConstSharedFragmentDataPtr<FArcItemSourceConfigFragment>();
					if (Config)
					{
						const FPrimaryAssetId SourceItem = Config->ItemDefinition;
						if (SourceItem == Interaction->Item)
						{
							const int32 FreeCapacity = FMath::Max(
								0,
								Capabilities[EntityIt].Capacity
									- ArcSmartObjectBehaviorProcessorsPrivate::CountAllItems(*BodyStore));
							const int32 Amount = FMath::Min3(
								Interaction->RequestedQuantity,
								Config->AmountPerInteraction,
								FreeCapacity);
							if (Amount > 0)
							{
								const FArcItemSpec Spec = FArcItemSpec::NewItem(Interaction->Item, 1, Amount);
								const FArcItemId AddedId = ArcMassItems::AddItem(
									EntityManager,
									Entity,
									FArcMassItemStoreFragment::StaticStruct(),
									Spec);
								Added = AddedId.IsValid() ? Amount : 0;
							}
						}
					}
				}

				Interaction->ProcessedQuantity = Added;
				ArcSmartObjectBehaviorProcessorsPrivate::QueueCompletion(
					Body,
					Users[EntityIt],
					*Lifecycle,
					Added > 0 ? EArcBodyCommandResult::Succeeded : EArcBodyCommandResult::Failed,
					Entity,
					EntitiesToSignal);
			}
		});

	if (!EntitiesToSignal.IsEmpty())
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
		SignalSubsystem.SignalEntities(UE::Mass::Signals::SmartObjectInteractionDone, EntitiesToSignal);
	}
}

UArcSmartObjectEatBehaviorProcessor::UArcSmartObjectEatBehaviorProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcSmartObjectEatBehaviorProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ArcSmartObjectBehaviorProcessorsPrivate::ConfigureTimedQuery(
		EntityQuery, EntityManager, FArcSmartObjectEatBehaviorFragment::StaticStruct());
	EntityQuery.AddRequirement<FArcHungerNeedFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcSmartObjectEatBehaviorProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FMassEntityHandle> EntitiesToSignal;
	EntityQuery.ForEachEntityChunk(Context,
		[World, &EntityManager, &EntitiesToSignal](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			TArrayView<FArcHungerNeedFragment> Hungers =
				ChunkContext.GetMutableFragmentView<FArcHungerNeedFragment>();
			const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcSmartObjectBehaviorFragment* Lifecycle =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectBehaviorFragment>(EntityIt);
				FArcSmartObjectEatBehaviorFragment* Eat =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectEatBehaviorFragment>(EntityIt);
				if (!Lifecycle || !Eat
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsCurrentInteraction(
						Bodies[EntityIt], Users[EntityIt], *Lifecycle, Eat->CommandSerial)
					|| Lifecycle->bEffectApplied || Lifecycle->bCompletionQueued
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsUseTimeComplete(*Lifecycle, DeltaTime))
				{
					continue;
				}

				Lifecycle->bEffectApplied = true;
				FArcBodyFragment& Body = Bodies[EntityIt];
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				EArcBodyCommandResult Result = EArcBodyCommandResult::Failed;
				if (Hungers.Num() > 0 && EntityManager.IsEntityValid(Body.Command.TargetEntity))
				{
					ArcWorldSim::FEatPortion Portion;
					if (ArcWorldSim::FindEatPortion(EntityManager, Body.Command.TargetEntity, Portion))
					{
						if (ArcWorldSim::ConsumeOnePortion(
							EntityManager, Body.Command.TargetEntity, Portion.ItemId, Portion.StackCount))
						{
							ArcWorldSim::ApplyEatNutrition(
								EntityManager,
								Entity,
								Hungers[EntityIt],
								Portion.Nutrition,
								World->GetTimeSeconds());
							Result = EArcBodyCommandResult::Succeeded;
						}
					}
				}
				ArcSmartObjectBehaviorProcessorsPrivate::QueueCompletion(
					Body,
					Users[EntityIt],
					*Lifecycle,
					Result,
					Entity,
					EntitiesToSignal);
			}
		});

	if (!EntitiesToSignal.IsEmpty())
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
		SignalSubsystem.SignalEntities(UE::Mass::Signals::SmartObjectInteractionDone, EntitiesToSignal);
	}
}

UArcSmartObjectSleepBehaviorProcessor::UArcSmartObjectSleepBehaviorProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteAfter.Add(UArcFatigueNeedProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcSmartObjectSleepBehaviorProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ArcSmartObjectBehaviorProcessorsPrivate::ConfigureTimedQuery(
		EntityQuery, EntityManager, FArcSmartObjectSleepBehaviorFragment::StaticStruct());
	EntityQuery.AddRequirement<FArcFatigueNeedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcNeedBandTableFragment>(EMassFragmentPresence::Optional);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcSmartObjectSleepBehaviorProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	TArray<FMassEntityHandle> EntitiesToSignal;
	EntityQuery.ForEachEntityChunk(Context,
		[Now, &EntitiesToSignal](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			TArrayView<FArcFatigueNeedFragment> Fatigues =
				ChunkContext.GetMutableFragmentView<FArcFatigueNeedFragment>();
			const FArcNeedBandTableFragment* BandTable =
				ChunkContext.GetConstSharedFragmentPtr<FArcNeedBandTableFragment>();
			const float SecondsPerGameHour = BandTable
				? BandTable->SecondsPerGameHour
				: ArcNeeds::DefaultSecondsPerGameHour;
			const TArray<FArcNeedBand>& FatigueBands = ArcNeeds::FatigueBandsFor(BandTable);

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcSmartObjectBehaviorFragment* Lifecycle =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectBehaviorFragment>(EntityIt);
				FArcSmartObjectSleepBehaviorFragment* Sleep =
					ChunkContext.GetMutableSparseElement<FArcSmartObjectSleepBehaviorFragment>(EntityIt);
				if (!Lifecycle || !Sleep || Lifecycle->bCompletionQueued
					|| !ArcSmartObjectBehaviorProcessorsPrivate::IsCurrentInteraction(
						Bodies[EntityIt], Users[EntityIt], *Lifecycle, Sleep->CommandSerial))
				{
					continue;
				}

				const double Elapsed = FMath::Max(0.0, Now - Sleep->LastRecoveryTimeSeconds);
				Sleep->LastRecoveryTimeSeconds = Now;
				FArcFatigueNeedFragment& Fatigue = Fatigues[EntityIt];
				const float Recovery = ArcNeeds::SleepRecoveryPerSecond(SecondsPerGameHour)
					* Sleep->Comfort * static_cast<float>(Elapsed);
				Fatigue.CurrentValue = FMath::Clamp(
					Fatigue.CurrentValue - Recovery, 0.0f, ArcNeeds::NeedMaxValue);
				Fatigue.LastUpdateTime = Now;

				const int32 NewBand = ArcNeeds::BandIndexFor(FatigueBands, Fatigue.CurrentValue);
				if (NewBand == INDEX_NONE)
				{
					continue;
				}
				Fatigue.LastBand = static_cast<uint8>(NewBand);
				if (NewBand != 0)
				{
					continue;
				}

				ArcSmartObjectBehaviorProcessorsPrivate::QueueCompletion(
					Bodies[EntityIt],
					Users[EntityIt],
					*Lifecycle,
					EArcBodyCommandResult::Succeeded,
					ChunkContext.GetEntity(EntityIt),
					EntitiesToSignal);
			}
		});

	if (!EntitiesToSignal.IsEmpty())
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
		SignalSubsystem.SignalEntities(UE::Mass::Signals::SmartObjectInteractionDone, EntitiesToSignal);
	}
}
