// Copyright Lukasz Baran. All Rights Reserved.

#include "Mesh/ArcRenderMeshFragments.h"

#include "InstancedStaticMeshSceneProxyDesc.h"
#include "Mass/EntityFragments.h"
#include "StaticMeshSceneProxyDesc.h"

struct FTransformFragment;

//----------------------------------------------------------------------//
// FArcStaticMeshFragment
//----------------------------------------------------------------------//
FArcStaticMeshFragment::FArcStaticMeshFragment(TNotNull<const UStaticMesh*> InMesh)
	: Mesh(InMesh)
{
}

FArcRenderStaticMeshFragment::FArcRenderStaticMeshFragment()
	: StaticMeshSceneProxyDesc(MakeShared<FStaticMeshSceneProxyDesc>())
{
}

FArcRenderStaticMeshFragment::FArcRenderStaticMeshFragment(const TSharedRef<FStaticMeshSceneProxyDesc>& InStaticMeshSceneProxyDesc)
	: StaticMeshSceneProxyDesc(InStaticMeshSceneProxyDesc)
{
}

UStaticMesh* FArcRenderStaticMeshFragment::GetStaticMesh()
{
	return StaticMeshSceneProxyDesc ? StaticMeshSceneProxyDesc->StaticMesh : nullptr;
}

const UStaticMesh* FArcRenderStaticMeshFragment::GetStaticMesh() const
{
	return StaticMeshSceneProxyDesc ? StaticMeshSceneProxyDesc->StaticMesh : nullptr;
}

#if WITH_EDITOR
//----------------------------------------------------------------------//
// HArcTypeElementHandleHitProxy
//----------------------------------------------------------------------//
IMPLEMENT_HIT_PROXY(HArcTypeElementHandleHitProxy, HHitProxy);

EMouseCursor::Type HArcTypeElementHandleHitProxy::GetMouseCursor()
{
	return EMouseCursor::Crosshairs;
}

FArcResolveTypeElementHandleDelegate& GetArcResolveTypeElementHandleDelegate()
{
	static FArcResolveTypeElementHandleDelegate Delegate;
	return Delegate;
}
#endif // WITH_EDITOR

//----------------------------------------------------------------------//
// FArcRenderISMFragment
//----------------------------------------------------------------------//
FArcRenderISMFragment::FArcRenderISMFragment()
	: InstancedStaticMeshSceneProxyDesc(MakeShared<FInstancedStaticMeshSceneProxyDesc>())
{
}

FArcRenderISMFragment::FArcRenderISMFragment(const TSharedRef<FInstancedStaticMeshSceneProxyDesc>& InInstancedStaticMeshSceneProxyDesc)
	: InstancedStaticMeshSceneProxyDesc(InInstancedStaticMeshSceneProxyDesc)
{
}

UStaticMesh* FArcRenderISMFragment::GetStaticMesh()
{
	return InstancedStaticMeshSceneProxyDesc ? InstancedStaticMeshSceneProxyDesc->StaticMesh : nullptr;
}

const UStaticMesh* FArcRenderISMFragment::GetStaticMesh() const
{
	return InstancedStaticMeshSceneProxyDesc ? InstancedStaticMeshSceneProxyDesc->StaticMesh : nullptr;
}
