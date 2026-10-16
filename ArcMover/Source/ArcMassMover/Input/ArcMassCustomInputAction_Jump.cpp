// Copyright Lukasz Baran. All Rights Reserved.

#include "Input/ArcMassCustomInputAction_Jump.h"

#include "GameFramework/Pawn.h"
#include "Pawn/ArcMassMoverInputProducerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassCustomInputAction_Jump)

namespace ArcMassCustomInputActionJump
{
	UArcMassMoverInputProducerComponent* FindProducer(APawn* Pawn)
	{
		if (Pawn == nullptr)
		{
			return nullptr;
		}
		return Pawn->FindComponentByClass<UArcMassMoverInputProducerComponent>();
	}

	void Release(APawn* Pawn)
	{
		UArcMassMoverInputProducerComponent* Producer = FindProducer(Pawn);
		if (Producer != nullptr)
		{
			Producer->ReleaseJump();
		}
	}
}

void FArcMassCustomInputAction_Jump::OnStarted(APawn* Pawn, const FInputActionValue& /*Value*/, FMassEntityHandle /*Entity*/) const
{
	UArcMassMoverInputProducerComponent* Producer = ArcMassCustomInputActionJump::FindProducer(Pawn);
	if (Producer != nullptr)
	{
		Producer->RequestJump();
	}
}

void FArcMassCustomInputAction_Jump::OnCanceled(APawn* Pawn, const FInputActionValue& /*Value*/, FMassEntityHandle /*Entity*/) const
{
	ArcMassCustomInputActionJump::Release(Pawn);
}

void FArcMassCustomInputAction_Jump::OnCompleted(APawn* Pawn, const FInputActionValue& /*Value*/, FMassEntityHandle /*Entity*/) const
{
	ArcMassCustomInputActionJump::Release(Pawn);
}
