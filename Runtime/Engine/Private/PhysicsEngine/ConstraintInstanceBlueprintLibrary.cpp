// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/ConstraintInstanceBlueprintLibrary.h"

//---------------------------------------------------------------------------------------------------
//
// CONSTRAINT BEHAVIOR 
//
//---------------------------------------------------------------------------------------------------

void UConstraintInstanceBlueprintLibrary::SetDisableCollision(
	FConstraintInstanceAccessor& Accessor,
	bool bDisableCollision
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetDisableCollision(bDisableCollision);
	}
}

bool UConstraintInstanceBlueprintLibrary::GetDisableCollsion(FConstraintInstanceAccessor& Accessor)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		return ConstraintInstance->IsCollisionDisabled();
	}
	return true;
}

void UConstraintInstanceBlueprintLibrary::SetProjectionParams(
	FConstraintInstanceAccessor& Accessor,
	bool bEnableProjection,
	float ProjectionLinearAlpha,
	float ProjectionAngularAlpha
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetProjectionParams(bEnableProjection, ProjectionLinearAlpha, ProjectionAngularAlpha);
	}
}

void UConstraintInstanceBlueprintLibrary::GetProjectionParams(
	FConstraintInstanceAccessor& Accessor,
	bool& bEnableProjection,
	float& ProjectionLinearAlpha,
	float& ProjectionAngularAlpha
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		bEnableProjection = ConstraintInstance->IsProjectionEnabled();
		ConstraintInstance->GetProjectionAlphasOrTolerances(ProjectionLinearAlpha, ProjectionAngularAlpha);
	}
	else
	{
		bEnableProjection = false;
		ProjectionLinearAlpha = 0.0f;
		ProjectionAngularAlpha = 0.0f;
	}
}

void UConstraintInstanceBlueprintLibrary::SetParentDominates(
	FConstraintInstanceAccessor& Accessor,
	bool bParentDominates
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		if (bParentDominates)
		{
			ConstraintInstance->EnableParentDominates();
		}
		else
		{
			ConstraintInstance->DisableParentDominates();
		}
	}
}

bool UConstraintInstanceBlueprintLibrary::GetParentDominates(FConstraintInstanceAccessor& Accessor)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		return ConstraintInstance->IsParentDominatesEnabled();
	}
	return false;
}

//---------------------------------------------------------------------------------------------------
//
// LINEAR LIMITS
//
//---------------------------------------------------------------------------------------------------

void UConstraintInstanceBlueprintLibrary::SetLinearLimits(
	FConstraintInstanceAccessor& Accessor,
	TEnumAsByte<ELinearConstraintMotion> XMotion,
	TEnumAsByte<ELinearConstraintMotion> YMotion,
	TEnumAsByte<ELinearConstraintMotion> ZMotion,
	float Limit
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetLinearLimits(XMotion, YMotion, ZMotion, Limit);
	}
}

void UConstraintInstanceBlueprintLibrary::GetLinearLimits(
	FConstraintInstanceAccessor& Accessor,
	TEnumAsByte<ELinearConstraintMotion>& XMotion,
	TEnumAsByte<ELinearConstraintMotion>& YMotion,
	TEnumAsByte<ELinearConstraintMotion>& ZMotion,
	float& Limit
)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		XMotion = ConstraintInstance->GetLinearXMotion();
		YMotion = ConstraintInstance->GetLinearYMotion();
		ZMotion = ConstraintInstance->GetLinearZMotion();
		Limit = ConstraintInstance->GetLinearLimit();
	} 
	else
	{
		XMotion = ELinearConstraintMotion::LCM_Free;
		YMotion = ELinearConstraintMotion::LCM_Free;
		ZMotion = ELinearConstraintMotion::LCM_Free;
		Limit = 0.f;
	}
}

void UConstraintInstanceBlueprintLibrary::SetLinearBreakable(
	FConstraintInstanceAccessor& Accessor,
	bool bLinearBreakable,
	float LinearBreakThreshold
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetLinearBreakable(bLinearBreakable, LinearBreakThreshold);
	}
}

void UConstraintInstanceBlueprintLibrary::GetLinearBreakable(
	FConstraintInstanceAccessor& Accessor,
	bool& bLinearBreakable,
	float& LinearBreakThreshold
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		bLinearBreakable = ConstraintInstance->IsLinearBreakable();
		LinearBreakThreshold = ConstraintInstance->GetLinearBreakThreshold();
	}
	else
	{
		bLinearBreakable = false;
		LinearBreakThreshold = 0.0f;
	}
}

