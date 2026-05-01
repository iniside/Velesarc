// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "SkMInstanceElementWorldInterface.generated.h"

UCLASS(MinimalAPI)
class USkMInstanceElementWorldInterface : public UObject, public ITypedElementWorldInterface
{
	GENERATED_BODY()

public:
	ARCMASS_API virtual bool CanEditElement(const FTypedElementHandle& InElementHandle) override;
	ARCMASS_API virtual bool IsTemplateElement(const FTypedElementHandle& InElementHandle) override;
	ARCMASS_API virtual ULevel* GetOwnerLevel(const FTypedElementHandle& InElementHandle) override;
	ARCMASS_API virtual UWorld* GetOwnerWorld(const FTypedElementHandle& InElementHandle) override;
	ARCMASS_API virtual bool GetBounds(const FTypedElementHandle& InElementHandle, FBoxSphereBounds& OutBounds) override;
	ARCMASS_API virtual bool CanMoveElement(const FTypedElementHandle& InElementHandle, const ETypedElementWorldType InWorldType) override;
	ARCMASS_API virtual bool GetWorldTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform) override;
	ARCMASS_API virtual bool SetWorldTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform) override;
	ARCMASS_API virtual bool GetRelativeTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform) override;
	ARCMASS_API virtual bool SetRelativeTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform) override;
	ARCMASS_API virtual void NotifyMovementStarted(const FTypedElementHandle& InElementHandle) override;
	ARCMASS_API virtual void NotifyMovementOngoing(const FTypedElementHandle& InElementHandle) override;
	ARCMASS_API virtual void NotifyMovementEnded(const FTypedElementHandle& InElementHandle) override;
};
