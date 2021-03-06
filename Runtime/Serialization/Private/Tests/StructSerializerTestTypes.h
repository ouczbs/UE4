// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "StructSerializerTestTypes.generated.h"

/**
 * Test structure for numeric properties.
 */
USTRUCT()
struct FStructSerializerNumericTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	int8 Int8;

	UPROPERTY()
	int16 Int16;

	UPROPERTY()
	int32 Int32;

	UPROPERTY()
	int64 Int64;

	UPROPERTY()
	uint8 UInt8;

	UPROPERTY()
	uint16 UInt16;

	UPROPERTY()
	uint32 UInt32;

	UPROPERTY()
	uint64 UInt64;

	UPROPERTY()
	float Float;

	UPROPERTY()
	double Double;

	/** Default constructor. */
	FStructSerializerNumericTestStruct()
		: Int8(-127)
		, Int16(-32767)
		, Int32(-2147483647)
		, Int64(-92233720368547/*75807*/)
		, UInt8(255)
		, UInt16(65535)
		, UInt32(4294967295)
		, UInt64(18446744073709/*551615*/)
		, Float(4.125)
		, Double(1.03125)
	{ }

	/** Creates an uninitialized instance. */
	FStructSerializerNumericTestStruct( ENoInit ) { }
};


/**
 * Test structure for boolean properties.
 */
USTRUCT()
struct FStructSerializerBooleanTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	bool BoolFalse;

	UPROPERTY()
	bool BoolTrue;

	UPROPERTY()
	uint8 Bitfield0 : 1;

	UPROPERTY()
	uint8 Bitfield1 : 1;

	UPROPERTY()
	uint8 Bitfield2Set : 1;

	UPROPERTY()
	uint8 Bitfield3 : 1;

	UPROPERTY()
	uint8 Bitfield4Set : 1;

	UPROPERTY()
	uint8 Bitfield5Set : 1;

	UPROPERTY()
	uint8 Bitfield6 : 1;

	UPROPERTY()
	uint8 Bitfield7Set : 1;

	/** Default constructor. */
	FStructSerializerBooleanTestStruct()
		: BoolFalse(false)
		, BoolTrue(true)
		, Bitfield0(0)
		, Bitfield1(0)
		, Bitfield2Set(1)
		, Bitfield3(0)
		, Bitfield4Set(1)
		, Bitfield5Set(1)
		, Bitfield6(0)
		, Bitfield7Set(1)
	{ }

	/** Creates an uninitialized instance. */
	FStructSerializerBooleanTestStruct( ENoInit ) { }
};


/**
 * Test structure for UObject properties.
 */
USTRUCT()
struct FStructSerializerObjectTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	UClass* RawClass;

	UPROPERTY()
	TObjectPtr<UClass> WrappedClass;

	UPROPERTY()
	TSubclassOf<class UMetaData> SubClass;

	UPROPERTY()
	TSoftClassPtr<class UMetaData> SoftClass;

	UPROPERTY()
	class UObject* RawObject;

	UPROPERTY()
	TObjectPtr<class UObject> WrappedObject;

	UPROPERTY()
	TWeakObjectPtr<class UMetaData> WeakObject;

	UPROPERTY()
	TSoftObjectPtr<class UMetaData> SoftObject;

	UPROPERTY()
	FSoftClassPath ClassPath;

	UPROPERTY()
	FSoftObjectPath ObjectPath;

	/** Default constructor. */
	FStructSerializerObjectTestStruct()
		: RawClass(nullptr)
		, WrappedClass(nullptr)
		, SubClass(nullptr)
		, SoftClass(nullptr)
		, RawObject(nullptr)
		, WrappedObject(nullptr)
		, WeakObject(nullptr)
		, SoftObject(nullptr)
		, ClassPath((UClass*)nullptr)
		, ObjectPath(nullptr)
	{}

	/** Creates an uninitialized instance. */
	FStructSerializerObjectTestStruct( ENoInit ) {}
};


