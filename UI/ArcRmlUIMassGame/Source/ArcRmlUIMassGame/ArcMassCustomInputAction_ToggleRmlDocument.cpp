// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassCustomInputAction_ToggleRmlDocument.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if ARC_WITH_RMLUI
#include "ArcRmlUILog.h"
#include "Host/ArcRmlViewportSubsystem.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassCustomInputAction_ToggleRmlDocument)

void FArcMassCustomInputAction_ToggleRmlDocument::OnStarted(APawn* Pawn
															, const FInputActionValue& /*Value*/
															, FMassEntityHandle /*Entity*/) const
{
#if ARC_WITH_RMLUI
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: Toggle RML Document '%ls' started", *Document.Path);
	if (Pawn == nullptr)
	{
		return;
	}

	APlayerController* PC = Pawn->GetController<APlayerController>();
	if (PC == nullptr)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PC->Player);
	if (LocalPlayer == nullptr)
	{
		return;
	}

	UArcRmlViewportSubsystem* Subsystem = LocalPlayer->GetSubsystem<UArcRmlViewportSubsystem>();
	if (Subsystem == nullptr)
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: Toggle RML Document '%ls': the local player has no RML viewport subsystem", *Document.Path);
		return;
	}

	if (Subsystem->IsDocumentOpen(Document))
	{
		Subsystem->CloseDocument(Document);
	}
	else
	{
		Subsystem->OpenDocument(Document, InputAction);
	}
#endif
}
