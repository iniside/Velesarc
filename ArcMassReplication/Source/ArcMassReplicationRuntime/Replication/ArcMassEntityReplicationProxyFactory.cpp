// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcMassEntityReplicationProxyFactory.h"

// TODO Phase 7: this factory targeted the deleted AArcMassEntityReplicationProxy
// actor. Per spec, the per-entity Iris path uses UArcMassEntityVessel instead;
// this factory will be removed (or rewired to vessels) when bridge integration
// lands. Kept now to preserve the UCLASS for any external references.

FName UArcMassEntityReplicationProxyFactory::GetFactoryName()
{
	// Legacy proxy factory — distinct name from the new vessel-based "ArcMassEntityReplication".
	static const FName Name(TEXT("ArcMassEntityReplicationProxy"));
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
