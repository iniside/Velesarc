// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementWorldInterface.h"
#include "SkMInstanceElementEditorWorldInterface.generated.h"

UCLASS(MinimalAPI)
class USkMInstanceElementEditorWorldInterface : public USkMInstanceElementWorldInterface
{
	GENERATED_BODY()

public:
	ARCMASSEDITOR_API virtual bool CanDeleteElement(const FTypedElementHandle& InElementHandle) override;
	ARCMASSEDITOR_API virtual bool DeleteElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions) override;
	ARCMASSEDITOR_API virtual bool CanDuplicateElement(const FTypedElementHandle& InElementHandle) override;
	ARCMASSEDITOR_API virtual void DuplicateElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements) override;
};
