// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/ArcRmlTextInput.h"

#include "Host/SArcRmlViewport.h"

void FArcRmlTextInputHandler::SetCurrentView(uint32 ViewId)
{
	CurrentViewId = ViewId;
}

Rml::TextInputContext* FArcRmlTextInputHandler::GetActiveContext(uint32 ViewId) const
{
	return ActiveViewId == ViewId ? ActiveContext : nullptr;
}

void FArcRmlTextInputHandler::OnActivate(Rml::TextInputContext* InputContext)
{
	ActiveContext = InputContext;
	ActiveViewId = CurrentViewId;
}

void FArcRmlTextInputHandler::OnDeactivate(Rml::TextInputContext* InputContext)
{
	if (ActiveContext == InputContext)
	{
		ActiveContext = nullptr;
		ActiveViewId = 0;
	}
}

void FArcRmlTextInputHandler::OnDestroy(Rml::TextInputContext* InputContext)
{
	OnDeactivate(InputContext);
}

FArcRmlTextInputContext::FArcRmlTextInputContext(const TWeakPtr<SArcRmlViewport>& InOwner)
	: Owner(InOwner)
{
}

bool FArcRmlTextInputContext::ApplyShadow(const FArcRmlTextFieldShadow& Shadow)
{
	if (Shadow.Generation < Generation)
	{
		return false;
	}

	const bool bChanged = Text != Shadow.Text || SelectionStart != Shadow.SelectionStart || SelectionEnd != Shadow.SelectionEnd;
	Text = Shadow.Text;
	SelectionStart = Shadow.SelectionStart;
	SelectionEnd = Shadow.SelectionEnd;
	Bounds = Shadow.Bounds;
	return bChanged;
}

void FArcRmlTextInputContext::PostEvent(EArcRmlInputType Type, int32 RangeStart, int32 RangeEnd, const FString& InText)
{
	const TSharedPtr<SArcRmlViewport> PinnedOwner = Owner.Pin();
	if (!PinnedOwner.IsValid())
	{
		return;
	}

	FArcRmlInputEvent Event;
	Event.Type = Type;
	Event.RangeStart = RangeStart;
	Event.RangeEnd = RangeEnd;
	Event.Text = InText;
	Event.Generation = ++Generation;
	PinnedOwner->PostInputEvent(MoveTemp(Event));
}

bool FArcRmlTextInputContext::IsComposing()
{
	return bComposing;
}

bool FArcRmlTextInputContext::IsReadOnly()
{
	return false;
}

uint32 FArcRmlTextInputContext::GetTextLength()
{
	return static_cast<uint32>(Text.Len());
}

void FArcRmlTextInputContext::GetSelectionRange(uint32& OutBeginIndex, uint32& OutLength, ECaretPosition& OutCaretPosition)
{
	OutBeginIndex = static_cast<uint32>(FMath::Min(SelectionStart, SelectionEnd));
	OutLength = static_cast<uint32>(FMath::Abs(SelectionEnd - SelectionStart));
	OutCaretPosition = SelectionEnd >= SelectionStart ? ECaretPosition::Ending : ECaretPosition::Beginning;
}

void FArcRmlTextInputContext::SetSelectionRange(const uint32 InBeginIndex, const uint32 InLength, const ECaretPosition InCaretPosition)
{
	const int32 Begin = FMath::Min(static_cast<int32>(InBeginIndex), Text.Len());
	const int32 End = FMath::Min(Begin + static_cast<int32>(InLength), Text.Len());
	SelectionStart = InCaretPosition == ECaretPosition::Beginning ? End : Begin;
	SelectionEnd = InCaretPosition == ECaretPosition::Beginning ? Begin : End;
	PostEvent(EArcRmlInputType::ImeSetSelection, Begin, End, FString());
}

void FArcRmlTextInputContext::GetTextInRange(const uint32 InBeginIndex, const uint32 InLength, FString& OutString)
{
	OutString = Text.Mid(static_cast<int32>(InBeginIndex), static_cast<int32>(InLength));
}

void FArcRmlTextInputContext::SetTextInRange(const uint32 InBeginIndex, const uint32 InLength, const FString& InString)
{
	const int32 Begin = FMath::Min(static_cast<int32>(InBeginIndex), Text.Len());
	const int32 End = FMath::Min(Begin + static_cast<int32>(InLength), Text.Len());
	Text = Text.Left(Begin) + InString + Text.Mid(End);
	SelectionStart = Begin + InString.Len();
	SelectionEnd = SelectionStart;
	PostEvent(EArcRmlInputType::ImeSetText, Begin, End, InString);
}

int32 FArcRmlTextInputContext::GetCharacterIndexFromPoint(const FVector2D& InPoint)
{
	return INDEX_NONE;
}

bool FArcRmlTextInputContext::GetTextBounds(const uint32 InBeginIndex, const uint32 InLength, FVector2D& OutPosition, FVector2D& OutSize)
{
	GetScreenBounds(OutPosition, OutSize);
	return false;
}

void FArcRmlTextInputContext::GetScreenBounds(FVector2D& OutPosition, FVector2D& OutSize)
{
	const TSharedPtr<SArcRmlViewport> PinnedOwner = Owner.Pin();
	if (!PinnedOwner.IsValid())
	{
		OutPosition = FVector2D::ZeroVector;
		OutSize = FVector2D::ZeroVector;
		return;
	}

	const FVector2D TopLeft = PinnedOwner->ViewPixelsToScreen(FVector2f(Bounds.Min));
	const FVector2D BottomRight = PinnedOwner->ViewPixelsToScreen(FVector2f(Bounds.Max));
	OutPosition = TopLeft;
	OutSize = BottomRight - TopLeft;
}

TSharedPtr<FGenericWindow> FArcRmlTextInputContext::GetWindow()
{
	const TSharedPtr<SArcRmlViewport> PinnedOwner = Owner.Pin();
	return PinnedOwner.IsValid() ? PinnedOwner->GetNativeWindow() : nullptr;
}

void FArcRmlTextInputContext::BeginComposition()
{
	bComposing = true;
}

void FArcRmlTextInputContext::UpdateCompositionRange(const int32 InBeginIndex, const uint32 InLength)
{
	PostEvent(EArcRmlInputType::ImeSetComposition, InBeginIndex, InBeginIndex + static_cast<int32>(InLength), FString());
}

void FArcRmlTextInputContext::EndComposition()
{
	bComposing = false;
	PostEvent(EArcRmlInputType::ImeSetComposition, 0, 0, FString());
}