//---------------------------------------------------------------------------------------------------
//
// ANGULAR LIMITS 
//
//---------------------------------------------------------------------------------------------------

void UConstraintInstanceBlueprintLibrary::SetAngularLimits(
	FConstraintInstanceAccessor& Accessor,
	TEnumAsByte<EAngularConstraintMotion> Swing1MotionType,
	float Swing1LimitAngle,
	TEnumAsByte<EAngularConstraintMotion> Swing2MotionType,
	float Swing2LimitAngle,
	TEnumAsByte<EAngularConstraintMotion> TwistMotionType,
	float TwistLimitAngle
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularSwing1Limit(Swing1MotionType, Swing1LimitAngle);
		ConstraintInstance->SetAngularSwing2Limit(Swing2MotionType, Swing2LimitAngle);
		ConstraintInstance->SetAngularTwistLimit(TwistMotionType, TwistLimitAngle);
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularLimits(
	FConstraintInstanceAccessor& Accessor,
	TEnumAsByte<EAngularConstraintMotion>& Swing1MotionType,
	float& Swing1LimitAngle,
	TEnumAsByte<EAngularConstraintMotion>& Swing2MotionType,
	float& Swing2LimitAngle,
	TEnumAsByte<EAngularConstraintMotion>& TwistMotionType,
	float& TwistLimitAngle
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		Swing1MotionType = ConstraintInstance->GetAngularSwing1Motion();
		Swing1LimitAngle = ConstraintInstance->GetAngularSwing1Limit();
		Swing2MotionType = ConstraintInstance->GetAngularSwing2Motion();
		Swing2LimitAngle = ConstraintInstance->GetAngularSwing2Limit();
		TwistMotionType = ConstraintInstance->GetAngularTwistMotion();
		TwistLimitAngle = ConstraintInstance->GetAngularTwistLimit();
	} 
	else
	{
		Swing1MotionType = EAngularConstraintMotion::ACM_Free;
		Swing1LimitAngle = 0.0f;
		Swing2MotionType = EAngularConstraintMotion::ACM_Free;
		Swing2LimitAngle = 0.0f;
		TwistMotionType = EAngularConstraintMotion::ACM_Free;
		TwistLimitAngle = 0.0f;
	}
}

void UConstraintInstanceBlueprintLibrary::SetAngularBreakable(
	FConstraintInstanceAccessor& Accessor,
	bool bAngularBreakable,
	float AngularBreakThreshold
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularBreakable(bAngularBreakable, AngularBreakThreshold);
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularBreakable(
	FConstraintInstanceAccessor& Accessor,
	bool& bAngularBreakable,
	float& AngularBreakThreshold
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		bAngularBreakable = ConstraintInstance->IsAngularBreakable();
		AngularBreakThreshold = ConstraintInstance->GetAngularBreakThreshold();
	}
	else
	{
		bAngularBreakable = false;
		AngularBreakThreshold = 0.0f;
	}

}

void UConstraintInstanceBlueprintLibrary::SetAngularPlasticity(
	FConstraintInstanceAccessor& Accessor,
	bool bAngularPlasticity,
	float AngularPlasticityThreshold
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularPlasticity(bAngularPlasticity, AngularPlasticityThreshold);
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularPlasticity(
	FConstraintInstanceAccessor& Accessor,
	bool& bAngularPlasticity,
	float& AngularPlasticityThreshold
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		bAngularPlasticity = ConstraintInstance->HasAngularPlasticity();
		AngularPlasticityThreshold = ConstraintInstance->GetAngularPlasticityThreshold();
	}
	else
	{
		bAngularPlasticity = false;
		AngularPlasticityThreshold = 0.0f;
	}
}


//---------------------------------------------------------------------------------------------------
//
// LINEAR MOTOR
//
//---------------------------------------------------------------------------------------------------

void UConstraintInstanceBlueprintLibrary::SetLinearPositionDrive(
	FConstraintInstanceAccessor& Accessor, 
	bool bEnableDriveX, 
	bool bEnableDriveY, 
	bool bEnableDriveZ
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetLinearPositionDrive(bEnableDriveX, bEnableDriveY, bEnableDriveZ);
	}
}

