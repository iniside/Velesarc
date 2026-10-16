// Copyright Lukasz Baran. All Rights Reserved.

#include "Input/ArcMassCustomInputAction_Crouch.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassCustomInputAction_Crouch)

namespace ArcMassCustomInputActionCrouch
{
	UCharacterMoverComponent* FindCharacterMover(APawn* Pawn)
	{
		if (Pawn == nullptr)
		{
			return nullptr;
		}
		return Pawn->FindComponentByClass<UCharacterMoverComponent>();
	}

	void ReleaseHold(APawn* Pawn, bool bToggle)
	{
		if (bToggle)
		{
			return;
		}
		UCharacterMoverComponent* CharacterMover = FindCharacterMover(Pawn);
		if (CharacterMover != nullptr)
		{
			CharacterMover->UnCrouch();
		}
	}
}

void FArcMassCustomInputAction_Crouch::OnStarted(APawn* Pawn, const FInputActionValue& /*Value*/, FMassEntityHandle /*Entity*/) const
{
	UCharacterMoverComponent* CharacterMover = ArcMassCustomInputActionCrouch::FindCharacterMover(Pawn);
	if (CharacterMover == nullptr)
	{
		return;
	}

	if (bToggle && CharacterMover->GetCrouchIntent())
	{
		CharacterMover->UnCrouch();
		return;
	}
	CharacterMover->Crouch();
}

void FArcMassCustomInputAction_Crouch::OnCanceled(APawn* Pawn, const FInputActionValue& /*Value*/, FMassEntityHandle /*Entity*/) const
{
	ArcMassCustomInputActionCrouch::ReleaseHold(Pawn, bToggle);
}

void FArcMassCustomInputAction_Crouch::OnCompleted(APawn* Pawn, const FInputActionValue& /*Value*/, FMassEntityHandle /*Entity*/) const
{
	ArcMassCustomInputActionCrouch::ReleaseHold(Pawn, bToggle);
}
