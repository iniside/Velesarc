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
};
