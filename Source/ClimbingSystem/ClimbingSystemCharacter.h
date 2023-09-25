// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "ClimbingSystemCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UCustomMovementComponent;
class UMotionWarpingComponent;
class UInputMappingContext;
class UInputAction;
UCLASS(config = Game)
class AClimbingSystemCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AClimbingSystemCharacter(const FObjectInitializer &ObjectInitializer);

private:
#pragma region Components

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent *CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent *FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	UCustomMovementComponent *CustomMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	UMotionWarpingComponent *MotionWarpingComponent;

#pragma endregion

#pragma region Input
	void OnPlayerEnterClimbState();
	void OnPlayerExitClimbState();
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext *DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction *JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction *MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction *LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction *ClimbAction;
#pragma endregion

#pragma region InputCallback
	/** Called for movement input */
	void Move(const FInputActionValue &Value);
	void HandleGroundMovementInput(const FInputActionValue &Value);
	void HandleClimbMovementInput(const FInputActionValue &Value);

	/** Called for looking input */
	void Look(const FInputActionValue &Value);

	void OnClimbActionStarted(const FInputActionValue &Value);

#pragma endregion

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent *PlayerInputComponent) override;

	// To add mapping context
	virtual void BeginPlay();

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE USpringArmComponent *GetCameraBoom() const { return CameraBoom; } 
	/** Returns FollowCamera subobject **/
	FORCEINLINE UCameraComponent *GetFollowCamera() const { return FollowCamera; }

	FORCEINLINE UCustomMovementComponent *GetCustomMovementComponent() const { return CustomMovementComponent; }
	FORCEINLINE UMotionWarpingComponent *GetMotionWarpingComponent() const { return MotionWarpingComponent; }
};
