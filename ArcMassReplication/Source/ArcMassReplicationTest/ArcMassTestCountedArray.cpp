// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassTestCountedArray.h"
#include "Net/UnrealNetwork.h"

int32 FArcMassTestCountedArray::AddCount = 0;
int32 FArcMassTestCountedArray::ChangeCount = 0;

void FArcMassTestCountedArray::ResetCounters()
{
	AddCount = 0;
	ChangeCount = 0;
}

void FArcMassTestCountedArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	AddCount += AddedIndices.Num();
}

void FArcMassTestCountedArray::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	ChangeCount += ChangedIndices.Num();
}

AArcMassTestCountedProxy::AArcMassTestCountedProxy()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AArcMassTestCountedProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcMassTestCountedProxy, TrackedItems);
}