void UConstraintInstanceBlueprintLibrary::GetLinearPositionDrive(
	FConstraintInstanceAccessor& Accessor,
	bool& bEnableDriveX,
	bool& bEnableDriveY,
	bool& bEnableDriveZ
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		bEnableDriveX = ConstraintInstance->IsLinearPositionDriveXEnabled();
		bEnableDriveY = ConstraintInstance->IsLinearPositionDriveYEnabled();
		bEnableDriveZ = ConstraintInstance->IsLinearPositionDriveZEnabled();
	}
	else
	{
		bEnableDriveX = false;
		bEnableDriveY = false;
		bEnableDriveZ = false;
	}
}

void UConstraintInstanceBlueprintLibrary::SetLinearVelocityDrive(
	FConstraintInstanceAccessor& Accessor,
	bool bEnableDriveX, 
	bool bEnableDriveY, 
	bool bEnableDriveZ
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetLinearVelocityDrive(bEnableDriveX, bEnableDriveY, bEnableDriveZ);
	}
}

void UConstraintInstanceBlueprintLibrary::GetLinearVelocityDrive(
	FConstraintInstanceAccessor& Accessor,
	bool& bEnableDriveX,
	bool& bEnableDriveY,
	bool& bEnableDriveZ
)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		bEnableDriveX = ConstraintInstance->IsLinearVelocityDriveXEnabled();
		bEnableDriveY = ConstraintInstance->IsLinearVelocityDriveYEnabled();
		bEnableDriveZ = ConstraintInstance->IsLinearVelocityDriveZEnabled();
	}
	else
	{
		bEnableDriveX = false;
		bEnableDriveY = false;
		bEnableDriveZ = false;
	}
}

void UConstraintInstanceBlueprintLibrary::SetLinearPositionTarget(
	FConstraintInstanceAccessor& Accessor, 
	const FVector& InPosTarget
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetLinearPositionTarget(InPosTarget);
	}
}

void UConstraintInstanceBlueprintLibrary::GetLinearPositionTarget(
	FConstraintInstanceAccessor& Accessor,
	FVector& OutPosTarget
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		OutPosTarget = ConstraintInstance->GetLinearPositionTarget();
	}
	else
	{
		OutPosTarget = FVector::ZeroVector;
	}
}


void UConstraintInstanceBlueprintLibrary::SetLinearVelocityTarget(
	FConstraintInstanceAccessor& Accessor, 
	const FVector& InVelTarget
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetLinearVelocityTarget(InVelTarget);
	}
}

void UConstraintInstanceBlueprintLibrary::GetLinearVelocityTarget(
	FConstraintInstanceAccessor& Accessor,
	FVector& OutVelTarget
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		OutVelTarget = ConstraintInstance->GetLinearVelocityTarget();
	}
	else
	{
		OutVelTarget = FVector::ZeroVector;
	}
}

void UConstraintInstanceBlueprintLibrary::SetLinearDriveParams(
	FConstraintInstanceAccessor& Accessor,
	float PositionStrength, 
	float VelocityStrength, 
	float InForceLimit
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetLinearDriveParams(PositionStrength, VelocityStrength, InForceLimit);
	}
}

void UConstraintInstanceBlueprintLibrary::GetLinearDriveParams(
	FConstraintInstanceAccessor& Accessor,
	float& OutPositionStrength,
	float& OutVelocityStrength,
	float& OutForceLimit
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->GetLinearDriveParams(OutPositionStrength, OutVelocityStrength, OutForceLimit);
	}
	else
	{
		OutPositionStrength = 0.0f;
		OutVelocityStrength = 0.0f;
		OutForceLimit = 0.0f;
	}
}


//---------------------------------------------------------------------------------------------------
//
// ANGULAR MOTOR
//
//---------------------------------------------------------------------------------------------------

void UConstraintInstanceBlueprintLibrary::SetOrientationDriveTwistAndSwing(
	FConstraintInstanceAccessor& Accessor, 
	bool bEnableTwistDrive, 
	bool bEnableSwingDrive
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetOrientationDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
	}
}

void UConstraintInstanceBlueprintLibrary::GetOrientationDriveTwistAndSwing(
	UPARAM(ref) FConstraintInstanceAccessor& Accessor,
	bool& bOutEnableTwistDrive,
	bool& bOutEnableSwingDrive
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->GetOrientationDriveTwistAndSwing(bOutEnableTwistDrive, bOutEnableSwingDrive);
	}
	else
	{
		bOutEnableTwistDrive = false;
		bOutEnableSwingDrive = false;
	}
}

