// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "PSOPrecacheValidation.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Templates/SharedPointer.h"

#include "ArcRenderStateHelper.generated.h"

struct FArcRenderStateFragment;
struct FMassEntityManager;
class UActorComponent;
class FSceneInterface;
class UWorld;
struct FArcRenderStateHelper;
class FRegisterComponentContext;
struct FArcDestroyRenderStateContext;

namespace UE::ArcMassRender
{
	enum class ESceneProxyCreationError
	{
		None,
		WaitingPSOs,
		InvalidMesh
	};

#if WITH_EDITOR
	/** Strip every componentless render-state FScene proxy owned by World's Mass holders, before the
	 *  world's FScene is released. Editor-only world-teardown safety net (bound in ArcMassRenderModule).
	 *  Removes the scene proxy only; the helper survives so later teardown paths stay idempotent. */
	ARCMASSRENDER_API void StripWorldRenderStateProxies(UWorld& World);
#endif // WITH_EDITOR
}

/**
 * Fragment holding the render state
 */
USTRUCT()
struct ARCMASSRENDER_API FArcRenderStateFragment : public FMassFragment
{
	GENERATED_BODY()

	FArcRenderStateFragment() = default;
	void DestroyRenderStateHelper();
	bool HasRenderStateHelper() const { return RenderStateHelper.IsValid(); }

	template<typename T = FArcRenderStateHelper>
	const T& GetRenderStateHelper() const requires TIsDerivedFrom<T, FArcRenderStateHelper>::Value
	{
		checkf(RenderStateHelper, TEXT("Expecting the ptr to be set"));
		// @Todo find a better way of doing this unsafe cast, Instanced struct are not possible du to atomics member variables
		return static_cast<T&>(*RenderStateHelper);
	}

	template<typename T = FArcRenderStateHelper>
	T& GetRenderStateHelper() requires TIsDerivedFrom<T, FArcRenderStateHelper>::Value
	{
		checkf(RenderStateHelper, TEXT("Expecting the ptr to be set"));
		// @Todo find a better way of doing this unsafe cast, Instanced struct are not possible du to atomics member variables
		return static_cast<T&>(*RenderStateHelper);
	}

	template<typename T, typename... TArgs>
	T* CreateRenderStateHelper(TArgs&&... InArgs) requires TIsDerivedFrom<T, FArcRenderStateHelper>::Value
	{
		checkf(!RenderStateHelper, TEXT("Render State Helper already exist"));
		TSharedPtr<T> CreatedRenderState = MakeShared<T>(Forward<TArgs>(InArgs)...);
		RenderStateHelper = CreatedRenderState;
		return CreatedRenderState.Get();
	}

private:
	TSharedPtr<FArcRenderStateHelper> RenderStateHelper = nullptr;

};

template<>
struct TMassFragmentTraits<FArcRenderStateFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Base helper to handle all the communication to the renderer from Mass
 */
struct ARCMASSRENDER_API FArcRenderStateHelper : public TSharedFromThis<FArcRenderStateHelper>
{
public:

	FArcRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager);
	virtual ~FArcRenderStateHelper() = default;

	void MarkRenderStateDirty();
	bool IsRenderStateDirty() const;
	bool IsRenderStateDelayed() const;
	bool IsRenderStateCreated() const;
	virtual bool ShouldCreateRenderState() const;
	virtual void CreateRenderState(FRegisterComponentContext* Context);
	virtual void DestroyRenderState(FArcDestroyRenderStateContext* Context);

	/** Editor-teardown hook: release any owned FSkeletalMeshObject (and its update handle).
	 *  No-op for helpers that own no skeletal mesh object. Safe to call repeatedly. */
	virtual void ReleaseMeshObjectForTeardown() {}

	virtual void UpdateVisibility();
	virtual void MarkPrecachePSOsRequired();
	virtual void PrecachePSOs();
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	virtual void OnPrecacheFinished(int32 JobSetThatJustCompleted);
#endif

	UWorld* GetWorld() const;
	FSceneInterface* GetScene() const;

	FMassEntityManager& GetEntityManager() const;

protected:
	FMassEntityHandle EntityHandle;
	TSharedRef<FMassEntityManager> EntityManager;

	const FArcRenderStateFragment& GetRenderStateFragment() const;
	FArcRenderStateFragment& GetMutableRenderStateFragment();

	enum class EProxyCreationState : uint8
	{
		None, // Constructed/Initialized
		Creating, // Actively creating the proxy
		Created, // Proxy is now created
		Delayed // Proxy creation delayed (used when PSO precaching is not ready when creating proxy)
	};

	EProxyCreationState ProxyState = EProxyCreationState::None;
	bool bRenderStateDirty = false;
};
