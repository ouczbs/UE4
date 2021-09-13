// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/CompactBinaryValidation.h"

#include "Misc/AutomationTest.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinaryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static constexpr EAutomationTestFlags::Type CompactBinaryValidationTestFlags =
	EAutomationTestFlags::Type(EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbValidateTest, "System.Core.Serialization.ValidateCompactBinary", CompactBinaryValidationTestFlags)
bool FCbValidateTest::RunTest(const FString& Parameters)
{
	auto Validate = [this](std::initializer_list<uint8> Data, ECbFieldType Type = ECbFieldType::HasFieldType) -> ECbValidateError
	{
		return ValidateCompactBinary(MakeMemoryView(Data), ECbValidateMode::All, Type);
	};
	auto ValidateMode = [this](std::initializer_list<uint8> Data, ECbValidateMode Mode, ECbFieldType Type = ECbFieldType::HasFieldType) -> ECbValidateError
	{
		return ValidateCompactBinary(MakeMemoryView(Data), Mode, Type);
	};

	auto AddName = [](ECbFieldType Type) -> uint8 { return uint8(Type | ECbFieldType::HasFieldName); };

	constexpr uint8 NullNoName = uint8(ECbFieldType::Null);
	constexpr uint8 NullWithName = uint8(ECbFieldType::Null | ECbFieldType::HasFieldName);
	constexpr uint8 IntNoName = uint8(ECbFieldType::IntegerPositive);
	constexpr uint8 IntWithName = uint8(ECbFieldType::IntegerPositive | ECbFieldType::HasFieldName);

	// Test OutOfBounds
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Empty)"), Validate({}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Empty, Mode)"), ValidateMode({}, ECbValidateMode::None), ECbValidateError::None);

	TestEqual(TEXT("ValidateCompactBinary(Valid, Null)"), Validate({NullNoName}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Null, Name)"), Validate({NullWithName, 1, 'N'}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Null, Name)"), Validate({NullWithName}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Null, Name)"), Validate({NullWithName, 1}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Null, Name)"), Validate({NullWithName, 0x80}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Null, Name)"), Validate({NullWithName, 0x80, 128}), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, Object, Empty)"), Validate({uint8(ECbFieldType::Object), 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Object, Empty, NoType)"), Validate({0}, ECbFieldType::Object), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Object, Field)"), Validate({uint8(ECbFieldType::Object), 7, NullWithName, 1, 'N', IntWithName, 1, 'I', 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Object, Field, NoType)"), Validate({7, NullWithName, 1, 'N', IntWithName, 1, 'I', 0}, ECbFieldType::Object), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Object)"), Validate({uint8(ECbFieldType::Object)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Object, NoType)"), Validate({}, ECbFieldType::Object), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Object)"), Validate({uint8(ECbFieldType::Object), 1}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Object, NoType)"), Validate({1}, ECbFieldType::Object), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Object, OOB Field)"), Validate({uint8(ECbFieldType::Object), 3, AddName(ECbFieldType::Float32), 1, 'N'}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Object, OOB Field, NoType)"), Validate({3, AddName(ECbFieldType::Float32), 1, 'N'}, ECbFieldType::Object), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, UniformObject, Field)"), Validate({uint8(ECbFieldType::UniformObject), 3, NullWithName, 1, 'N'}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, UniformObject, Field, NoType)"), Validate({3, NullWithName, 1, 'N'}, ECbFieldType::UniformObject), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformObject)"), Validate({uint8(ECbFieldType::UniformObject)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformObject, NoType)"), Validate({}, ECbFieldType::UniformObject), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformObject)"), Validate({uint8(ECbFieldType::UniformObject), 1}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformObject, NoType)"), Validate({1}, ECbFieldType::UniformObject), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformObject, OOB Field)"), Validate({uint8(ECbFieldType::UniformObject), 3, AddName(ECbFieldType::Float32), 1, 'N'}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformObject, OOB Field, NoType)"), Validate({3, AddName(ECbFieldType::Float32), 1, 'N'}, ECbFieldType::UniformObject), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, Array, Empty)"), Validate({uint8(ECbFieldType::Array), 1, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Array, Empty, NoType)"), Validate({1, 0}, ECbFieldType::Array), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Array, Field)"), Validate({uint8(ECbFieldType::Array), 4, 2, NullNoName, IntNoName, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Array, Field, NoType)"), Validate({4, 2, NullNoName, IntNoName, 0}, ECbFieldType::Array), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Array)"), Validate({uint8(ECbFieldType::Array)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Array, NoType)"), Validate({}, ECbFieldType::Array), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Array)"), Validate({uint8(ECbFieldType::Array), 1}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Array, NoType)"), Validate({1}, ECbFieldType::Array), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Array, OOB Field)"), Validate({uint8(ECbFieldType::Array), 2, 1, uint8(ECbFieldType::Float32)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Array, OOB Field, NoType)"), Validate({2, 1, uint8(ECbFieldType::Float32)}, ECbFieldType::Array), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, UniformArray, Field)"), Validate({uint8(ECbFieldType::UniformArray), 3, 1, IntNoName, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, UniformArray, Field, NoType)"), Validate({3, 1, IntNoName, 0}, ECbFieldType::UniformArray), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformArray)"), Validate({uint8(ECbFieldType::UniformArray)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformArray, NoType)"), Validate({}, ECbFieldType::UniformArray), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformArray)"), Validate({uint8(ECbFieldType::UniformArray), 1}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformArray, NoType)"), Validate({1}, ECbFieldType::UniformArray), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformArray, OOB Field)"), Validate({uint8(ECbFieldType::UniformArray), 2, 1, uint8(ECbFieldType::Float32)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, UniformArray, OOB Field, NoType)"), Validate({2, 1, uint8(ECbFieldType::Float32)}, ECbFieldType::UniformArray), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, Binary, Empty)"), Validate({uint8(ECbFieldType::Binary), 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Binary, Empty, NoType)"), Validate({0}, ECbFieldType::Binary), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Binary, Field)"), Validate({uint8(ECbFieldType::Binary), 1, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Binary, Field, NoType)"), Validate({1, 0}, ECbFieldType::Binary), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Binary)"), Validate({uint8(ECbFieldType::Binary)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Binary, NoType)"), Validate({}, ECbFieldType::Binary), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Binary)"), Validate({uint8(ECbFieldType::Binary), 1}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Binary, NoType)"), Validate({1}, ECbFieldType::Binary), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, String, Empty)"), Validate({uint8(ECbFieldType::String), 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, String, Empty, NoType)"), Validate({0}, ECbFieldType::String), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, String, Field)"), Validate({uint8(ECbFieldType::String), 1, 'S'}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, String, Field, NoType)"), Validate({1, 'S'}, ECbFieldType::String), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, String)"), Validate({uint8(ECbFieldType::String)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, String, NoType)"), Validate({}, ECbFieldType::String), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, String)"), Validate({uint8(ECbFieldType::String), 1}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, String, NoType)"), Validate({1}, ECbFieldType::String), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, IntegerPositive, 1-byte)"), Validate({uint8(ECbFieldType::IntegerPositive), 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, IntegerPositive, 1-byte, NoType)"), Validate({0}, ECbFieldType::IntegerPositive), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, IntegerPositive, 2-byte)"), Validate({uint8(ECbFieldType::IntegerPositive), 0x80, 0x80}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, IntegerPositive, 2-byte, NoType)"), Validate({0x80, 0x80}, ECbFieldType::IntegerPositive), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerPositive, 1-byte)"), Validate({uint8(ECbFieldType::IntegerPositive)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerPositive, 1-byte, NoType)"), Validate({}, ECbFieldType::IntegerPositive), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerPositive, 2-byte)"), Validate({uint8(ECbFieldType::IntegerPositive), 0x80}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerPositive, 2-byte, NoType)"), Validate({0x80}, ECbFieldType::IntegerPositive), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerPositive, 9-byte)"), Validate({uint8(ECbFieldType::IntegerPositive), 0xff, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerPositive, 9-byte, NoType)"), Validate({0xff, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::IntegerPositive), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, IntegerNegative, 1-byte)"), Validate({uint8(ECbFieldType::IntegerNegative), 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, IntegerNegative, 1-byte, NoType)"), Validate({0}, ECbFieldType::IntegerNegative), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, IntegerNegative, 2-byte)"), Validate({uint8(ECbFieldType::IntegerNegative), 0x80, 0x80}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, IntegerNegative, 2-byte, NoType)"), Validate({0x80, 0x80}, ECbFieldType::IntegerNegative), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerNegative, 1-byte)"), Validate({uint8(ECbFieldType::IntegerNegative)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerNegative, 1-byte, NoType)"), Validate({}, ECbFieldType::IntegerNegative), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerNegative, 2-byte)"), Validate({uint8(ECbFieldType::IntegerNegative), 0x80}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerNegative, 2-byte, NoType)"), Validate({0x80}, ECbFieldType::IntegerNegative), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerNegative, 9-byte)"), Validate({uint8(ECbFieldType::IntegerNegative), 0xff, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, IntegerNegative, 9-byte, NoType)"), Validate({0xff, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::IntegerNegative), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, Float32)"), Validate({uint8(ECbFieldType::Float32), 0, 0, 0, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Float32, NoType)"), Validate({0, 0, 0, 0}, ECbFieldType::Float32), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Float32)"), Validate({uint8(ECbFieldType::Float32), 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Float32, NoType)"), Validate({0, 0, 0}, ECbFieldType::Float32), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, Float64)"), Validate({uint8(ECbFieldType::Float64), 0x3f, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Float64, NoType)"), Validate({0x3f, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00}, ECbFieldType::Float64), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Float64)"), Validate({uint8(ECbFieldType::Float64), 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Float64, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0}, ECbFieldType::Float64), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, BoolFalse)"), Validate({uint8(ECbFieldType::BoolFalse)}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, BoolTrue)"), Validate({uint8(ECbFieldType::BoolTrue)}), ECbValidateError::None);

	TestEqual(TEXT("ValidateCompactBinary(Valid, CompactBinaryAttachment)"), Validate({uint8(ECbFieldType::CompactBinaryAttachment), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, CompactBinaryAttachment, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::CompactBinaryAttachment), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, CompactBinaryAttachment)"), Validate({uint8(ECbFieldType::CompactBinaryAttachment), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, CompactBinaryAttachment, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::CompactBinaryAttachment), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, BinaryAttachment)"), Validate({uint8(ECbFieldType::BinaryAttachment), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, BinaryAttachment, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::BinaryAttachment), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, BinaryAttachment)"), Validate({uint8(ECbFieldType::BinaryAttachment), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, BinaryAttachment, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::BinaryAttachment), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, Hash)"), Validate({uint8(ECbFieldType::Hash), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Hash, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::Hash), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Hash)"), Validate({uint8(ECbFieldType::Hash), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Hash, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::Hash), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, Uuid)"), Validate({uint8(ECbFieldType::Uuid), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, Uuid, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::Uuid), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Uuid)"), Validate({uint8(ECbFieldType::Uuid), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, Uuid, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::Uuid), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, DateTime)"), Validate({uint8(ECbFieldType::DateTime), 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, DateTime, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::DateTime), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, DateTime)"), Validate({uint8(ECbFieldType::DateTime), 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, DateTime, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0}, ECbFieldType::DateTime), ECbValidateError::OutOfBounds);

	TestEqual(TEXT("ValidateCompactBinary(Valid, TimeSpan)"), Validate({uint8(ECbFieldType::TimeSpan), 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Valid, TimeSpan, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0, 0}, ECbFieldType::TimeSpan), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, TimeSpan)"), Validate({uint8(ECbFieldType::TimeSpan), 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinary(OutOfBounds, TimeSpan, NoType)"), Validate({0, 0, 0, 0, 0, 0, 0}, ECbFieldType::TimeSpan), ECbValidateError::OutOfBounds);

	// Test InvalidType
	TestEqual(TEXT("ValidateCompactBinary(InvalidType, Unknown)"), Validate({uint8(ECbFieldType::TimeSpan) + 1}), ECbValidateError::InvalidType);
	TestEqual(TEXT("ValidateCompactBinary(InvalidType, Unknown)"), Validate({}, ECbFieldType(uint8(ECbFieldType::TimeSpan) + 1)), ECbValidateError::InvalidType);
	TestEqual(TEXT("ValidateCompactBinary(InvalidType, HasFieldType)"), Validate({uint8(ECbFieldType::Null | ECbFieldType::HasFieldType)}), ECbValidateError::InvalidType);

	TestEqual(TEXT("ValidateCompactBinary(InvalidType, ZeroSizeField)"), Validate({}, ECbFieldType::Null), ECbValidateError::InvalidType);
	TestEqual(TEXT("ValidateCompactBinary(InvalidType, ZeroSizeField, BoolFalse)"), Validate({}, ECbFieldType::BoolFalse), ECbValidateError::InvalidType);
	TestEqual(TEXT("ValidateCompactBinary(InvalidType, ZeroSizeField, BoolTrue)"), Validate({}, ECbFieldType::BoolTrue), ECbValidateError::InvalidType);

	TestEqual(TEXT("ValidateCompactBinary(InvalidType, ZeroSizeField, Array)"), Validate({uint8(ECbFieldType::UniformArray), 2, 2, NullNoName}), ECbValidateError::InvalidType);
	TestEqual(TEXT("ValidateCompactBinary(InvalidType, ZeroSizeField, Object)"), Validate({uint8(ECbFieldType::UniformObject), 2, NullNoName, 0}), ECbValidateError::InvalidType);

	// Test DuplicateName
	TestEqual(TEXT("ValidateCompactBinary(DuplicateName)"), Validate({uint8(ECbFieldType::UniformObject), 7, NullWithName, 1, 'A', 1, 'B', 1, 'A'}), ECbValidateError::DuplicateName);
	TestEqual(TEXT("ValidateCompactBinary(DuplicateName, CaseSensitive)"), Validate({uint8(ECbFieldType::UniformObject), 7, NullWithName, 1, 'A', 1, 'B', 1, 'a'}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(DuplicateName, Mode)"), ValidateMode({uint8(ECbFieldType::UniformObject), 7, NullWithName, 1, 'A', 1, 'B', 1, 'A'}, ECbValidateMode::All & ~ECbValidateMode::Names), ECbValidateError::None);

	// Test MissingName
	TestEqual(TEXT("ValidateCompactBinary(MissingName)"), Validate({uint8(ECbFieldType::Object), 3, NullNoName, IntNoName, 0}), ECbValidateError::MissingName);
	TestEqual(TEXT("ValidateCompactBinary(MissingName, Uniform)"), Validate({uint8(ECbFieldType::UniformObject), 3, IntNoName, 0, 0}), ECbValidateError::MissingName);
	TestEqual(TEXT("ValidateCompactBinary(MissingName, Mode)"), ValidateMode({uint8(ECbFieldType::Object), 3, NullNoName, IntNoName, 0}, ECbValidateMode::All & ~ECbValidateMode::Names), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(MissingName, Uniform, Mode)"), ValidateMode({uint8(ECbFieldType::UniformObject), 3, IntNoName, 0, 0}, ECbValidateMode::All & ~ECbValidateMode::Names), ECbValidateError::None);

	// Test ArrayName
	TestEqual(TEXT("ValidateCompactBinary(ArrayName)"), Validate({uint8(ECbFieldType::Array), 5, 2, NullNoName, NullWithName, 1, 'F'}), ECbValidateError::ArrayName);
	TestEqual(TEXT("ValidateCompactBinary(ArrayName, Uniform)"), Validate({uint8(ECbFieldType::UniformArray), 4, 1, NullWithName, 1, 'F'}), ECbValidateError::ArrayName);
	TestEqual(TEXT("ValidateCompactBinary(ArrayName, Mode)"), ValidateMode({uint8(ECbFieldType::Array), 5, 2, NullNoName, NullWithName, 1, 'F'}, ECbValidateMode::All & ~ECbValidateMode::Names), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(ArrayName, Uniform, Mode)"), ValidateMode({uint8(ECbFieldType::UniformArray), 4, 1, NullWithName, 1, 'F'}, ECbValidateMode::All & ~ECbValidateMode::Names), ECbValidateError::None);

	// Test InvalidString
	// Not tested or implemented yet because the engine does not provide enough UTF-8 functionality.

	// Test InvalidInteger
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, NameSize)"), Validate({NullWithName, 0x80, 1, 'N'}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, ObjectSize)"), Validate({uint8(ECbFieldType::Object), 0xc0, 0, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, ArraySize)"), Validate({uint8(ECbFieldType::Array), 0xe0, 0, 0, 1, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, ArrayCount)"), Validate({uint8(ECbFieldType::Array), 5, 0xf0, 0, 0, 0, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, BinarySize)"), Validate({uint8(ECbFieldType::Binary), 0xf8, 0, 0, 0, 0, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, StringSize)"), Validate({uint8(ECbFieldType::String), 0xfc, 0, 0, 0, 0, 0, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, IntegerPositive)"), Validate({uint8(ECbFieldType::IntegerPositive), 0xfe, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, IntegerNegative)"), Validate({uint8(ECbFieldType::IntegerNegative), 0xff, 0, 0, 0, 0, 0, 0, 0, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, ArraySize)"), Validate({uint8(ECbFieldType::Array), 0x80, 1, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, ArrayCount)"), Validate({uint8(ECbFieldType::Array), 3, 0xc0, 0, 0}), ECbValidateError::InvalidInteger);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, ObjectSize)"), Validate({uint8(ECbFieldType::Object), 0xe0, 0, 0, 0}), ECbValidateError::InvalidInteger);

	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, NameSize, Mode)"), ValidateMode({NullWithName, 0x80, 1, 'N'}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, ArraySize, Mode)"), ValidateMode({uint8(ECbFieldType::Array), 0xc0, 0, 1, 0}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(InvalidInteger, ObjectSize, Mode)"), ValidateMode({uint8(ECbFieldType::Object), 0xe0, 0, 0, 0}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None);

	// Test InvalidFloat
	TestEqual(TEXT("ValidateCompactBinary(InvalidFloat, MaxSignificant+1)"), Validate({uint8(ECbFieldType::Float64), 0x3f, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00}), ECbValidateError::None); // 1.9999999403953552
	TestEqual(TEXT("ValidateCompactBinary(InvalidFloat, MaxExponent+1)"), Validate({uint8(ECbFieldType::Float64), 0x47, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00}), ECbValidateError::None); // 6.8056469327705771e38
	TestEqual(TEXT("ValidateCompactBinary(InvalidFloat, MaxSignificand)"), Validate({uint8(ECbFieldType::Float64), 0x3f, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00}), ECbValidateError::InvalidFloat); // 1.9999998807907104
	TestEqual(TEXT("ValidateCompactBinary(InvalidFloat, MaxExponent)"), Validate({uint8(ECbFieldType::Float64), 0x47, 0xef, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00}), ECbValidateError::InvalidFloat); // 3.4028234663852886e38
	TestEqual(TEXT("ValidateCompactBinary(InvalidFloat, MaxSignificand, Mode)"), ValidateMode({uint8(ECbFieldType::Float64), 0x3f, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None); // 1.9999998807907104
	TestEqual(TEXT("ValidateCompactBinary(InvalidFloat, MaxExponent, Mode)"), ValidateMode({uint8(ECbFieldType::Float64), 0x47, 0xef, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None); // 3.4028234663852886e38

	// Test NonUniformObject
	TestEqual(TEXT("ValidateCompactBinary(NonUniformObject)"), Validate({uint8(ECbFieldType::Object), 3, NullWithName, 1, 'A'}), ECbValidateError::NonUniformObject);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformObject)"), Validate({uint8(ECbFieldType::Object), 6, NullWithName, 1, 'A', NullWithName, 1, 'B'}), ECbValidateError::NonUniformObject);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformObject, Mode)"), ValidateMode({uint8(ECbFieldType::Object), 3, NullWithName, 1, 'A'}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformObject, Mode)"), ValidateMode({uint8(ECbFieldType::Object), 6, NullWithName, 1, 'A', NullWithName, 1, 'B'}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None);

	// Test NonUniformArray
	TestEqual(TEXT("ValidateCompactBinary(NonUniformArray)"), Validate({uint8(ECbFieldType::Array), 3, 1, IntNoName, 0}), ECbValidateError::NonUniformArray);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformArray)"), Validate({uint8(ECbFieldType::Array), 5, 2, IntNoName, 1, IntNoName, 2}), ECbValidateError::NonUniformArray);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformArray, Null)"), Validate({uint8(ECbFieldType::Array), 3, 2, NullNoName, NullNoName}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformArray, Bool)"), Validate({uint8(ECbFieldType::Array), 3, 2, uint8(ECbFieldType::BoolFalse), uint8(ECbFieldType::BoolFalse)}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformArray, Bool)"), Validate({uint8(ECbFieldType::Array), 3, 2, uint8(ECbFieldType::BoolTrue), uint8(ECbFieldType::BoolTrue)}), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformArray, Mode)"), ValidateMode({uint8(ECbFieldType::Array), 3, 1, IntNoName, 0}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(NonUniformArray, Mode)"), ValidateMode({uint8(ECbFieldType::Array), 5, 2, IntNoName, 1, IntNoName, 2}, ECbValidateMode::All & ~ECbValidateMode::Format), ECbValidateError::None);

	// Test Padding
	TestEqual(TEXT("ValidateCompactBinary(Padding)"), Validate({NullNoName, 0}), ECbValidateError::Padding);
	TestEqual(TEXT("ValidateCompactBinary(Padding)"), Validate({uint8(ECbFieldType::Array), 1, 0, 0}), ECbValidateError::Padding);
	TestEqual(TEXT("ValidateCompactBinary(Padding)"), Validate({uint8(ECbFieldType::Object), 0, 0}), ECbValidateError::Padding);
	TestEqual(TEXT("ValidateCompactBinary(Padding, Mode)"), ValidateMode({NullNoName, 0}, ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Padding, Mode)"), ValidateMode({uint8(ECbFieldType::Array), 1, 0, 0}, ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);
	TestEqual(TEXT("ValidateCompactBinary(Padding, Mode)"), ValidateMode({uint8(ECbFieldType::Object), 0, 0}, ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbValidateRangeTest, "System.Core.Serialization.ValidateCompactBinaryRange", CompactBinaryValidationTestFlags)
bool FCbValidateRangeTest::RunTest(const FString& Parameters)
{
	auto Validate = [this](std::initializer_list<uint8> Data, ECbValidateMode Mode = ECbValidateMode::All) -> ECbValidateError
	{
		return ValidateCompactBinaryRange(MakeMemoryView(Data), Mode);
	};

	// Test Empty
	TestEqual(TEXT("ValidateCompactBinaryRange(Empty)"), Validate({}), ECbValidateError::None);

	// Test Valid
	TestEqual(TEXT("ValidateCompactBinaryRange(Null x2)"), Validate({uint8(ECbFieldType::Null), uint8(ECbFieldType::Null)}), ECbValidateError::None);

	// Test Padding
	TestEqual(TEXT("ValidateCompactBinaryRange(Padding InvalidType)"), Validate({uint8(ECbFieldType::Null), 0}), ECbValidateError::InvalidType);
	TestEqual(TEXT("ValidateCompactBinaryRange(Padding OutOfBounds)"), Validate({uint8(ECbFieldType::Null), uint8(ECbFieldType::Binary)}), ECbValidateError::OutOfBounds);
	TestEqual(TEXT("ValidateCompactBinaryRange(Padding OutOfBounds, Mode)"), Validate({uint8(ECbFieldType::Null), uint8(ECbFieldType::Binary)}, ECbValidateMode::None), ECbValidateError::None);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbValidateAttachmentTest, "System.Core.Serialization.ValidateCompactBinaryAttachment", CompactBinaryValidationTestFlags)
bool FCbValidateAttachmentTest::RunTest(const FString& Parameters)
{
	auto Validate = [this](std::initializer_list<uint8> Data, ECbValidateMode Mode = ECbValidateMode::All) -> ECbValidateError
	{
		return ValidateCompactBinaryAttachment(MakeMemoryView(Data), Mode);
	};

	const uint8 BinaryValue[] = { 0, 1, 2, 3 };
	const FMemoryView BinaryView = MakeMemoryView(BinaryValue);

	// Test Null
	{
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Null)"), Validate({uint8(ECbFieldType::Binary), 0}), ECbValidateError::None);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Null, Padding)"), Validate({uint8(ECbFieldType::Binary), 0, 0}), ECbValidateError::Padding);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Null, Padding, Mode)"), Validate({uint8(ECbFieldType::Binary), 0, 0}, ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);
	}

	// Test Binary
	{
		FBufferArchive Buffer;
		FCbAttachment(FSharedBuffer::MakeView(MakeMemoryView<uint8>({0}))).Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::None);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, MissingHash)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(3), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, MissingHash, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(3), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		++Buffer.Last();
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, InvalidHash)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageHash);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, InvalidHash, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		--Buffer.Last();
		Buffer.AddZeroed(1);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, Padding)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::Padding);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, Padding, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);
		Buffer.Pop(/*bAllowShrinking*/ false);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, OutOfBoundsValue)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(1), ECbValidateMode::All), ECbValidateError::OutOfBounds);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, OutOfBoundsHash)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(4), ECbValidateMode::All), ECbValidateError::OutOfBounds);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Binary, OutOfBoundsHash, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(4), ECbValidateMode::None), ECbValidateError::None);
	}

	// Test CompactBinary
	{
		FBufferArchive Buffer;
		FCbAttachment(FCbFieldRefIterator::MakeSingle(FCbFieldRef::MakeView(FCbField(nullptr, ECbFieldType::Null)))).Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::None);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, MissingHash)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(3), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, MissingHash, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(3), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		++Buffer.Last();
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, InvalidHash)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageHash);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, InvalidHash, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		--Buffer.Last();
		Buffer.AddZeroed(1);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, Padding)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::Padding);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, Padding, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);
		Buffer.Pop(/*bAllowShrinking*/ false);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, OutOfBoundsValue)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(1), ECbValidateMode::All), ECbValidateError::OutOfBounds);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, OutOfBoundsHash)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(4), ECbValidateMode::All), ECbValidateError::OutOfBounds);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(CompactBinary, OutOfBoundsHash, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer).Left(4), ECbValidateMode::None), ECbValidateError::None);
	}

	// Test InvalidPackageFormat
	{
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Empty)"), Validate({}), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Empty, Mode)"), Validate({}, ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		FCbFieldRefIterator::MakeSingle(FCbObjectRef().AsFieldRef()).CopyRangeTo(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Object)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(Object, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		FCbWriter Writer;
		Writer.CompactBinaryAttachment(FIoHash::HashBuffer(BinaryView));
		Writer.Binary(BinaryView);
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(HashBeforeValue)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat | ECbValidateError::Padding);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(HashBeforeValue, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::Padding);
	}
	{
		FBufferArchive Buffer;
		FCbWriter Writer;
		Writer.Name("Name"_ASV).Binary(BinaryView);
		Writer.CompactBinaryAttachment(FIoHash::HashBuffer(BinaryView));
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(NameOnValue)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(NameOnValue, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		FCbWriter Writer;
		Writer.Binary(BinaryView);
		Writer.Name("Name"_ASV).CompactBinaryAttachment(FIoHash::HashBuffer(BinaryView));
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(NameOnHash)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryAttachment(NameOnHash, Mode)"), ValidateCompactBinaryAttachment(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbValidatePackageTest, "System.Core.Serialization.ValidateCompactBinaryPackage", CompactBinaryValidationTestFlags)
bool FCbValidatePackageTest::RunTest(const FString& Parameters)
{
	const uint8 BinaryValue[] = { 0, 1, 2, 3 };
	const FMemoryView BinaryView = MakeMemoryView(BinaryValue);

	// Test Null
	{
		TestEqual(TEXT("ValidateCompactBinaryPackage(Null)"), ValidateCompactBinaryPackage(MakeMemoryView<uint8>({uint8(ECbFieldType::Null)}), ECbValidateMode::All), ECbValidateError::None);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Null, Padding)"), ValidateCompactBinaryPackage(MakeMemoryView<uint8>({uint8(ECbFieldType::Null), 0}), ECbValidateMode::All), ECbValidateError::Padding);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Null, Padding, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView<uint8>({uint8(ECbFieldType::Null), 0}), ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);
	}

	// Test Object
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.BeginObject();
		Writer << "Field" << 42;
		Writer.EndObject();
		Writer.CompactBinaryAttachment(Writer.Save().GetHash());
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::None);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, MissingHash)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).LeftChop(34), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, MissingHash, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).LeftChop(34), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, MissingNull)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).LeftChop(1), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, MissingNull, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).LeftChop(1), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		++Buffer.Last(1);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, InvalidHash)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageHash);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, InvalidHash, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		--Buffer.Last(1);
		Buffer.AddZeroed(1);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, Padding)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::Padding);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, Padding, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);
		Buffer.Pop(/*bAllowShrinking*/ false);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, OutOfBoundsValue)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).Left(1), ECbValidateMode::All), ECbValidateError::OutOfBounds);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, OutOfBoundsHash)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).Left(11), ECbValidateMode::All), ECbValidateError::OutOfBounds);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object, OutOfBoundsHash, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).Left(11), ECbValidateMode::None), ECbValidateError::None);
	}

	// Test Object + Attachment
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.BeginObject();
		Writer << "Field" << 42;
		Writer.EndObject();
		Writer.CompactBinaryAttachment(Writer.Save().GetHash());
		const uint64 AttachmentOffset = Writer.GetSaveSize();
		Writer.Binary(BinaryView);
		Writer.CompactBinaryAttachment(FIoHash::HashBuffer(BinaryView));
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::None);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, MissingHash)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).LeftChop(22), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, MissingHash, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).LeftChop(22), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, MissingNull)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).LeftChop(1), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, MissingNull, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).LeftChop(1), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		++Buffer.Last(1);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, InvalidHash)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageHash);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, InvalidHash, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
		--Buffer.Last(1);
		Buffer.AddZeroed(1);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, Padding)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::Padding);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, Padding, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Padding), ECbValidateError::None);
		Buffer.Pop(/*bAllowShrinking*/ false);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, OutOfBoundsValue)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).Left(AttachmentOffset + 1), ECbValidateMode::All), ECbValidateError::OutOfBounds);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, OutOfBoundsHash)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).Left(AttachmentOffset + 7), ECbValidateMode::All), ECbValidateError::OutOfBounds);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Object+Attachment, OutOfBoundsHash, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer).Left(AttachmentOffset + 7), ECbValidateMode::None), ECbValidateError::None);
	}

	// Test InvalidPackageFormat
	{
		TestEqual(TEXT("ValidateCompactBinaryPackage(Empty)"), ValidateCompactBinaryPackage(FMemoryView(), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryPackage(Empty, Mode)"), ValidateCompactBinaryPackage(FMemoryView(), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.Name("Name"_ASV);
		Writer.BeginObject();
		Writer << "Field" << 42;
		Writer.EndObject();
		Writer.CompactBinaryAttachment(Writer.Save().GetHash());
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NameOnObject)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NameOnObject, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.BeginObject();
		Writer << "Field" << 42;
		Writer.EndObject();
		const FIoHash ObjectHash = Writer.Save().GetHash();
		Writer.Name("Name"_ASV);
		Writer.CompactBinaryAttachment(ObjectHash);
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NameOnHash)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NameOnHash, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.BeginObject();
		Writer << "Field" << 42;
		Writer.EndObject();
		Writer.CompactBinaryAttachment(Writer.Save().GetHash());
		Writer.Name("Name"_ASV).Binary(BinaryView);
		Writer.CompactBinaryAttachment(FIoHash::HashBuffer(BinaryView));
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NameOnAttachment)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::InvalidPackageFormat);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NameOnAttachment, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.BeginObject();
		Writer << "Field" << 42;
		Writer.EndObject();
		const FIoHash ObjectHash = Writer.Save().GetHash();
		Writer.CompactBinaryAttachment(ObjectHash);
		Writer.BeginObject();
		Writer << "Field" << 42;
		Writer.EndObject();
		Writer.CompactBinaryAttachment(ObjectHash);
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(MultipleObjects)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::MultiplePackageObjects);
		TestEqual(TEXT("ValidateCompactBinaryPackage(MultipleObjects, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.Binary(BinaryView);
		Writer.CompactBinaryAttachment(FIoHash::HashBuffer(BinaryView));
		Writer.Binary(BinaryView);
		Writer.CompactBinaryAttachment(FIoHash::HashBuffer(BinaryView));
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(DuplicateAttachments)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::DuplicateAttachments);
		TestEqual(TEXT("ValidateCompactBinaryPackage(DuplicateAttachments, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.BeginObject();
		Writer.EndObject();
		Writer.CompactBinaryAttachment(FCbObject().GetHash());
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NullObject)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::NullPackageObject);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NullObject, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}
	{
		FBufferArchive Buffer;
		TCbWriter<256> Writer;
		Writer.Binary(FMemoryView());
		Writer.Null();
		Writer.Save(Buffer);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NullAttachment)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::NullPackageAttachment);
		TestEqual(TEXT("ValidateCompactBinaryPackage(NullAttachment, Mode)"), ValidateCompactBinaryPackage(MakeMemoryView(Buffer), ECbValidateMode::All & ~ECbValidateMode::Package), ECbValidateError::None);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // WITH_DEV_AUTOMATION_TESTS
