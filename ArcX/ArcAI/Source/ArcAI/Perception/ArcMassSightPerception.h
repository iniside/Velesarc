// Copyright Lukasz Baran. All Rights Reserved.

#pragma once
#include "ArcMassPerception.h"
#include "MassEntityFragments.h"
#include "MassEntityTraitBase.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"

#include "ArcMassSightPerception.generated.h"

class UArcMassSpatialHashSubsystem;
class UMassEntitySubsystem;

USTRUCT(BlueprintType)
struct ARCAI_API FArcPerceptionSightSenseConfigFragment : public FArcPerceptionSenseConfigFragment
{
	GENERATED_BODY()
};
template<>
struct TMassFragmentTraits<FArcPerceptionSightSenseConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// Sight perception result
USTRUCT()
struct ARCAI_API FArcMassSightPerceptionResult : public FArcMassPerceptionResultFragmentBase
{
	GENERATED_BODY()
};

template<>
struct TMassFragmentTraits<FArcMassSightPerceptionResult> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};


UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class ARCAI_API UArcPerceptionSightPerceiverTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FArcPerceptionSightSenseConfigFragment SightConfig;
	
	/** Appends items into the entity template required for the trait. */
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Perception Sight Perceivable"))
class ARCAI_API UArcPerceptionSightPerceivableTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

USTRUCT()
struct ARCAI_API FArcMassSightPerceivableTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * Subsystem managing perception events and queries
 */
UCLASS()
class ARCAI_API UArcMassSightPerceptionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	TMap<FMassEntityHandle, FArcPerceptionEntityAddedNative> OnEntityPerceived;
	TMap<FMassEntityHandle, FArcPerceptionEntityAddedNative> OnEntityLostFromPerception;
	
	TMap<FMassEntityHandle, FArcPerceptionEntityList> OnPerceptionUpdated;
	
	// Internal - called by processor to broadcast events
	void BroadcastEntityPerceived(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag);
	void BroadcastEntityLostFromPerception(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag);

	void CleanupEntity(FMassEntityHandle Entity)
	{
		OnEntityPerceived.Remove(Entity);
		OnEntityLostFromPerception.Remove(Entity);
		OnPerceptionUpdated.Remove(Entity);
	}

private:
	TWeakObjectPtr<UMassEntitySubsystem> CachedEntitySubsystem;
	UMassEntitySubsystem* GetEntitySubsystem() const;
};

//----------------------------------------------------------------------
// Sight Processor
//----------------------------------------------------------------------
UCLASS()
class ARCAI_API UArcMassSightPerceptionProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassSightPerceptionProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	void ProcessPerceptionChunk(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		UArcMassSpatialHashSubsystem* SpatialHash,
		UArcMassSightPerceptionSubsystem* PerceptionSubsystem,
		float DeltaTime,
		const TConstArrayView<FTransformFragment>& TransformList,
		const FArcPerceptionSightSenseConfigFragment& Config,
		TArrayView<FArcMassSightPerceptionResult> ResultList);

protected:
	FMassEntityQuery PerceptionQuery;
};

UCLASS()
class ARCAI_API UArcMassSightPerceptionObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassSightPerceptionObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};