/**
 * Test structure for properties of various built-in types.
 * @see NoExportTypes.h
 */
USTRUCT()
struct FStructSerializerBuiltinTestStruct
{
	GENERATED_BODY()

	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	FGuid Guid;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString String;

	UPROPERTY()
	FText Text;

	// FDatetime and FTimespan should be tested here but aren't properly setup in `NoExportTypes.h` and so do not properly work currently.
	//UPROPERTY()
	//FDateTime Datetime;

	//UPROPERTY()
	//FTimespan Timespan;

	UPROPERTY()
	FVector Vector;

	UPROPERTY()
	FVector4 Vector4;

	UPROPERTY()
	FRotator Rotator;

	UPROPERTY()
	FQuat Quat;

	UPROPERTY()
	FColor Color;

	/** Default constructor. */
	FStructSerializerBuiltinTestStruct()
		: Guid(FGuid::NewGuid())
		, Name(TEXT("Test FName"))
		, String("Test String")
		, Text(FText::FromString("Test Text"))
		, Vector(1.0f, 2.0f, 3.0f)
		, Vector4(4.0f, 5.0f, 6.0f, 7.0f)
		, Rotator(4096, 8192, 16384)
		, Quat(1.0f, 2.0f, 3.0f, 0.46f)
		, Color(3, 255, 60, 255)
	{ }

	/** Creates an uninitialized instance. */
	FStructSerializerBuiltinTestStruct( ENoInit ) { }

	bool operator==(const FStructSerializerBuiltinTestStruct& Rhs) const
	{
		return Guid == Rhs.Guid && Name == Rhs.Name && String == Rhs.String && Text.EqualTo(Rhs.Text) && Vector == Rhs.Vector && Vector4 == Rhs.Vector4 && Rotator == Rhs.Rotator && Quat == Rhs.Quat && Color == Rhs.Color;
	}
};

// basic type hash to test built in struct in sets
FORCEINLINE uint32 GetTypeHash(const FStructSerializerBuiltinTestStruct& S)
{
	return GetTypeHash(S.String);
}

/**
 * Test structure for byte array properties.
 */
USTRUCT()
struct FStructSerializerByteArray
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Dummy1; // To test 'skip array'.

	UPROPERTY()
	TArray<uint8> ByteArray;

	UPROPERTY()
	int32 Dummy2; // To test 'skip array'.

	UPROPERTY()
	TArray<int8> Int8Array;

	UPROPERTY()
	int32 Dummy3; // To test 'skip array'.

	FStructSerializerByteArray()
	{
		Dummy1 = 1;
		Dummy2 = 2;
		Dummy3 = 3;

		ByteArray.Add(0);
		ByteArray.Add(127);
		ByteArray.Add(255);

		Int8Array.Add(-128);
		Int8Array.Add(0);
		Int8Array.Add(127);
	}

	FStructSerializerByteArray(ENoInit) { }
};


/**
 * Test structure for array properties.
 */
USTRUCT()
struct FStructSerializerArrayTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Int32Array;

	UPROPERTY()
	TArray<uint8> ByteArray;

	UPROPERTY()
	int32 StaticSingleElement[1];

	UPROPERTY()
	int32 StaticInt32Array[3];

	UPROPERTY()
	float StaticFloatArray[3];

	UPROPERTY()
	TArray<FVector> VectorArray;

	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	TArray<FStructSerializerBuiltinTestStruct> StructArray;

	/** Default constructor. */
	FStructSerializerArrayTestStruct()
	{
		Int32Array.Add(-1);
		Int32Array.Add(0);
		Int32Array.Add(1);

		ByteArray.Add(0);
		ByteArray.Add(127);
		ByteArray.Add(255);

		StaticSingleElement[0] = 42;

		StaticInt32Array[0] = -1;
		StaticInt32Array[1] = 0;
		StaticInt32Array[2] = 1;

		StaticFloatArray[0] = -1.0f;
		StaticFloatArray[1] = 0.0f;
		StaticFloatArray[2] = 1.0f;

		VectorArray.Add(FVector(1.0f, 2.0f, 3.0f));
		VectorArray.Add(FVector(-1.0f, -2.0f, -3.0f));

		StructArray.AddDefaulted(2);
	}

	/** Creates an uninitialized instance. */
	FStructSerializerArrayTestStruct( ENoInit ) { }
};


