// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcMassItemRecordSyncSubsystem.generated.h"

UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassItemRecordSyncSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void OnJsonRecordChanged(const FPrimaryAssetId& Id);

	FDelegateHandle RecordChangedHandle;
};
