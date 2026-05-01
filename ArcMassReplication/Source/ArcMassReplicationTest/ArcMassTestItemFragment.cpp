// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassTestItemFragment.h"
#include "Net/UnrealNetwork.h"

void FArcMassTestReplicatedItem::PreReplicatedRemove(FArcMassTestReplicatedItemArray& Array)
{
	++Array.RemoveCallbackCount;
	UE_LOG(LogTemp, Warning, TEXT("PreReplicatedRemove ItemId=%d RemoveCb=%d"), ItemId, Array.RemoveCallbackCount);
}

void FArcMassTestReplicatedItem::PostReplicatedAdd(FArcMassTestReplicatedItemArray& Array)
{
	++Array.AddCallbackCount;
	UE_LOG(LogTemp, Warning, TEXT("PostReplicatedAdd ItemId=%d AddCb=%d"), ItemId, Array.AddCallbackCount);
}

void FArcMassTestReplicatedItem::PostReplicatedChange(FArcMassTestReplicatedItemArray& Array)
{
	++Array.ChangeCallbackCount;
	UE_LOG(LogTemp, Warning, TEXT("PostReplicatedChange ItemId=%d ChangeCb=%d"), ItemId, Array.ChangeCallbackCount);
}

AArcMassTestDirectArrayActor::AArcMassTestDirectArrayActor()
{
	bReplicates = true;
}

void AArcMassTestDirectArrayActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcMassTestDirectArrayActor, ReplicatedItems);
}