/**
 * Test structure for map properties.
 */
USTRUCT()
struct FStructSerializerMapTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<int32, FString> IntToStr;

	UPROPERTY()
	TMap<FString, FString> StrToStr;

	UPROPERTY()
	TMap<FString, FVector> StrToVec;

	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	TMap<FString, FStructSerializerBuiltinTestStruct> StrToStruct;

	/** Default constructor. */
	FStructSerializerMapTestStruct()
	{
		IntToStr.Add(1, TEXT("One"));
		IntToStr.Add(2, TEXT("Two"));
		IntToStr.Add(3, TEXT("Three"));

		StrToStr.Add(TEXT("StrAll"), TEXT("All"));
		StrToStr.Add(TEXT("StrYour"), TEXT("Your"));
		StrToStr.Add(TEXT("StrBase"), TEXT("Base"));

		StrToVec.Add(TEXT("V000"), FVector(0.0f, 0.0f, 0.0f));
		StrToVec.Add(TEXT("V123"), FVector(1.0f, 2.0f, 3.0f));
		StrToVec.Add(TEXT("V666"), FVector(6.0f, 6.0f, 6.0f));

		StrToStruct.Add(TEXT("StructOne"), FStructSerializerBuiltinTestStruct());
		StrToStruct.Add(TEXT("StructTwo"), FStructSerializerBuiltinTestStruct());
	}

	/** Creates an uninitialized instance. */
	FStructSerializerMapTestStruct( ENoInit ) { }
};

/**
 * Test structure for set properties.
 */
USTRUCT()
struct FStructSerializerSetTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	TSet<FString> StrSet;

	UPROPERTY()
	TSet<int32> IntSet;

	UPROPERTY()
	TSet<FName> NameSet;

	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	TSet<FStructSerializerBuiltinTestStruct> StructSet;

	/** Default constructor. */
	FStructSerializerSetTestStruct()
	{
		IntSet.Add(1);
		IntSet.Add(2);
		IntSet.Add(3);

		StrSet.Add(TEXT("Are"));
		StrSet.Add(TEXT("Belong"));
		StrSet.Add(TEXT("To"));
		StrSet.Add(TEXT("Us"));

		NameSet.Add(TEXT("Make"));
		NameSet.Add(TEXT("Your"));
		NameSet.Add(TEXT("Time"));

		StructSet.Add(FStructSerializerBuiltinTestStruct());
	}

	/** Creates an uninitialized instance. */
	FStructSerializerSetTestStruct( ENoInit ) { }
};



/**
 * Test structure for all supported types.
 */
USTRUCT()
struct FStructSerializerTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	FStructSerializerNumericTestStruct Numerics;

	UPROPERTY()
	FStructSerializerBooleanTestStruct Booleans;

	UPROPERTY()
	FStructSerializerObjectTestStruct Objects;

	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	FStructSerializerBuiltinTestStruct Builtins;

	UPROPERTY()
	FStructSerializerArrayTestStruct Arrays;

	UPROPERTY()
	FStructSerializerMapTestStruct Maps;

	UPROPERTY()
	FStructSerializerSetTestStruct Sets;

	/** Default constructor. */
	FStructSerializerTestStruct() = default;

	/** Creates an uninitialized instance. */
	FStructSerializerTestStruct( ENoInit )
		: Numerics(NoInit)
		, Booleans(NoInit)
		, Objects(NoInit)
		, Builtins(NoInit)
		, Arrays(NoInit)
		, Maps(NoInit)
		, Sets(NoInit)
	{ }
};
