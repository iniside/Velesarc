// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassTestItemFragment.h"
#include "Net/UnrealNetwork.h"

void FArcMassTestReplicatedItem::PreReplicatedRemove(FArcMassTestReplicatedItemArray& Array)
{
	++Array.RemoveCallbackCount;
	UE_LOGF(LogTemp, Warning,"PreReplicatedRemove ItemId=%d RemoveCb=%d", ItemId, Array.RemoveCallbackCount);
}

void FArcMassTestReplicatedItem::PostReplicatedAdd(FArcMassTestReplicatedItemArray& Array)
{
	++Array.AddCallbackCount;
	UE_LOGF(LogTemp, Warning,"PostReplicatedAdd ItemId=%d AddCb=%d", ItemId, Array.AddCallbackCount);
}

void FArcMassTestReplicatedItem::PostReplicatedChange(FArcMassTestReplicatedItemArray& Array)
{
	++Array.ChangeCallbackCount;
	UE_LOGF(LogTemp, Warning,"PostReplicatedChange ItemId=%d ChangeCb=%d", ItemId, Array.ChangeCallbackCount);
}

AArcMassTestDirectMapActor::AArcMassTestDirectMapActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AArcMassTestDirectMapActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcMassTestDirectMapActor, IntStructMap);
	DOREPLIFETIME(AArcMassTestDirectMapActor, TagFloatMap);
	DOREPLIFETIME(AArcMassTestDirectMapActor, TagInstancedStructMap);
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
	UE_LOGF(LogTemp, Warning,"EngineFast PreReplicatedRemove ItemId=%d RemoveCb=%d", ItemId, Array.RemoveCallbackCount);
}

void FArcMassTestEngineFastItem::PostReplicatedAdd(const FArcMassTestEngineFastArray& Array)
{
	++const_cast<FArcMassTestEngineFastArray&>(Array).AddCallbackCount;
	UE_LOGF(LogTemp, Warning,"EngineFast PostReplicatedAdd ItemId=%d AddCb=%d", ItemId, Array.AddCallbackCount);
}

void FArcMassTestEngineFastItem::PostReplicatedChange(const FArcMassTestEngineFastArray& Array)
{
	++const_cast<FArcMassTestEngineFastArray&>(Array).ChangeCallbackCount;
	UE_LOGF(LogTemp, Warning,"EngineFast PostReplicatedChange ItemId=%d ChangeCb=%d", ItemId, Array.ChangeCallbackCount);
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
