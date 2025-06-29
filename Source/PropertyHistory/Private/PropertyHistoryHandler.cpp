// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "PropertyHistoryHandler.h"
#include "PropertyHistoryUtilities.h"
#include "SPropertyHistory.h"
#include "Async/Async.h"
#include "DiffUtils.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "SourceControlWindows.h"
#include "SourceControlOperations.h"

bool FPropertyHistoryHandler::Initialize(const UObject& Object)
{
	const ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if (!SourceControlProvider.IsEnabled())
	{
		return false;
	}

	{
		const UObject* CurrentObject = &Object;
		OuterChain.Add(CurrentObject);

		while (!CurrentObject->IsA<AActor>())
		{
			CurrentObject = CurrentObject->GetOuter();

			if (!CurrentObject)
			{
				return false;
			}

			OuterChain.Add(CurrentObject);
		}
	}

	const AActor* Actor = Cast<AActor>(OuterChain.Last().Get());
	if (!ensure(Actor) ||
		!Actor->IsPackageExternal())
	{
		return false;
	}

	const FString PackageName = Actor->GetExternalPackage()->GetName();
	const TArray<FString> PackageFilenames = SourceControlHelpers::PackageFilenames({ PackageName });

	if (!ensure(PackageFilenames.Num() == 1))
	{
		return false;
	}

	PackageFilename = PackageFilenames[0];

	return true;
}

void FPropertyHistoryHandler::ShowHistory()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	const TSharedRef<FUpdateStatus> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory(true);

	if (!SourceControlProvider.Execute(
		UpdateStatusOperation,
		{ PackageFilename },
		EConcurrency::Asynchronous,
		MakeLambdaDelegate(MakeWeakPtrLambda(this, [this](const FSourceControlOperationRef&, const ECommandResult::Type Result)
		{
			check(IsInGameThread());

			if (Result != ECommandResult::Succeeded)
			{
				AddError("Failed to update status for " + PackageFilename);
				return;
			}

			ensure(bWaitingForUpdateStatus);
			ensure(!bUpdateStatusReady);
			bUpdateStatusReady = true;
		}))))
	{
		AddError("Failed to update status for " + PackageFilename);
		return;
	}

	const FVector2f CursorPos = FSlateApplication::Get().GetCursorPos();

	const TSharedRef<SWindow> Window =
		SNew(SWindow)
		.Type(EWindowType::Menu)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.IsPopupWindow(true)
		.bDragAnywhere(true)
		.IsTopmostWindow(true)
		.SizingRule(ESizingRule::Autosized)
		.ScreenPosition(CursorPos)
		.SupportsTransparency(EWindowTransparency::PerPixel);

	Window->MoveWindowTo(CursorPos);

	const TSharedRef<SPropertyHistory> Widget =
		SNew(SPropertyHistory)
		.Window(Window)
		.Handler(AsShared());

	Window->SetContent(Widget);

	const TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (!ensure(RootWindow))
	{
		return;
	}

	FSlateApplication::Get().AddWindowAsNativeChild(Window, RootWindow.ToSharedRef());
	Window->BringToFront();

	FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
}

void FPropertyHistoryHandler::ShowFullHistory()
{
	FSourceControlWindows::DisplayRevisionHistory({ PackageFilename });
}

bool FPropertyHistoryHandler::IsLoading() const
{
	if (Error.IsSet())
	{
		return false;
	}

	if (!SourceControlState)
	{
		return true;
	}

	return HistoryIndex < SourceControlState->GetHistorySize();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FPropertyHistoryHandler::Tick(float DeltaTime)
{
	Tick();
	return true;
}

void FPropertyHistoryHandler::Tick()
{
	if (bWaitingForUpdateStatus)
	{
		if (!bUpdateStatusReady)
		{
			return;
		}

		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

		TArray<FSourceControlStateRef> SourceControlStates;
		if (SourceControlProvider.GetState(
			{ PackageFilename },
			SourceControlStates,
			EStateCacheUsage::Use) != ECommandResult::Succeeded)
		{
			SourceControlStates.Empty();
		}

		if (SourceControlStates.Num() != 1)
		{
			AddError("Failed to get source control state for " + PackageFilename);
			return;
		}

		SourceControlState = SourceControlStates[0];
	}

	if (!SourceControlState)
	{
		return;
	}

	if (Future.IsSet() &&
		!Future->IsReady())
	{
		return;
	}

	if (HistoryIndex == SourceControlState->GetHistorySize())
	{
		return;
	}

	const TSharedPtr<ISourceControlRevision> Revision = SourceControlState->GetHistoryItem(HistoryIndex);
	if (!Revision)
	{
		AddError("Failed to get source control state for " + PackageFilename);
		HistoryIndex++;
		return;
	}

	if (!Future.IsSet())
	{
		Future = Async(EAsyncExecution::LargeThreadPool, [Revision]
		{
			FString TempFileName;
			if (!Revision->Get(TempFileName, EConcurrency::Asynchronous))
			{
				TempFileName.Empty();
			}
			return TempFileName;
		});

		return;
	}
	check(Future->IsReady());

	HistoryIndex++;

	const FString TempFileName = Future->Get();
	Future.Reset();

	const FPackagePath TempPackagePath = FPackagePath::FromLocalPath(TempFileName);
	const FPackagePath OriginalPackagePath = FPackagePath::FromLocalPath(Revision->GetFilename());

	UPackage* Package = DiffUtils::LoadPackageForDiff(TempPackagePath, OriginalPackagePath);
	if (!Package)
	{
		AddError("Failed to load package for " + PackageFilename);
		return;
	}

	UObject* NewObject = Package;
	for (const TWeakObjectPtr<const UObject>& WeakOuter : ReverseIterate(OuterChain))
	{
		const UObject* Outer = WeakOuter.Get();
		if (!ensure(Outer))
		{
			return;
		}

		NewObject = StaticFindObject(nullptr, NewObject, *Outer->GetName());

		if (!NewObject)
		{
			// Object did not exist yet
			return;
		}
	}

	void* Container = NewObject;
	for (int32 Index = Properties.Num() - 1; Index >= 1; Index--)
	{
		Container = Properties[Index]->ContainerPtrToValuePtr<void>(Container);
	}

	FInstancedPropertyBag Value;
	Value.AddProperty("Value", Properties[0]);

	if (!ensure(Value.SetValue("Value", Properties[0], Container) == EPropertyBagResult::Success))
	{
		return;
	}

	const TSharedRef<FPropertyHistoryEntry> NewEntry = MakeSharedCopy(FPropertyHistoryEntry
	{
		MoveTemp(Value),
		Revision
	});

	if (Entries.Num() > 0 &&
		Entries.Last()->Value.Identical(&NewEntry->Value, PPF_None))
	{
		// Replace the entry, the current one we have didn't change this property
		Entries.Last() = NewEntry;
	}
	else
	{
		Entries.Add(NewEntry);
	}

	OnNewEntry.Broadcast();
}

void FPropertyHistoryHandler::AddError(const FString& NewError)
{
	if (Error.IsSet())
	{
		Error.GetValue() += "\n" + NewError;
	}
	else
	{
		Error = NewError;
	}
}