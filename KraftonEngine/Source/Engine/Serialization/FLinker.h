#pragma once
#include "Archive.h"

class FLinker : public FArchive
{
public:
	explicit FLinker(FArchive* InInner) : Inner(InInner) {}

protected:
	FArchive* Inner;				 // real byte stream underneath
	TArray<UObject*> ObjectTable;    // 1-based; index 0 reserved for null
};

class FLinkerSave final : public FLinker
{
public:
	explicit FLinkerSave(FArchive* InInner) : FLinker(InInner) { bIsSaving = true; }

	// TODO: Promote to MapObject when UPackage is a thing
	int32 IndexForObject(UObject* Obj);         // 0 = null; adds on first sight
	void  Finalize();                           // writes [Count][Path*Count][Buffer] to Inner

	void      Serialize(void* Data, size_t Num) override;     // appends to SaveBuffer
	FArchive& operator<<(UObject*& Obj) override;             // writes int32 index

private:
	TArray<uint8>          SaveBuffer;
	TMap<UObject*, int32>  ObjectToIndex;
};

class FLinkerLoad final : public FLinker
{
public:
	explicit FLinkerLoad(FArchive* InInner) : FLinker(InInner) { bIsLoading = true; }

	void     ReadTable();                                       // populates ObjectTable from Inner
	UObject* ObjectForIndex(int32 Index) const;                 // bounds-checked, 0 = null

	void      Serialize(void* Data, size_t Num) override;       // forwards to Inner
	FArchive& operator<<(UObject*& Obj) override;               // reads int32 index, looks up
};