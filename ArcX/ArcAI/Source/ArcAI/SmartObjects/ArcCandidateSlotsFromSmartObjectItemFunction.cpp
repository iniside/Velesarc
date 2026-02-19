// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCandidateSlotsFromSmartObjectItemFunction.h"

#include "StateTreeExecutionContext.h"

void FArcCandidateSlotsFromSmartObjectItemFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	
	FSmartObjectCandidateSlot CandidateSlot;
	CandidateSlot.Result.SmartObjectHandle = InstanceData.Input.SmartObjectHandle;
	CandidateSlot.Result.SlotHandle = InstanceData.Input.SlotHandle;
	CandidateSlot.Cost = 0.f;
	InstanceData.CandidateSlots.Slots[0] = CandidateSlot;
	InstanceData.CandidateSlots.NumSlots = 1;
}
