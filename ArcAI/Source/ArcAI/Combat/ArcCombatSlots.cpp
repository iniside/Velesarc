// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcCombatSlots.h"

#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "StructUtils/StructView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCombatSlots)

namespace ArcCombatSlotsPrivate
{
	FVector MakeSlotPoint(const FVector& Centre, const FArcCombatSlotClaim& Claim)
	{
		const double YawRadians = FMath::DegreesToRadians(static_cast<double>(Claim.YawDegrees));
		return Centre + FVector(FMath::Cos(YawRadians), FMath::Sin(YawRadians), 0.0) * Claim.Radius;
	}

	bool IsClaimLive(
		const FMassEntityManager& EntityManager,
		const FArcCombatSlotClaim& Claim,
		const FVector& Centre,
		const double Now)
	{
		if (!EntityManager.IsEntityActive(Claim.Claimer))
		{
			return false;
		}
		if (Now - Claim.LastRenewTime <= ArcCombatSlots::LeaseSeconds)
		{
			return true;
		}
		const FTransformFragment* ClaimerTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Claim.Claimer);
		if (!ClaimerTransform)
		{
			return false;
		}
		const double HoldDistance = FMath::Max(0.0, static_cast<double>(Claim.HoldDistance));
		return FVector::DistSquared2D(ClaimerTransform->GetTransform().GetLocation(), MakeSlotPoint(Centre, Claim))
			<= HoldDistance * HoldDistance;
	}

	bool IsWithinSeparation(const float YawA, const float YawB, const float MinSeparationDegrees)
	{
		return FMath::Abs(FMath::FindDeltaAngleDegrees(YawA, YawB)) < MinSeparationDegrees;
	}

	const FArcCombatSlotsFragment* FindSlots(const FMassEntityManager& EntityManager, const FMassEntityHandle Target)
	{
		if (!EntityManager.IsEntityActive(Target))
		{
			return nullptr;
		}
		const FConstStructView View = EntityManager.GetSparseElementDataForEntity(FArcCombatSlotsFragment::StaticStruct(), Target);
		return View.GetPtr<FArcCombatSlotsFragment>();
	}

	FArcCombatSlotsFragment* FindMutableSlots(FMassEntityManager& EntityManager, const FMassEntityHandle Target)
	{
		const FStructView View = EntityManager.GetMutableSparseElementDataForEntity(FArcCombatSlotsFragment::StaticStruct(), Target);
		return View.GetPtr<FArcCombatSlotsFragment>();
	}

	int32 FindClaimIndex(const FArcCombatSlotsFragment& Slots, const FMassEntityHandle Claimer)
	{
		return Slots.Claims.IndexOfByPredicate([Claimer](const FArcCombatSlotClaim& Claim) -> bool
		{
			return Claim.Claimer == Claimer;
		});
	}
}

