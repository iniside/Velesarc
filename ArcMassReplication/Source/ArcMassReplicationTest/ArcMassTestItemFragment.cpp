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

AArcMassTestWrappedArrayActor::AArcMassTestWrappedArrayActor()
{
	bReplicates = true;
}

void AArcMassTestWrappedArrayActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcMassTestWrappedArrayActor, Wrapper);
}

AArcMassTestInstancedStructWrappedArrayActor::AArcMassTestInstancedStructWrappedArrayActor()
{
	bReplicates = true;
}

void AArcMassTestInstancedStructWrappedArrayActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcMassTestInstancedStructWrappedArrayActor, Slots);
}

void FArcMassTestEngineFastItem::PreReplicatedRemove(const FArcMassTestEngineFastArray& Array)
{
	++const_cast<FArcMassTestEngineFastArray&>(Array).RemoveCallbackCount;
	UE_LOG(LogTemp, Warning, TEXT("EngineFast PreReplicatedRemove ItemId=%d RemoveCb=%d"), ItemId, Array.RemoveCallbackCount);
}

void FArcMassTestEngineFastItem::PostReplicatedAdd(const FArcMassTestEngineFastArray& Array)
{
	++const_cast<FArcMassTestEngineFastArray&>(Array).AddCallbackCount;
	UE_LOG(LogTemp, Warning, TEXT("EngineFast PostReplicatedAdd ItemId=%d AddCb=%d"), ItemId, Array.AddCallbackCount);
}

void FArcMassTestEngineFastItem::PostReplicatedChange(const FArcMassTestEngineFastArray& Array)
{
	++const_cast<FArcMassTestEngineFastArray&>(Array).ChangeCallbackCount;
	UE_LOG(LogTemp, Warning, TEXT("EngineFast PostReplicatedChange ItemId=%d ChangeCb=%d"), ItemId, Array.ChangeCallbackCount);
}

AArcMassTestEngineFastArrayDirectActor::AArcMassTestEngineFastArrayDirectActor()
{
	bReplicates = true;
}

void AArcMassTestEngineFastArrayDirectActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcMassTestEngineFastArrayDirectActor, ReplicatedItems);
}

AArcMassTestEngineFastArrayWrappedActor::AArcMassTestEngineFastArrayWrappedActor()
{
	bReplicates = true;
}

void AArcMassTestEngineFastArrayWrappedActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcMassTestEngineFastArrayWrappedActor, Wrapper);
}
