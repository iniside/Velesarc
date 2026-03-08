// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "UObject/Interface.h"
#include "MassEntityHandle.h"
#include "ArcProjectileFragments.generated.h"

// ---------------------------------------------------------------------------
// Config — const shared fragment (per-archetype)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcProjectileConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Speed cap. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float MaxSpeed = 0.f;

	/** Multiplier on world gravity. 0 = linear (no gravity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float GravityScale = 0.f;

	/** Auto-update rotation each frame to match velocity direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bRotationFollowsVelocity = true;

	/** Enable bounce simulation on impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Bounce")
	bool bShouldBounce = false;

	/** Coefficient of restitution (1.0 = no loss, 0.0 = no bounce). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Bounce", meta = (EditCondition = "bShouldBounce", ClampMin = "0.0", ClampMax = "1.0"))
	float Bounciness = 0.6f;

	/** Surface friction applied on bounce (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Bounce", meta = (EditCondition = "bShouldBounce", ClampMin = "0.0", ClampMax = "1.0"))
	float Friction = 0.2f;

	/** Stop bouncing when speed drops below this threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Bounce", meta = (EditCondition = "bShouldBounce", ClampMin = "0.0"))
	float BounceVelocityStopThreshold = 5.f;

	/** Maximum distance before automatic destruction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float MaxDistance = 50000.f;

	/** Collision radius for sphere overlap check at destination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float CollisionRadius = 10.f;

	/** Enable homing toward a target entity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing")
	bool bIsHoming = false;

	/** Acceleration magnitude toward homing target (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bIsHoming"))
	float HomingAcceleration = 1000.f;

	/** Actor class to spawn for visualization. nullptr = no actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Visualization")
	TSubclassOf<AActor> ActorClass;
};

// ---------------------------------------------------------------------------
// Per-entity mutable state
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcProjectileFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Current velocity vector (direction * speed). Set at spawn time. */
	UPROPERTY(EditAnywhere)
	FVector Velocity = FVector::ForwardVector * 5000.f;

	/** Accumulated distance traveled so far. */
	UPROPERTY()
	float DistanceTraveled = 0.f;

	/** True once the projectile has hit something or exceeded MaxDistance. */
	UPROPERTY()
	bool bPendingDestroy = false;

	/** Who spawned this projectile. In ability context, points to the ability instance. */
	UPROPERTY()
	TWeakObjectPtr<UObject> Instigator;
};

// ---------------------------------------------------------------------------
// Homing fragment — optional, only on homing projectiles
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcProjectileHomingFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Entity to home toward. */
	UPROPERTY()
	FMassEntityHandle TargetEntity;
};

// ---------------------------------------------------------------------------
// Collision filter — per-entity ignore list
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcProjectileCollisionFilterFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Actors to skip in collision queries. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> IgnoredActors;
};

template<>
struct TMassFragmentTraits<FArcProjectileCollisionFilterFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Tag — archetype differentiator
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcProjectileTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Interface — collision/lifecycle callbacks on visualization actor
// ---------------------------------------------------------------------------

UINTERFACE(MinimalAPI, BlueprintType)
class UArcProjectileActorInterface : public UInterface
{
	GENERATED_BODY()
};

class ARCMASS_API IArcProjectileActorInterface
{
	GENERATED_BODY()

public:
	/** Called after actor spawn + entity link. */
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
	void OnProjectileEntityCreated(FMassEntityHandle EntityHandle);

	/** Called BEFORE entity destruction on collision. */
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
	void OnProjectileCollision(const FHitResult& HitResult);

	/** Called BEFORE entity destruction when max distance exceeded. */
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
	void OnProjectileExpired();
};
