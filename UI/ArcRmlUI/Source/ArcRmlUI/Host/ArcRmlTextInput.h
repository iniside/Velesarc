// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/ITextInputMethodSystem.h"
#include "Thread/ArcRmlUIQueues.h"

#include "RmlUi/Core/Header.h"
#include "RmlUi/Core/Traits.h"
#include "RmlUi/Core/TextInputHandler.h"

class SArcRmlViewport;

namespace Rml
{
	class TextInputContext;
}

class FArcRmlTextInputHandler final : public Rml::TextInputHandler
{
public:
	void SetCurrentView(uint32 ViewId);
	Rml::TextInputContext* GetActiveContext(uint32 ViewId) const;

	virtual void OnActivate(Rml::TextInputContext* InputContext) override;
	virtual void OnDeactivate(Rml::TextInputContext* InputContext) override;
	virtual void OnDestroy(Rml::TextInputContext* InputContext) override;

private:
	Rml::TextInputContext* ActiveContext = nullptr;
	uint32 ActiveViewId = 0;
	uint32 CurrentViewId = 0;
};

class FArcRmlTextInputContext final : public ITextInputMethodContext
{
public:
	explicit FArcRmlTextInputContext(const TWeakPtr<SArcRmlViewport>& InOwner);

	bool ApplyShadow(const FArcRmlTextFieldShadow& Shadow);

	virtual bool IsComposing() override;
	virtual bool IsReadOnly() override;
	virtual uint32 GetTextLength() override;
	virtual void GetSelectionRange(uint32& OutBeginIndex, uint32& OutLength, ECaretPosition& OutCaretPosition) override;
	virtual void SetSelectionRange(const uint32 InBeginIndex, const uint32 InLength, const ECaretPosition InCaretPosition) override;
	virtual void GetTextInRange(const uint32 InBeginIndex, const uint32 InLength, FString& OutString) override;
	virtual void SetTextInRange(const uint32 InBeginIndex, const uint32 InLength, const FString& InString) override;
	virtual int32 GetCharacterIndexFromPoint(const FVector2D& InPoint) override;
	virtual bool GetTextBounds(const uint32 InBeginIndex, const uint32 InLength, FVector2D& OutPosition, FVector2D& OutSize) override;
	virtual void GetScreenBounds(FVector2D& OutPosition, FVector2D& OutSize) override;
	virtual TSharedPtr<FGenericWindow> GetWindow() override;
	virtual void BeginComposition() override;
	virtual void UpdateCompositionRange(const int32 InBeginIndex, const uint32 InLength) override;
	virtual void EndComposition() override;

private:
	void PostEvent(EArcRmlInputType Type, int32 RangeStart, int32 RangeEnd, const FString& Text);

	TWeakPtr<SArcRmlViewport> Owner;
	FString Text;
	int32 SelectionStart = 0;
	int32 SelectionEnd = 0;
	FIntRect Bounds;
	uint64 Generation = 0;
	bool bComposing = false;
};
