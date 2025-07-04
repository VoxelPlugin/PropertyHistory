// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "PropertyHistoryHandler.h"
#include "DiffUtils.h"
#include "Async/Async.h"
#include "SPropertyHistory.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "SourceControlWindows.h"
#include "SourceControlOperations.h"
#include "PropertyHistoryUtilities.h"
#include "PropertyHistoryProcessor.h"

FPropertyHistoryHandler::FPropertyHistoryHandler(const FPropertyHistoryProcessor& Processor)
	: PropertyChain(Processor.Properties)
	, PropertyGuid(Processor.Guid)
{
}

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

		while (
			!CurrentObject->IsA<UPackage>() &&
			!CurrentObject->IsPackageExternal())
		{
			CurrentObject = CurrentObject->GetOuter();

			if (!CurrentObject)
			{
				return false;
			}

			OuterChain.Add(CurrentObject);
		}
	}

	// Also handles UPackage
	const UPackage* Package = OuterChain.Last()->GetExternalPackage();
	if (!ensure(Package))
	{
		return false;
	}

	const FString PackageName = Package->GetName();
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

	ON_SCOPE_EXIT
	{
		const TSharedPtr<SDockTab> NewTab = FGlobalTabmanager::Get()->TryInvokeTab(FName("PropertyHistoryTab"));
		if (!ensure(NewTab))
		{
			return;
		}

		const TSharedRef<SPropertyHistory> PropertyHistoryWidget = StaticCastSharedRef<SPropertyHistory>(NewTab->GetContent());
		PropertyHistoryWidget->SetHandler(AsShared());

		FSlateApplication::Get().SetKeyboardFocus(PropertyHistoryWidget, EFocusCause::SetDirectly);
	};

	if (PropertyChain[0].Property->IsA<FSetProperty>())
	{
		AddError("Set container type variables cannot be previewed. Preview inner items.");
		return;
	}

	if (PropertyChain[0].Property->IsA<FMapProperty>())
	{
		AddError("Map container type variables cannot be previewed. Preview inner items.");
		return;
	}

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
	}
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

		if (Outer->IsA<UPackage>())
		{
			// Root package
			ensure(NewObject == Package);
			continue;
		}

		NewObject = StaticFindObject(nullptr, NewObject, *Outer->GetName());

		if (!NewObject)
		{
			// Object did not exist yet
			return;
		}
	}

	FPropertyHistoryProcessor Processor(NewObject, PropertyChain, PropertyGuid);
	void* Container = nullptr;
	if (!Processor.Process(Container))
	{
		return;
	}

	if (Container == nullptr)
	{
		return;
	}

	FInstancedPropertyBag Value;

	const bool bValueSet = INLINE_LAMBDA
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(PropertyChain[0].Property))
		{
			if (StructProperty->Struct == FInstancedStruct::StaticStruct())
			{
				FInstancedStruct* InstancedStruct = StructProperty->ContainerPtrToValuePtr<FInstancedStruct>(Container);
				if (!InstancedStruct->IsValid())
				{
					return false;
				}

				Container = InstancedStruct->GetMutableMemory();
				if (!Container)
				{
					return false;
				}

				Value.AddProperty("Value", EPropertyBagPropertyType::Struct, InstancedStruct->GetScriptStruct());
				const FConstStructView View(InstancedStruct->GetScriptStruct(), InstancedStruct->GetMutableMemory());
				Value.SetValueStruct("Value", View);
				return true;
			}
		}

		Value.AddProperty("Value", PropertyChain[0].Property);
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(PropertyChain[0].Property))
		{
			if (ByteProperty->Enum)
			{
				if (!ensure(Value.SetValueEnum("Value", *PropertyChain[0].Property->ContainerPtrToValuePtr<uint8>(Container), ByteProperty->Enum) == EPropertyBagResult::Success))
				{
					return false;
				}

				return true;
			}
		}

		if (!ensure(Value.SetValue("Value", PropertyChain[0].Property, Container) == EPropertyBagResult::Success))
		{
			return false;
		}

		return true;
	};

	if (!bValueSet)
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