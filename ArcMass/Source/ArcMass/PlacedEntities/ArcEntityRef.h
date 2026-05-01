// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcEntityRef.generated.h"

class AArcPlacedEntityPartitionActor;

/** A reference to another Mass entity identified by its FArcLinkableGuidFragment::LinkGuid. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcEntityRef
{
	GENERATED_BODY()

	/** The LinkGuid of the target entity. Invalid GUID means no reference. */
	UPROPERTY(EditAnywhere, Category = "Linking")
	FGuid TargetGuid;

	bool IsValid() const { return TargetGuid.IsValid(); }

	bool operator==(const FArcEntityRef& Other) const { return TargetGuid == Other.TargetGuid; }
	bool operator!=(const FArcEntityRef& Other) const { return TargetGuid != Other.TargetGuid; }

	friend uint32 GetTypeHash(const FArcEntityRef& Ref) { return GetTypeHash(Ref.TargetGuid); }

#if WITH_EDITORONLY_DATA
	/** The partition actor that owns the target entity instance. Set by the eyedropper picker. */
	UPROPERTY()
	TSoftObjectPtr<AArcPlacedEntityPartitionActor> SourcePartitionActor;

	/** Human-readable label: "ConfigName [InstanceIndex]". Set by the eyedropper picker. */
	UPROPERTY()
	FString DisplayLabel;
#endif
};
