// Copyright Lukasz Baran. All Rights Reserved.

#include "Events/ArcMassSignalListener.h"
#include "Events/ArcMassEventBufferSubsystem.h"

FArcMassSignalListener::~FArcMassSignalListener()
{
	Unbind();
}

void FArcMassSignalListener::Bind(UWorld* World, FArcAttributeChangedCallback InCallback)
{
	Unbind();

	BoundWorld = World;
	Callback = InCallback;
	bFilterByEntity = false;

	UArcMassEventBufferSubsystem* BufferSubsystem = UWorld::GetSubsystem<UArcMassEventBufferSubsystem>(World);
	if (BufferSubsystem)
	{
		DelegateHandle = BufferSubsystem->OnAttributeChangesReady.AddRaw(this, &FArcMassSignalListener::HandleBufferedChanges);
	}
}

void FArcMassSignalListener::BindEntity(UWorld* World, FMassEntityHandle InEntity, FArcAttributeChangedCallback InCallback)
{
	Unbind();

	BoundWorld = World;
	Callback = InCallback;
	BoundEntity = InEntity;
	bFilterByEntity = true;

	UArcMassEventBufferSubsystem* BufferSubsystem = UWorld::GetSubsystem<UArcMassEventBufferSubsystem>(World);
	if (BufferSubsystem)
	{
		DelegateHandle = BufferSubsystem->OnAttributeChangesReady.AddRaw(this, &FArcMassSignalListener::HandleBufferedChanges);
	}
}

void FArcMassSignalListener::Unbind()
{
	if (DelegateHandle.IsValid() && BoundWorld.IsValid())
	{
		UArcMassEventBufferSubsystem* BufferSubsystem = UWorld::GetSubsystem<UArcMassEventBufferSubsystem>(BoundWorld.Get());
		if (BufferSubsystem)
		{
			BufferSubsystem->OnAttributeChangesReady.Remove(DelegateHandle);
		}
	}
	DelegateHandle.Reset();
	Callback.Unbind();
	bFilterByEntity = false;
}

void FArcMassSignalListener::HandleBufferedChanges(TConstArrayView<FArcBufferedAttributeChange> Changes)
{
	if (!Callback.IsBound())
	{
		return;
	}

	for (const FArcBufferedAttributeChange& Change : Changes)
	{
		if (bFilterByEntity && Change.Entity != BoundEntity)
		{
			continue;
		}

		FArcAttributeChangedFragment Fragment;
		Fragment.Changes = Change.Changes;
		Callback.Execute(Change.Entity, Fragment);
	}
}
