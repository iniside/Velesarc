// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityHandle.h"
#include "ArcMassTestEntityHandleActor.generated.h"

UCLASS(NotBlueprintable)
class AArcMassTestEntityHandleActor : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestEntityHandleActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FMassEntityHandle ReplicatedHandle;

	UPROPERTY()
	FMassEntityHandle HandleReceivedFromClient;

	UFUNCTION(Server, Reliable)
	void ServerSendHandle(FMassEntityHandle Handle);

	bool bServerReceivedHandle = false;
};
