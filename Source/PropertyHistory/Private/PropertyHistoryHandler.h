// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "PropertyHistoryProcessor.h"

class ISourceControlState;
class FDetailColumnSizeData;

struct FPropertyHistoryEntry
{
	FInstancedPropertyBag Value;
	TSharedPtr<ISourceControlRevision> Revision;

	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
	TSharedPtr<IDetailTreeNode> Node;
	TSharedPtr<IPropertyHandle> Handle;

	TSharedPtr<FDetailColumnSizeData> ColumnSizeData;
	TArray<TSharedPtr<FPropertyHistoryEntry>> Children;
};

class FPropertyHistoryHandler
	: public TSharedFromThis<FPropertyHistoryHandler>
	, public FTSTickerObjectBase
{
public:
	FSimpleMulticastDelegate OnNewEntry;
	TArray<TSharedPtr<FPropertyHistoryEntry>> Entries;

public:
	explicit FPropertyHistoryHandler(const FPropertyHistoryProcessor& Processor);

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
	TArray<FPropertyData> PropertyChain;
	FString PackageFilename;
	TArray<TWeakObjectPtr<const UObject>> OuterChain;

	bool bWaitingForUpdateStatus = true;
	bool bUpdateStatusReady = false;

	TOptional<FString> Error;
	int32 HistoryIndex = 0;
	TOptional<TFuture<FString>> Future;
	TSharedPtr<ISourceControlState> SourceControlState;

	const FGuid PropertyGuid;

	void Tick();
	void AddError(const FString& NewError);
};