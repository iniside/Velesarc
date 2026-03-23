// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "MassSignalProcessorBase.h"
#include "Components/PrimitiveComponent.h"

#include "ArcMassPhysicsLink.generated.h"

struct FChaosUserEntityAppend;

// ---------------------------------------------------------------------------
// Signal
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Signals
{
	inline const FName PhysicsLinkRequested = FName(TEXT("ArcMassPhysicsLinkRequested"));
}

// ---------------------------------------------------------------------------
// Fragment
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcMassPhysicsLinkEntry
{
	GENERATED_BODY()

	/** The primitive component owning the physics body.
	 *  For ISM: the UInstancedStaticMeshComponent. For actors: any UPrimitiveComponent. */
	TWeakObjectPtr<UPrimitiveComponent> Component;

	/** For ISM: index into InstanceBodies[]. INDEX_NONE for actor components (uses main body). */
	int32 BodyIndex = INDEX_NONE;
	FChaosUserEntityAppend* Append = nullptr;
};

USTRUCT()
struct ARCMASS_API FArcMassPhysicsLinkFragment : public FMassFragment
{
	GENERATED_BODY()

	TArray<FArcMassPhysicsLinkEntry> Entries;
};

template<>
struct TMassFragmentTraits<FArcMassPhysicsLinkFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Tag
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcMassPhysicsLinkTag : public FMassTag
{
	GENERATED_BODY()
};


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace ArcMassPhysicsLink
{
	/** Populate physics link entries from an actor's primitive components.
	 *  Adds one entry per primitive component that has a valid body instance. */
	ARCMASS_API void PopulateEntriesFromActor(FArcMassPhysicsLinkFragment& LinkFragment, AActor* Actor);
}

// ---------------------------------------------------------------------------
// Signal Processor
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassPhysicsLinkProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassPhysicsLinkProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context,
		FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Deinit Observer
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassPhysicsLinkDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassPhysicsLinkDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// Trait
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Physics Link", Category = "Mass"))
class ARCMASS_API UArcMassPhysicsLinkTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
