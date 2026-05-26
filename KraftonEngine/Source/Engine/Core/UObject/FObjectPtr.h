#pragma once
#include "Core/ClassTypes.h"

class UObject;

struct FObjectPtr
{
public:
	FObjectPtr() = default;
	FObjectPtr(const FObjectPtr& Other);
	FObjectPtr(int Index);
	FObjectPtr(const UObject* InObject);

	void Reset() {  }

	bool IsValid() const;



public:
	bool operator== (const FObjectPtr& Other) const;
	bool operator!= (const FObjectPtr& Other) const;
	void operator= (const UObject* Object);
	FObjectPtr& operator= (const FObjectPtr& Other);


protected:

};