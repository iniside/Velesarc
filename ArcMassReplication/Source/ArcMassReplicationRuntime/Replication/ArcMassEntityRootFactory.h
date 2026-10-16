// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Iris/Core/NetObjectReference.h"
#include "Net/Iris/ReplicationSystem/NetRootObjectFactory.h"
#include "ArcMassEntityRootFactory.generated.h"

/**
 * Wire-format version baked into every creation header. Bump when the header's
 * on-wire layout changes in a way an older receiver would mis-deserialize.
 */
inline constexpr uint32 ArcMassNetProtocolVersion = 1;

/**
 * Creation header for vessel replication. Carries a resolvable reference to the
 * vessel's UMassEntityConfigAsset (stably-named on-disk asset) so the client can
 * load/find the same config and spawn a matching local entity — no protocol-id
 * warmup registration needed on either side.
 */
class FArcMassNetCreationHeader : public UE::Net::FNetObjectCreationHeader
{
public:
	/** Wire-format protocol version (see ArcMassNetProtocolVersion). */
	uint32 ProtocolVersion = ArcMassNetProtocolVersion;

	/** The UMassEntityConfigAsset the vessel was bound to on the server. */
	UE::Net::FNetObjectReference ConfigReference;

	virtual FString ToString() const override;
};

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

	virtual TUniquePtr<UE::Net::FNetObjectCreationHeader> CreateAndFillHeader(
		UE::Net::FNetRefHandle Handle) override;

	virtual bool SerializeHeader(
		const UE::Net::FCreationHeaderContext& Context,
		const UE::Net::FNetObjectCreationHeader* Header) override;

	virtual TUniquePtr<UE::Net::FNetObjectCreationHeader> CreateAndDeserializeHeader(
		const UE::Net::FCreationHeaderContext& Context) override;

	virtual void DetachedFromReplication(
		const FDetachContext& Context,
		const TOptional<FSubObjectDetachContext>& SubObjectContext) override;
};
