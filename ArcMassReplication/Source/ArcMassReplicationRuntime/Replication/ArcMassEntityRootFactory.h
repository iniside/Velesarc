// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/Iris/ReplicationSystem/NetRootObjectFactory.h"
#include "ArcMassEntityRootFactory.generated.h"

UCLASS()
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityRootFactory : public UNetRootObjectFactory
{
	GENERATED_BODY()

public:
	static FName GetFactoryName();
	virtual TOptional<FWorldInfoData> GetWorldInfo(const FWorldInfoContext& Context) const override;

protected:
	virtual void FillRootObjectReplicationParams(
		const UE::Net::FRootObjectReplicationParamsContext& Context,
		UE::Net::FRootObjectReplicationParams& OutParams) override;

	virtual FInstantiateResult InstantiateReplicatedObjectFromHeader(
		const FInstantiateContext& Context,
		const UE::Net::FNetObjectCreationHeader* Header) override;

	virtual void DetachedFromReplication(
		const FDetachContext& Context,
		const TOptional<FSubObjectDetachContext>& SubObjectContext) override;
};