void UConstraintInstanceBlueprintLibrary::SetOrientationDriveSLERP(
	FConstraintInstanceAccessor& Accessor, 
	bool bEnableSLERP
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetOrientationDriveSLERP(bEnableSLERP);
	}
}

void UConstraintInstanceBlueprintLibrary::GetOrientationDriveSLERP(
	FConstraintInstanceAccessor& Accessor,
	bool& bOutEnableSLERP
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		bOutEnableSLERP = ConstraintInstance->GetOrientationDriveSLERP();
	}
	else
	{
		bOutEnableSLERP = false;
	}
}

void UConstraintInstanceBlueprintLibrary::SetAngularVelocityDriveTwistAndSwing(
	FConstraintInstanceAccessor& Accessor, 
	bool bEnableTwistDrive, 
	bool bEnableSwingDrive
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularVelocityDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularVelocityDriveTwistAndSwing(
	FConstraintInstanceAccessor& Accessor,
	bool& bOutEnableTwistDrive,
	bool& bOutEnableSwingDrive
)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->GetAngularVelocityDriveTwistAndSwing(bOutEnableTwistDrive, bOutEnableSwingDrive);
	}
	else
	{
		bOutEnableTwistDrive = false;
		bOutEnableSwingDrive = false;
	}
}

void UConstraintInstanceBlueprintLibrary::SetAngularVelocityDriveSLERP(
	FConstraintInstanceAccessor& Accessor,
	bool bEnableSLERP
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularVelocityDriveSLERP(bEnableSLERP);
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularVelocityDriveSLERP(
	FConstraintInstanceAccessor& Accessor,
	bool& bOutEnableSLERP
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		bOutEnableSLERP = ConstraintInstance->GetAngularVelocityDriveSLERP();
	} 
	else
	{
		bOutEnableSLERP = false;
	}
}

void UConstraintInstanceBlueprintLibrary::SetAngularDriveMode(
	FConstraintInstanceAccessor& Accessor,
	EAngularDriveMode::Type DriveMode
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularDriveMode(DriveMode);
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularDriveMode(
	FConstraintInstanceAccessor& Accessor,
	TEnumAsByte<EAngularDriveMode::Type>& OutDriveMode
)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		OutDriveMode = ConstraintInstance->GetAngularDriveMode();
	} 
	else
	{
		OutDriveMode = EAngularDriveMode::Type::SLERP;
	}

}

void UConstraintInstanceBlueprintLibrary::SetAngularOrientationTarget(
	FConstraintInstanceAccessor& Accessor,
	const FRotator& InPosTarget
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularOrientationTarget(InPosTarget.Quaternion());
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularOrientationTarget(
	FConstraintInstanceAccessor& Accessor,
	FRotator& OutPosTarget
)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		OutPosTarget = ConstraintInstance->GetAngularOrientationTarget();
	}
	else
	{
		OutPosTarget = FRotator::ZeroRotator;
	}
}

void UConstraintInstanceBlueprintLibrary::SetAngularVelocityTarget(
	FConstraintInstanceAccessor& Accessor,
	const FVector& InVelTarget
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularVelocityTarget(InVelTarget);
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularVelocityTarget(
	FConstraintInstanceAccessor& Accessor,
	FVector& OutVelTarget
)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		OutVelTarget = ConstraintInstance->GetAngularVelocityTarget();
	}
	else
	{
		OutVelTarget = FVector::ZeroVector;
	}
}

void UConstraintInstanceBlueprintLibrary::SetAngularDriveParams(
	FConstraintInstanceAccessor& Accessor,
	float PositionStrength,
	float VelocityStrength,
	float InForceLimit
	)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->SetAngularDriveParams(PositionStrength, VelocityStrength, InForceLimit);
	}
}

void UConstraintInstanceBlueprintLibrary::GetAngularDriveParams(
	FConstraintInstanceAccessor& Accessor,
	float OutPositionStrength,
	float OutVelocityStrength,
	float OutForceLimit
)
{
	if (FConstraintInstance* ConstraintInstance = Accessor.Get())
	{
		ConstraintInstance->GetAngularDriveParams(OutPositionStrength, OutVelocityStrength, OutForceLimit);
	}
	else
	{
		OutPositionStrength = 0.0f;
		OutVelocityStrength = 0.0f;
		OutForceLimit = 0.0f;
	}
}