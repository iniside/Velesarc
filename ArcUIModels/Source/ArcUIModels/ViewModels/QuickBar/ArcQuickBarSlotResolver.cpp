// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/QuickBar/ArcQuickBarSlotResolver.h"

#include "ViewModels/QuickBar/ArcQuickBarSlotWidgetInterface.h"
#include "ViewModels/QuickBar/ArcQuickBarViewModel.h"
#include "ViewModels/QuickBar/ArcQuickBarSlotViewModel.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerState.h"
#include "MVVMViewModelBase.h"

UArcQuickBarSlotViewModel* UArcQuickBarSlotResolver::ResolveSlotViewModel(
	UArcFrontendMVVMModelComponent& Component, FGameplayTag InBarId, FGameplayTag InQuickSlotId)
{
	UMVVMViewModelBase* ViewModel = nullptr;
	Component.FindViewModel(InBarId, ViewModel);

	if (!ViewModel)
	{
		return nullptr;
	}

	UArcQuickBarViewModel* BarViewModel = Cast<UArcQuickBarViewModel>(ViewModel);
	if (!BarViewModel)
	{
		UE_LOGF(LogTemp, Warning,"ArcQuickBar slot resolver: VM '%ls' for bar '%ls' is not a UArcQuickBarViewModel",
			*ViewModel->GetClass()->GetName(), *InBarId.ToString());
		return nullptr;
	}

	return BarViewModel->FindSlot(InQuickSlotId);
}

UObject* UArcQuickBarSlotResolver::CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* /*View*/) const
{
	if (!UserWidget)
	{
		return nullptr;
	}

	// Per-tag fallback: interface value when the widget implements the interface
	// AND reports a valid tag, else the authored UPROPERTY.
	FGameplayTag FinalBarId = BarId;
	FGameplayTag FinalQuickSlotId = QuickSlotId;

	if (UserWidget->Implements<UArcQuickBarSlotWidget>())
	{
		const FGameplayTag WidgetBarId = IArcQuickBarSlotWidget::Execute_GetQuickBarId(const_cast<UUserWidget*>(UserWidget));
		if (WidgetBarId.IsValid())
		{
			FinalBarId = WidgetBarId;
		}

		const FGameplayTag WidgetQuickSlotId = IArcQuickBarSlotWidget::Execute_GetQuickBarSlotId(const_cast<UUserWidget*>(UserWidget));
		if (WidgetQuickSlotId.IsValid())
		{
			FinalQuickSlotId = WidgetQuickSlotId;
		}
	}

	if (!FinalBarId.IsValid() || !FinalQuickSlotId.IsValid())
	{
		return nullptr;
	}

	APlayerState* PS = UserWidget->GetOwningPlayerState();
	if (!PS)
	{
		return nullptr;
	}

	UArcFrontendMVVMModelComponent* Comp = PS->FindComponentByClass<UArcFrontendMVVMModelComponent>();
	if (!Comp)
	{
		return nullptr;
	}

	UArcQuickBarSlotViewModel* SlotViewModel = ResolveSlotViewModel(*Comp, FinalBarId, FinalQuickSlotId);

	// Guard the engine's IsA(ExpectedType) contract explicitly: return null + log
	// rather than hand back a wrong-type VM the engine would silently reject.
	if (SlotViewModel && ExpectedType && !SlotViewModel->IsA(ExpectedType))
	{
		UE_LOGF(LogTemp, Warning,"ArcQuickBar slot resolver: VM '%ls' for bar '%ls' slot '%ls' is not a '%ls'",
			*SlotViewModel->GetClass()->GetName(), *FinalBarId.ToString(), *FinalQuickSlotId.ToString(), *ExpectedType->GetName());
		return nullptr;
	}

	return SlotViewModel;
}
