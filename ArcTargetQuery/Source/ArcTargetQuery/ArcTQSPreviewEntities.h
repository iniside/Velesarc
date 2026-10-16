// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "ArcTQSQueryReference.h"

struct FMassEntityManager;
struct FArcTQSPreviewMock;
class UWorld;

class ARCTARGETQUERY_API FArcTQSPreviewEntities : public FNoncopyable
{
public:
	FMassEntityHandle Create(UWorld& InWorld, const FArcTQSPreviewMock& Mock);
	void Move(FMassEntityHandle Entity, const FTransform& Transform);
	void Destroy(FMassEntityHandle Entity);
	void Release();

	FMassEntityHandle FindByName(FName Name) const;

private:
	struct FEntry
	{
		FName Name;
		FMassEntityHandle Entity;
	};

	FMassEntityManager* GetEntityManager() const;
	void DestroyEntity(FMassEntityHandle Entity) const;

	TWeakObjectPtr<UWorld> World;
	TArray<FEntry> Entries;
};

namespace ArcTQS
{
	ARCTARGETQUERY_API FArcTQSQueryReference MakeReferenceWithEntityParameters(
		const FArcTQSQueryReference& Reference,
		const TMap<FGuid, FMassEntityHandle>& EntityParameters);
}
