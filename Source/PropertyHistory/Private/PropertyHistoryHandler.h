// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"

class ISourceControlState;

struct FPropertyHistoryEntry
{
	FInstancedPropertyBag Value;
	TSharedPtr<ISourceControlRevision> Revision;
};

class FPropertyHistoryHandler
	: public TSharedFromThis<FPropertyHistoryHandler>
	, public FTSTickerObjectBase
{
public:
	FSimpleMulticastDelegate OnNewEntry;
	TArray<TSharedPtr<FPropertyHistoryEntry>> Entries;

public:
	explicit FPropertyHistoryHandler(const TArray<const FProperty*>& Properties)
		: Properties(Properties)
	{
	}

	bool Initialize(const UObject& Object);
	void ShowHistory();
	void ShowFullHistory();

	bool IsLoading() const;

	const TOptional<FString>& GetError() const
	{
		return Error;
	}

protected:
	//~ Begin FTSTickerObjectBase Interface
	virtual bool Tick(float DeltaTime) override;
	//~ End FTSTickerObjectBase Interface

private:
	const TArray<const FProperty*> Properties;
	FString PackageFilename;
	TArray<TWeakObjectPtr<const UObject>> OuterChain;

	bool bWaitingForUpdateStatus = true;
	bool bUpdateStatusReady = false;

	TOptional<FString> Error;
	int32 HistoryIndex = 0;
	TOptional<TFuture<FString>> Future;
	TSharedPtr<ISourceControlState> SourceControlState;

	void Tick();
	void AddError(const FString& NewError);
};