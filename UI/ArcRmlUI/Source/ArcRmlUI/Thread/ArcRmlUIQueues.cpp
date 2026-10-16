// Copyright Lukasz Baran. All Rights Reserved.

#include "Thread/ArcRmlUIQueues.h"

#include "Misc/ScopeLock.h"

void FArcRmlViewChannel::PublishSnapshot(FArcRmlViewSnapshot&& InSnapshot)
{
	FScopeLock Lock(&SnapshotLock);
	Snapshot = MoveTemp(InSnapshot);
	++SnapshotSerial;
}

bool FArcRmlViewChannel::ReadSnapshot(FArcRmlViewSnapshot& OutSnapshot, uint64& InOutSerial) const
{
	FScopeLock Lock(&SnapshotLock);
	if (SnapshotSerial == InOutSerial)
	{
		return false;
	}
	OutSnapshot = Snapshot;
	InOutSerial = SnapshotSerial;
	return true;
}

void FArcRmlViewChannel::PublishCamera(const FArcRmlViewCamera& InCamera)
{
	FScopeLock Lock(&CameraLock);
	Camera = InCamera;
	++CameraSerial;
}

bool FArcRmlViewChannel::ReadCamera(FArcRmlViewCamera& OutCamera, uint64& InOutSerial) const
{
	FScopeLock Lock(&CameraLock);
	if (CameraSerial == InOutSerial)
	{
		return false;
	}
	OutCamera = Camera;
	InOutSerial = CameraSerial;
	return true;
}

void FArcRmlViewChannel::PushOutbound(FArcRmlOutboundEvent&& Event)
{
	Outbound.Enqueue(MoveTemp(Event));
}

bool FArcRmlViewChannel::PopOutbound(FArcRmlOutboundEvent& OutEvent)
{
	return Outbound.Dequeue(OutEvent);
}
