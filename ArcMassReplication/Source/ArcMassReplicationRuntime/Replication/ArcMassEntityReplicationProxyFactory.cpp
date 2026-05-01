// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcMassEntityReplicationProxyFactory.h"
#include "Replication/ArcMassEntityReplicationProxy.h"

FName UArcMassEntityReplicationProxyFactory::GetFactoryName()
{
	static const FName Name(TEXT("ArcMassEntityReplication"));
	return Name;
}

TOptional<UArcMassEntityReplicationProxyFactory::FWorldInfoData> UArcMassEntityReplicationProxyFactory::GetWorldInfo(
	const FWorldInfoContext& Context) const
{
	return NullOpt;
}

void UArcMassEntityReplicationProxyFactory::FillRootObjectReplicationParams(
	const UE::Net::FRootObjectReplicationParamsContext& Context,
	UE::Net::FRootObjectReplicationParams& OutParams)
{
	OutParams.bNeedsWorldLocationUpdate = false;
	OutParams.bUseClassConfigDynamicFilter = false;
}