namespace ArcCombatSlots
{
	bool GetTargetCentre(const FMassEntityManager& EntityManager, const FMassEntityHandle Target, FVector& OutCentre)
	{
		if (!EntityManager.IsEntityActive(Target))
		{
			return false;
		}
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Target);
		if (!Transform)
		{
			return false;
		}
		OutCentre = Transform->GetTransform().GetLocation();
		return true;
	}

	bool GetLiveSlotPoint(
		const FMassEntityManager& EntityManager,
		const FMassEntityHandle Claimer,
		const FMassEntityHandle Target,
		const double Now,
		FVector& OutSlotPoint)
	{
		const FArcCombatSlotsFragment* Slots = ArcCombatSlotsPrivate::FindSlots(EntityManager, Target);
		FVector Centre = FVector::ZeroVector;
		if (!Slots || !GetTargetCentre(EntityManager, Target, Centre))
		{
			return false;
		}
		const int32 ClaimIndex = ArcCombatSlotsPrivate::FindClaimIndex(*Slots, Claimer);
		if (ClaimIndex == INDEX_NONE)
		{
			return false;
		}
		const FArcCombatSlotClaim& Claim = Slots->Claims[ClaimIndex];
		if (!ArcCombatSlotsPrivate::IsClaimLive(EntityManager, Claim, Centre, Now))
		{
			return false;
		}
		OutSlotPoint = ArcCombatSlotsPrivate::MakeSlotPoint(Centre, Claim);
		return true;
	}

	int32 CountLiveClaimers(
		const FMassEntityManager& EntityManager,
		const FMassEntityHandle Target,
		const FMassEntityHandle Excluding,
		const double Now)
	{
		const FArcCombatSlotsFragment* Slots = ArcCombatSlotsPrivate::FindSlots(EntityManager, Target);
		FVector Centre = FVector::ZeroVector;
		if (!Slots || !GetTargetCentre(EntityManager, Target, Centre))
		{
			return 0;
		}
		int32 Count = 0;
		for (const FArcCombatSlotClaim& Claim : Slots->Claims)
		{
			if (Claim.Claimer != Excluding && ArcCombatSlotsPrivate::IsClaimLive(EntityManager, Claim, Centre, Now))
			{
				++Count;
			}
		}
		return Count;
	}

	bool IsAngleFree(
		const FMassEntityManager& EntityManager,
		const FMassEntityHandle Target,
		const float YawDegrees,
		const float MinSeparationDegrees,
		const FMassEntityHandle Excluding,
		const double Now)
	{
		const FArcCombatSlotsFragment* Slots = ArcCombatSlotsPrivate::FindSlots(EntityManager, Target);
		FVector Centre = FVector::ZeroVector;
		if (!Slots || !GetTargetCentre(EntityManager, Target, Centre))
		{
			return true;
		}
		for (const FArcCombatSlotClaim& Claim : Slots->Claims)
		{
			if (Claim.Claimer != Excluding
				&& ArcCombatSlotsPrivate::IsWithinSeparation(Claim.YawDegrees, YawDegrees, MinSeparationDegrees)
				&& ArcCombatSlotsPrivate::IsClaimLive(EntityManager, Claim, Centre, Now))
			{
				return false;
			}
		}
		return true;
	}

	void PushClaim(
		FMassEntityManager& EntityManager,
		const FMassEntityHandle Target,
		const FMassEntityHandle Claimer,
		const float YawDegrees,
		const float Radius,
		const float HoldDistance,
		const float MinSeparationDegrees,
		const int32 MaxClaimers,
		const double Now)
	{
		FArcCombatSlotClaim NewClaim;
		NewClaim.Claimer = Claimer;
		NewClaim.YawDegrees = static_cast<float>(FRotator::NormalizeAxis(YawDegrees));
		NewClaim.Radius = Radius;
		NewClaim.HoldDistance = HoldDistance;
		NewClaim.LastRenewTime = Now;
		EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
			[Target, NewClaim, MinSeparationDegrees, MaxClaimers, Now](FMassEntityManager& Mgr)
			{
				if (!Mgr.IsEntityActive(Target) || !Mgr.IsEntityActive(NewClaim.Claimer))
				{
					return;
				}
				FVector Centre = FVector::ZeroVector;
				if (!GetTargetCentre(Mgr, Target, Centre))
				{
					return;
				}
				FArcCombatSlotsFragment* Slots = ArcCombatSlotsPrivate::FindMutableSlots(Mgr, Target);
				if (Slots)
				{
					Slots->Claims.RemoveAll([&Mgr, &Centre, Now](const FArcCombatSlotClaim& Claim) -> bool
					{
						return !ArcCombatSlotsPrivate::IsClaimLive(Mgr, Claim, Centre, Now);
					});
					int32 OtherClaims = 0;
					for (const FArcCombatSlotClaim& Claim : Slots->Claims)
					{
						if (Claim.Claimer == NewClaim.Claimer)
						{
							continue;
						}
						if (ArcCombatSlotsPrivate::IsWithinSeparation(Claim.YawDegrees, NewClaim.YawDegrees, MinSeparationDegrees))
						{
							return;
						}
						++OtherClaims;
					}
					if (OtherClaims >= MaxClaimers)
					{
						return;
					}
				}
				else
				{
					if (MaxClaimers <= 0)
					{
						return;
					}
					Slots = &Mgr.AddSparseElementToEntity<FArcCombatSlotsFragment>(Target);
				}
				const int32 ClaimIndex = ArcCombatSlotsPrivate::FindClaimIndex(*Slots, NewClaim.Claimer);
				if (ClaimIndex == INDEX_NONE)
				{
					Slots->Claims.Add(NewClaim);
				}
				else
				{
					Slots->Claims[ClaimIndex] = NewClaim;
				}
			});
	}

	void PushRenew(FMassEntityManager& EntityManager, const FMassEntityHandle Target, const FMassEntityHandle Claimer, const double Now)
	{
		EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
			[Target, Claimer, Now](FMassEntityManager& Mgr)
			{
				if (!Mgr.IsEntityActive(Target))
				{
					return;
				}
				FArcCombatSlotsFragment* Slots = ArcCombatSlotsPrivate::FindMutableSlots(Mgr, Target);
				if (!Slots)
				{
					return;
				}
				const int32 ClaimIndex = ArcCombatSlotsPrivate::FindClaimIndex(*Slots, Claimer);
				if (ClaimIndex != INDEX_NONE)
				{
					Slots->Claims[ClaimIndex].LastRenewTime = Now;
				}
			});
	}

	void PushRelease(FMassEntityManager& EntityManager, const FMassEntityHandle Target, const FMassEntityHandle Claimer)
	{
		EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
			[Target, Claimer](FMassEntityManager& Mgr)
			{
				if (!Mgr.IsEntityActive(Target))
				{
					return;
				}
				FArcCombatSlotsFragment* Slots = ArcCombatSlotsPrivate::FindMutableSlots(Mgr, Target);
				if (!Slots)
				{
					return;
				}
				const int32 ClaimIndex = ArcCombatSlotsPrivate::FindClaimIndex(*Slots, Claimer);
				if (ClaimIndex != INDEX_NONE)
				{
					Slots->Claims.RemoveAtSwap(ClaimIndex);
				}
			});
	}
}
