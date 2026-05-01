// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Fragments/ArcAttributeChangedFragment.h"
#include "ArcMassEventBufferSubsystem.generated.h"

struct FArcBufferedAttributeChange
{
	FMassEntityHandle Entity;
	TArray<FArcAttributeChangeEntry> Changes;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FArcOnBufferedAttributeChanges, TConstArrayView<FArcBufferedAttributeChange> /*Changes*/);

UCLASS()
class ARCMASSABILITIES_API UArcMassEventBufferSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	FArcOnBufferedAttributeChanges OnAttributeChangesReady;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleAttributeChangedSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);
	void HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);

	TArray<FArcBufferedAttributeChange> AttributeChangeBuffer;
	bool bBufferDirty = false;
	FDelegateHandle SignalDelegateHandle;
	FDelegateHandle WorldTickDelegateHandle;
};
