// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcPendingAttributeOpsFragment.generated.h"

UENUM()
enum class EArcAttributeOpType : uint8
{
	Change,
	Execute
};

USTRUCT()
struct ARCMASSABILITIES_API FArcPendingAttributeOp
{
	GENERATED_BODY()

	FArcAttributeRef Attribute;
	EArcModifierOp Operation = EArcModifierOp::Add;
	float Magnitude = 0.f;
	EArcAttributeOpType OpType = EArcAttributeOpType::Change;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcPendingAttributeOpsFragment : public FMassFragment
{
	GENERATED_BODY()

	TArray<FArcPendingAttributeOp> PendingOps;
};

template <>
struct TMassFragmentTraits<FArcPendingAttributeOpsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

namespace UE::ArcMassAbilities::Signals
{
	const FName PendingAttributeOps = FName(TEXT("ArcPendingAttributeOps"));
}
