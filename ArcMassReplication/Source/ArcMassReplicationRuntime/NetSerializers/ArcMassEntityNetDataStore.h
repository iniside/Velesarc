// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/NetTokenStore.h"

class UArcMassEntityReplicationSubsystem;

namespace ArcMassReplication
{

class FArcMassEntityNetDataStore final : public UE::Net::FNetTokenDataStore
{
	UE_NONCOPYABLE(FArcMassEntityNetDataStore);

public:
	explicit FArcMassEntityNetDataStore(UE::Net::FNetTokenStore& InTokenStore, UArcMassEntityReplicationSubsystem* InSubsystem)
		: FNetTokenDataStore(InTokenStore)
		, Subsystem(InSubsystem)
	{
	}

	UArcMassEntityReplicationSubsystem* GetSubsystem() const { return Subsystem; }

	static FName GetTokenStoreName()
	{
		static const FName Name(TEXT("ArcMassEntityNetDataStore"));
		return Name;
	}

protected:
	void WriteTokenData(UE::Net::FNetSerializationContext&, FNetTokenStoreKey) const override {}
	FNetTokenStoreKey ReadTokenData(UE::Net::FNetSerializationContext&, const UE::Net::FNetToken&) override { return {}; }
	void WriteTokenData(FArchive&, FNetTokenStoreKey, UPackageMap*) const override {}
	FNetTokenStoreKey ReadTokenData(FArchive&, const UE::Net::FNetToken&, UPackageMap*) override { return {}; }

private:
	UArcMassEntityReplicationSubsystem* Subsystem;
};

} // namespace ArcMassReplication
