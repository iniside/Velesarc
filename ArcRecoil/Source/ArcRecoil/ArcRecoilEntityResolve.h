// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

class AActor;
struct FMassEntityManager;

namespace ArcRecoil
{
	enum class EEntityResolveResult : uint8
	{
		Resolved,
		NoActor,
		NoWorld,
		NoEntitySubsystem,
		NoAgentComponent,
		InvalidHandle,
		MissingFragments
	};

	struct FResolvedRecoilEntity
	{
		FMassEntityManager* EntityManager = nullptr;
		FMassEntityHandle Entity;
	};

	EEntityResolveResult ResolveRecoilEntity(const AActor* Actor, FResolvedRecoilEntity& Out);

	const TCHAR* DescribeResolveResult(EEntityResolveResult Result);
}
