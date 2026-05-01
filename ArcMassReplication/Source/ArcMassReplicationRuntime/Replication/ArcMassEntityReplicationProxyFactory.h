// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/Iris/ReplicationSystem/NetRootObjectFactory.h"
#include "ArcMassEntityReplicationProxyFactory.generated.h"

UCLASS()
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityReplicationProxyFactory : public UNetRootObjectFactory
{
	GENERATED_BODY()

public:
	static FName GetFactoryName();
	virtual TOptional<FWorldInfoData> GetWorldInfo(const FWorldInfoContext& Context) const override;

protected:
	virtual void FillRootObjectReplicationParams(
		const UE::Net::FRootObjectReplicationParamsContext& Context,
		UE::Net::FRootObjectReplicationParams& OutParams) override;
};
