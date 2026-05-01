// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Fragments/ArcAttributeChangedFragment.h"

struct FArcBufferedAttributeChange;

DECLARE_DELEGATE_TwoParams(FArcAttributeChangedCallback, FMassEntityHandle /*Entity*/, const FArcAttributeChangedFragment& /*Fragment*/);

struct ARCMASSABILITIES_API FArcMassSignalListener
{
	~FArcMassSignalListener();

	void Bind(UWorld* World, FArcAttributeChangedCallback InCallback);
	void BindEntity(UWorld* World, FMassEntityHandle InEntity, FArcAttributeChangedCallback InCallback);
	void Unbind();

private:
	void HandleBufferedChanges(TConstArrayView<FArcBufferedAttributeChange> Changes);

	FArcAttributeChangedCallback Callback;
	FMassEntityHandle BoundEntity;
	bool bFilterByEntity = false;
	FDelegateHandle DelegateHandle;
	TWeakObjectPtr<UWorld> BoundWorld;
};
