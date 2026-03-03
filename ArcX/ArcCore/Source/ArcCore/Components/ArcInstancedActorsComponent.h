// Copyright Lukasz Baran. All Rights Reserved.

#pragma once
#include "InstancedActorsComponent.h"

#include "ArcInstancedActorsComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCCORE_API UArcInstancedActorsComponent : public UInstancedActorsComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(Category = "Traits", EditAnywhere, Instanced)
	TArray<TObjectPtr<UMassEntityTraitBase>> Traits;

public:
	// Sets default values for this component's properties
	UArcInstancedActorsComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	virtual void ModifyMassEntityConfig(FMassEntityManager& InMassEntityManager, UInstancedActorsData* InstancedActorData, FMassEntityConfig& InOutMassEntityConfig) const override;
	
	virtual void OnServerPreSpawnInitForInstance(FInstancedActorsInstanceHandle InInstanceHandle) override;

	virtual void InitializeComponentForInstance(FInstancedActorsInstanceHandle InInstanceHandle) override;

	const FInstancedActorsInstanceHandle& GetInstanceHandle() const
	{
		return InstanceHandle;
	}

	// ── Persistence (Flow 2) ──────────────────────────────────────────

	/** Fragment types to persist beyond engine defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Persistence",
		meta = (AllowAbstract = "false"))
	TArray<TObjectPtr<UScriptStruct>> PersistenceAllowedFragments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Persistence",
		meta = (AllowAbstract = "false"))
	TArray<TObjectPtr<UScriptStruct>> PersistenceDisallowedFragments;

	virtual uint32 GetInstancePersistenceDataID() const override;
	virtual bool ShouldSerializeInstancePersistenceData(const FArchive& Archive,
		UInstancedActorsData* InstanceData, int64 TimeDelta) const override;
	virtual void SerializeInstancePersistenceData(
		FStructuredArchive::FRecord Record, UInstancedActorsData* InstanceData,
		int64 TimeDelta) const override;
};
