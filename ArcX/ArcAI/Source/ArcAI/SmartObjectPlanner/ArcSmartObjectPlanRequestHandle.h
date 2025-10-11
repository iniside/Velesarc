#pragma once

struct FArcSmartObjectPlanRequestHandle
{
	uint32 Handle = 0;

	static FArcSmartObjectPlanRequestHandle Make()
	{
		static uint32 NewHandle = 0;
		NewHandle++;
		
		FArcSmartObjectPlanRequestHandle NewHandleObj;
		NewHandleObj.Handle = NewHandle;

		return NewHandleObj;
	}

	bool operator==(const FArcSmartObjectPlanRequestHandle& Other) const
	{
		return Handle == Other.Handle;
	}
};
