// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassTestEntityHandleActor.h"
#include "Net/UnrealNetwork.h"

AArcMassTestEntityHandleActor::AArcMassTestEntityHandleActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AArcMassTestEntityHandleActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcMassTestEntityHandleActor, ReplicatedHandle);
}

void AArcMassTestEntityHandleActor::ServerSendHandle_Implementation(FMassEntityHandle Handle)
{
	HandleReceivedFromClient = Handle;
	bServerReceivedHandle = true;
}
