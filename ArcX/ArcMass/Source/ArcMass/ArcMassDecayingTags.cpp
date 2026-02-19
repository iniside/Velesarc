// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassDecayingTags.h"

#include "ArcMassInfluenceMapping.h"
#include "DrawDebugHelpers.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassDecayingTags)

#if WITH_GAMEPLAY_DEBUGGER
TAutoConsoleVariable<bool> CVarArcDebugDrawDecayingTags(
	TEXT("arc.mass.DebugDrawDecayingTags"),
	false,
	TEXT("Toggles debug drawing for decaying gameplay tags on entities (0 = off, 1 = on)"));
#endif

// ---------------------------------------------------------------------------
// FArcMassDecayingTagFragment
// ---------------------------------------------------------------------------

FArcMassDecayingTagEntry* FArcMassDecayingTagFragment::FindEntry(const FGameplayTag& Tag)
{
	for (FArcMassDecayingTagEntry& Entry : Entries)
	{
		if (Entry.Tag == Tag)
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FArcMassDecayingTagEntry* FArcMassDecayingTagFragment::FindEntry(const FGameplayTag& Tag) const
{
	for (const FArcMassDecayingTagEntry& Entry : Entries)
	{
		if (Entry.Tag == Tag)
		{
			return &Entry;
		}
	}
	return nullptr;
}

bool FArcMassDecayingTagFragment::HasTag(const FGameplayTag& Tag) const
{
	return FindEntry(Tag) != nullptr;
}

float FArcMassDecayingTagFragment::GetRemainingTime(const FGameplayTag& Tag) const
{
	if (const FArcMassDecayingTagEntry* Entry = FindEntry(Tag))
	{
		return Entry->RemainingTime;
	}
	return 0.f;
}

void FArcMassDecayingTagFragment::AddOrResetTag(const FGameplayTag& Tag, float Duration)
{
	if (FArcMassDecayingTagEntry* Existing = FindEntry(Tag))
	{
		Existing->RemainingTime = Duration;
		Existing->InitialDuration = Duration;
	}
	else
	{
		Entries.Add({ Tag, Duration, Duration });
	}
}

int32 FArcMassDecayingTagFragment::RemoveExpiredEntries()
{
	int32 Removed = 0;
	for (int32 i = Entries.Num() - 1; i >= 0; --i)
	{
		if (Entries[i].RemainingTime <= 0.f)
		{
			Entries.RemoveAtSwap(i);
			++Removed;
		}
	}
	return Removed;
}

// ---------------------------------------------------------------------------
// UArcMassDecayingTagSubsystem
// ---------------------------------------------------------------------------

void UArcMassDecayingTagSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcMassDecayingTagSubsystem::Deinitialize()
{
	PendingRequests.Empty();
	Super::Deinitialize();
}

void UArcMassDecayingTagSubsystem::RequestAddDecayingTag(const FMassEntityHandle& Entity, FGameplayTag Tag, float Duration)
{
	RequestAddDecayingTag(Entity, Tag, Duration, INDEX_NONE, INDEX_NONE, 0.f);
}

void UArcMassDecayingTagSubsystem::RequestAddDecayingTag(const FMassEntityHandle& Entity, FGameplayTag Tag, float Duration,
	int32 InfluenceGridIndex, int32 InfluenceChannel, float InfluenceStrength)
{
	if (!Entity.IsValid() || !Tag.IsValid() || Duration <= 0.f)
	{
		return;
	}

	PendingRequests.Add({ Entity, Tag, Duration, InfluenceGridIndex, InfluenceChannel, InfluenceStrength });

	UWorld* World = GetWorld();
	if (UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr)
	{
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::DecayingTagAdded, Entity);
	}
}

bool UArcMassDecayingTagSubsystem::EntityHasDecayingTag(const UObject* WorldContextObject, const FMassEntityHandle& Entity, FGameplayTag Tag)
{
	if (!WorldContextObject || !Entity.IsValid() || !Tag.IsValid())
	{
		return false;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return false;
	}

	const FArcMassDecayingTagFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcMassDecayingTagFragment>(Entity);
	return Fragment && Fragment->HasTag(Tag);
}

float UArcMassDecayingTagSubsystem::GetDecayingTagRemainingTime(const UObject* WorldContextObject, const FMassEntityHandle& Entity, FGameplayTag Tag)
{
	if (!WorldContextObject || !Entity.IsValid() || !Tag.IsValid())
	{
		return 0.f;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return 0.f;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return 0.f;
	}

	const FArcMassDecayingTagFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcMassDecayingTagFragment>(Entity);
	return Fragment ? Fragment->GetRemainingTime(Tag) : 0.f;
}

TArray<FGameplayTag> UArcMassDecayingTagSubsystem::GetAllDecayingTags(const UObject* WorldContextObject, const FMassEntityHandle& Entity)
{
	TArray<FGameplayTag> Result;

	if (!WorldContextObject || !Entity.IsValid())
	{
		return Result;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return Result;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return Result;
	}

	const FArcMassDecayingTagFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcMassDecayingTagFragment>(Entity);
	if (Fragment)
	{
		Result.Reserve(Fragment->Entries.Num());
		for (const FArcMassDecayingTagEntry& Entry : Fragment->Entries)
		{
			Result.Add(Entry.Tag);
		}
	}

	return Result;
}

// ---------------------------------------------------------------------------
// UArcMassDecayingTagSignalProcessor
// ---------------------------------------------------------------------------

UArcMassDecayingTagSignalProcessor::UArcMassDecayingTagSignalProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcMassDecayingTagSignalProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::DecayingTagAdded);
}

void UArcMassDecayingTagSignalProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassDecayingTagFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcMassDecayingTagSignalProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	UArcMassDecayingTagSubsystem* Subsystem = World ? World->GetSubsystem<UArcMassDecayingTagSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	// Drain pending requests
	TArray<FArcMassDecayingTagRequest> Requests = MoveTemp(Subsystem->GetPendingRequests());
	Subsystem->GetPendingRequests().Reset();

	if (Requests.IsEmpty())
	{
		return;
	}

	// Build lookup: Entity -> indices into Requests array
	TMap<FMassEntityHandle, TArray<int32>> RequestsByEntity;
	for (int32 i = 0; i < Requests.Num(); ++i)
	{
		RequestsByEntity.FindOrAdd(Requests[i].Entity).Add(i);
	}

	// Resolve influence subsystem once (may be null)
	UArcInfluenceMappingSubsystem* InfluenceSubsystem = World->GetSubsystem<UArcInfluenceMappingSubsystem>();

	EntityQuery.ForEachEntityChunk(Context,
		[&Requests, &RequestsByEntity, InfluenceSubsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMassDecayingTagFragment> DecayFragments =
				Ctx.GetMutableFragmentView<FArcMassDecayingTagFragment>();
			const TConstArrayView<FTransformFragment> Transforms =
				Ctx.GetFragmentView<FTransformFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const TArray<int32>* Indices = RequestsByEntity.Find(Entity);
				if (!Indices)
				{
					continue;
				}

				FArcMassDecayingTagFragment& Fragment = DecayFragments[EntityIt];

				for (const int32 ReqIdx : *Indices)
				{
					const FArcMassDecayingTagRequest& Req = Requests[ReqIdx];
					Fragment.AddOrResetTag(Req.Tag, Req.Duration);

					// Bridge to influence grid if requested
					if (Req.InfluenceGridIndex != INDEX_NONE && InfluenceSubsystem)
					{
						const FVector EntityPos = Transforms[EntityIt].GetTransform().GetLocation();
						InfluenceSubsystem->AddInfluence(Req.InfluenceGridIndex, EntityPos,
							Req.InfluenceChannel, Req.InfluenceStrength, Entity);
					}
				}
			}
		});
}

// ---------------------------------------------------------------------------
// UArcMassDecayingTagDecayProcessor
// ---------------------------------------------------------------------------

UArcMassDecayingTagDecayProcessor::UArcMassDecayingTagDecayProcessor()
	: EntityQuery{*this}
#if WITH_GAMEPLAY_DEBUGGER
	, DebugQuery{*this}
#endif
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
}

void UArcMassDecayingTagDecayProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassDecayingTagFragment>(EMassFragmentAccess::ReadWrite);

#if WITH_GAMEPLAY_DEBUGGER
	DebugQuery.AddRequirement<FArcMassDecayingTagFragment>(EMassFragmentAccess::ReadOnly);
	DebugQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
#endif
}

void UArcMassDecayingTagDecayProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	if (DeltaTime <= 0.f)
	{
		return;
	}

	// Phase 1: Tick decay on all entities with the fragment
	EntityQuery.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcMassDecayingTagFragment> DecayFragments =
			Ctx.GetMutableFragmentView<FArcMassDecayingTagFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcMassDecayingTagFragment& Fragment = DecayFragments[EntityIt];

			if (Fragment.Entries.IsEmpty())
			{
				continue;
			}

			for (FArcMassDecayingTagEntry& Entry : Fragment.Entries)
			{
				Entry.RemainingTime -= DeltaTime;
			}

			Fragment.RemoveExpiredEntries();
		}
	});

	// Phase 2: Debug draw (requires transform for world position)
#if WITH_GAMEPLAY_DEBUGGER
	if (CVarArcDebugDrawDecayingTags.GetValueOnAnyThread())
	{
		UWorld* World = EntityManager.GetWorld();
		if (!World)
		{
			return;
		}

		DebugQuery.ForEachEntityChunk(Context, [World](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FArcMassDecayingTagFragment> DecayFragments =
				Ctx.GetFragmentView<FArcMassDecayingTagFragment>();
			const TConstArrayView<FTransformFragment> Transforms =
				Ctx.GetFragmentView<FTransformFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMassDecayingTagFragment& Fragment = DecayFragments[EntityIt];
				if (Fragment.Entries.IsEmpty())
				{
					continue;
				}

				const FVector EntityPos = Transforms[EntityIt].GetTransform().GetLocation();

				// Find highest normalized decay ratio for sphere color
				float MaxRatio = 0.f;
				for (const FArcMassDecayingTagEntry& Entry : Fragment.Entries)
				{
					if (Entry.InitialDuration > 0.f)
					{
						MaxRatio = FMath::Max(MaxRatio, Entry.RemainingTime / Entry.InitialDuration);
					}
				}

				const FColor SphereColor = FColor::MakeRedToGreenColorFromScalar(MaxRatio);
				DrawDebugSphere(World, EntityPos, 30.f, 8, SphereColor, false, -1.f, 0, 1.f);

				// Draw tag list above entity
				FVector TextPos = EntityPos + FVector(0.f, 0.f, 80.f);
				for (const FArcMassDecayingTagEntry& Entry : Fragment.Entries)
				{
					const FString TagText = FString::Printf(TEXT("[%s] %.1fs"),
						*Entry.Tag.ToString(), Entry.RemainingTime);
					DrawDebugString(World, TextPos, TagText, nullptr, FColor::White, -1.f, true, 0.8f);
					TextPos.Z += 20.f;
				}
			}
		});
	}
#endif
}

// ---------------------------------------------------------------------------
// UArcMassDecayingTagTrait
// ---------------------------------------------------------------------------

void UArcMassDecayingTagTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcMassDecayingTagFragment>();
}
