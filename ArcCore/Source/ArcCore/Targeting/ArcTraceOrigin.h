// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Tasks/TargetingTask.h"
#include "ArcTraceOrigin.generated.h"

struct FArcTargetingSourceContext;

USTRUCT(BlueprintType, meta = (ExcludeBaseStruct))
struct ARCCORE_API FArcTraceOrigin
{
	GENERATED_BODY()
	virtual ~FArcTraceOrigin() = default;

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const;
};

USTRUCT(BlueprintType, DisplayName = "Eyes ViewPoint")
struct ARCCORE_API FArcTraceOrigin_EyesViewPoint : public FArcTraceOrigin
{
	GENERATED_BODY()
	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

USTRUCT(BlueprintType, DisplayName = "Previous Result")
struct ARCCORE_API FArcTraceOrigin_PreviousResult : public FArcTraceOrigin
{
	GENERATED_BODY()
	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

USTRUCT(BlueprintType, DisplayName = "Socket")
struct ARCCORE_API FArcTraceOrigin_Socket : public FArcTraceOrigin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FName SocketName;

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

USTRUCT(BlueprintType, DisplayName = "World Location")
struct ARCCORE_API FArcTraceOrigin_WorldLocation : public FArcTraceOrigin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FRotator Rotation = FRotator::ZeroRotator;

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

/** Origin from global targeting hit result on the ArcASC, looked up by gameplay tag. */
USTRUCT(BlueprintType, DisplayName = "Global Targeting")
struct ARCCORE_API FArcTraceOrigin_GlobalTargeting : public FArcTraceOrigin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FGameplayTag TargetingTag;

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

/** Origin from the location of the actor hit by a previous targeting result. */
USTRUCT(BlueprintType, DisplayName = "Hit Actor Location")
struct ARCCORE_API FArcTraceOrigin_HitActorLocation : public FArcTraceOrigin
{
	GENERATED_BODY()

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

/** Origin from the floor location of the actor hit by a previous targeting result.
 *  The hit actor must implement IArcFloorLocationInterface. */
USTRUCT(BlueprintType, DisplayName = "Hit Actor Floor Location")
struct ARCCORE_API FArcTraceOrigin_HitActorFloorLocation : public FArcTraceOrigin
{
	GENERATED_BODY()

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

/** Origin from the location of the actor hit by global targeting, looked up by gameplay tag. */
USTRUCT(BlueprintType, DisplayName = "Global Targeting Hit Actor Location")
struct ARCCORE_API FArcTraceOrigin_GlobalTargetingHitActorLocation : public FArcTraceOrigin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FGameplayTag TargetingTag;

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

/** Origin from the floor location of the actor hit by global targeting.
 *  The hit actor must implement IArcFloorLocationInterface. */
USTRUCT(BlueprintType, DisplayName = "Global Targeting Hit Actor Floor Location")
struct ARCCORE_API FArcTraceOrigin_GlobalTargetingHitActorFloorLocation : public FArcTraceOrigin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FGameplayTag TargetingTag;

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};

USTRUCT(BlueprintType, DisplayName = "Direction To Previous Result")
struct ARCCORE_API FArcTraceOrigin_DirectionToPreviousResult : public FArcTraceOrigin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Config", meta = (BaseStruct = "/Script/ArcCore.ArcTraceOrigin", ExcludeBaseStruct))
	FInstancedStruct OriginProvider;

	virtual bool Resolve(const FTargetingRequestHandle& Handle,
	                     FVector& OutOrigin, FVector& OutDirection,
	                     bool& bHasDirection) const override;
};
