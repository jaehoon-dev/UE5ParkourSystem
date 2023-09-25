// Fill out your copyright notice in the Description page of Project Settings.

#include "../../Public/Components/CustomMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "../../ClimbingSystemCharacter.h"
#include "../../DebugHelper.h"
#include "Components/CapsuleComponent.h"

void UCustomMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    OwningPlayerAnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();

    if (OwningPlayerAnimInstance)
    {
        OwningPlayerAnimInstance->OnMontageEnded.AddDynamic(this, &UCustomMovementComponent::OnClimbMontageEnded);
        OwningPlayerAnimInstance->OnMontageBlendingOut.AddDynamic(this, &UCustomMovementComponent::OnClimbMontageEnded);
    }
}

void UCustomMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCustomMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    if (IsClimbing())
    {
        bOrientRotationToMovement = false;
        CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
    }

    if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::MOVE_Climb)
    {
        bOrientRotationToMovement = true;
        CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

        // Reset pitch and roll
        const FRotator DirtyRotation = UpdatedComponent->GetComponentRotation();
        const FRotator CleanStandRotation = FRotator(0.f, DirtyRotation.Yaw, 0.f);
        UpdatedComponent->SetRelativeRotation(CleanStandRotation);

        StopMovementImmediately();
    }

    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UCustomMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
    if (IsClimbing())
    {
        PhysClimb(deltaTime, Iterations);
    }

    Super::PhysCustom(deltaTime, Iterations);
}

float UCustomMovementComponent::GetMaxSpeed() const
{
    if (IsClimbing())
    {
        return MaxClimbSpeed;
    }
    else
    {
        return Super::GetMaxSpeed();
    }
}

float UCustomMovementComponent::GetMaxAcceleration() const
{
    if (IsClimbing())
    {
        return MaxClimbAcceleration;
    }
    else
    {
        return Super::GetMaxAcceleration();
    }
}

FVector UCustomMovementComponent::ConstrainAnimRootMotionVelocity(const FVector &RootMotionVelocity, const FVector &CurrentVelocity) const
{
    const bool bIsPlayingRMMontage =
        IsFalling() && OwningPlayerAnimInstance && OwningPlayerAnimInstance->IsAnyMontagePlaying();

    if (bIsPlayingRMMontage)
    {
        return RootMotionVelocity;
    }
    else
    {
        return Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);
    }
}

#pragma region ClimbTraces
TArray<FHitResult> UCustomMovementComponent::DoCapsuleTraceMultiByObject(const FVector &Start, const FVector &End, bool bShowDebugShape, bool bDrawPersistentShapes)
{
    TArray<FHitResult> OutCapsuleTraceHitResults;

    EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;
    if (bShowDebugShape)
    {
        DebugTraceType = EDrawDebugTrace::ForOneFrame;

        if (bDrawPersistentShapes)
        {
            DebugTraceType = EDrawDebugTrace::Persistent;
        }
    }

    UKismetSystemLibrary::CapsuleTraceMultiForObjects(
        this,
        Start,
        End,
        ClimbCapsuleTraceRadius,
        ClimbCapsuleTraceHalfHeight,
        ClimbableSurfaceTraceTypes,
        false,
        TArray<AActor *>(),
        DebugTraceType,
        OutCapsuleTraceHitResults,
        false);

    return OutCapsuleTraceHitResults;
}

FHitResult UCustomMovementComponent::DoLineTraceSingleByObject(const FVector &Start, const FVector &End, bool bShowDebugShape, bool bDrawPersistentShapes)
{
    FHitResult OutHit;

    EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;
    if (bShowDebugShape)
    {
        DebugTraceType = EDrawDebugTrace::ForOneFrame;

        if (bDrawPersistentShapes)
        {
            DebugTraceType = EDrawDebugTrace::Persistent;
        }
    }

    UKismetSystemLibrary::LineTraceSingleForObjects(
        this,
        Start,
        End,
        ClimbableSurfaceTraceTypes,
        false,
        TArray<AActor *>(),
        DebugTraceType, OutHit,
        false);

    return OutHit;
}

#pragma endregion

#pragma region ClimbCore
void UCustomMovementComponent::ToggleClimbing(bool bAttemptClimbing)
{
    if (bAttemptClimbing)
    {
        if (CanStartClimbing())
        {
            PlayClimbMontage(IdleToClimbMontage);
        }
        else if (CanClimbDownLedge())
        {
            PlayClimbMontage(ClimbDownLedgeMontage);
        }
    }
    else
    {
        StopClimbing();
    }
}

bool UCustomMovementComponent::CanClimbDownLedge()
{
    if (IsFalling())
        return false;

    const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
    const FVector ComponentForward = UpdatedComponent->GetForwardVector();
    const FVector DownVector = -UpdatedComponent->GetUpVector();

    const FVector WalkableSurfaceTraceStart = ComponentLocation + ComponentForward * ClimbDownWalkableSurfaceTraceOffset;
    const FVector WalkableSurfaceTraceEnd = WalkableSurfaceTraceStart + DownVector * 100.f;

    FHitResult WalkableSurfaceHit = DoLineTraceSingleByObject(WalkableSurfaceTraceStart, WalkableSurfaceTraceEnd, true);

    const FVector LedgeTraceStart = WalkableSurfaceHit.TraceStart + ComponentForward * ClimbDownLedgeTraceOffset;
    const FVector LedgeTraceEnd = LedgeTraceStart + DownVector * 300.f;

    FHitResult LedgeTraceHit = DoLineTraceSingleByObject(LedgeTraceStart, LedgeTraceEnd);

    if (WalkableSurfaceHit.bBlockingHit && !LedgeTraceHit.bBlockingHit)
    {
        return true;
    }

    return false;
}

bool UCustomMovementComponent::CanStartClimbing()
{
    if (IsFalling())
        return false;
    ClimbableSurfacesTracedResults = GetClimbableSurfaces();
    if (ClimbableSurfacesTracedResults.IsEmpty())
        return false;
    if (!TraceFromEyeHeight(100.f).bBlockingHit)
        return false;

    return true;
}

void UCustomMovementComponent::StartClimbing()
{
    SetMovementMode(MOVE_Custom, ECustomMovementMode::MOVE_Climb);
}

void UCustomMovementComponent::StopClimbing()
{
    if (IsClimbing())
    {
        SetMovementMode(MOVE_Falling);
    }
    else
    {
        SetMovementMode(MOVE_Walking);
    }
}

bool UCustomMovementComponent::CheckHasReachedLedge()
{
    FHitResult LedgetHitResult = TraceFromEyeHeight(100.f, 50.f);

    if (!LedgetHitResult.bBlockingHit)
    {
        const FVector WalkableSurfaceTraceStart = LedgetHitResult.TraceEnd;

        const FVector DownVector = -UpdatedComponent->GetUpVector();
        const FVector WalkableSurfaceTraceEnd = WalkableSurfaceTraceStart + DownVector * 100.f;

        FHitResult WalkabkeSurfaceHitResult =
            DoLineTraceSingleByObject(WalkableSurfaceTraceStart, WalkableSurfaceTraceEnd);

        if (WalkabkeSurfaceHitResult.bBlockingHit && GetUnrotatedClimbVelocity().Z > 10.f)
        {
            return true;
        }
    }

    return false;
}

bool UCustomMovementComponent::IsClimbing() const
{
    return MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::MOVE_Climb;
}

void UCustomMovementComponent::PhysClimb(float deltaTime, int32 Iterations)
{
    if (deltaTime < MIN_TICK_TIME)
    {
        return;
    }

    GetClimbableSurfaces();
    ProcessClimbableSurfaceInfo();

    if (ShouldStopClimbing() || CheckHasReachedFloor())
    {
        StopClimbing();
    }

    RestorePreAdditiveRootMotionVelocity();

    if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
    { // Define the max climb speed and acceleration
        CalcVelocity(deltaTime, 0.f, true, MaxBreakClimbDeceleration);
    }

    ApplyRootMotionToVelocity(deltaTime);

    FVector OldLocation = UpdatedComponent->GetComponentLocation();
    const FVector Adjusted = Velocity * deltaTime;
    FHitResult Hit(1.f);

    // Handle climb rotation
    SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(deltaTime), true, Hit);

    if (Hit.Time < 1.f)
    {
        // adjust and try again
        HandleImpact(Hit, deltaTime, Adjusted);
        SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
    }

    if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
    {
        Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
    }

    SnapMovementToClimbableSurfaces(deltaTime);

    if (CheckHasReachedLedge())
    {
        PlayClimbMontage(ClimbToTopMontage);
    }
}

void UCustomMovementComponent::ProcessClimbableSurfaceInfo()
{
    CurrentClimbableSurfaceLocation = FVector::ZeroVector;
    CurrentClimbableSurfaceNormal = FVector::ZeroVector;

    if (ClimbableSurfacesTracedResults.IsEmpty())
        return;

    for (const FHitResult &TracedHitResult : ClimbableSurfacesTracedResults)
    {
        CurrentClimbableSurfaceLocation += TracedHitResult.ImpactPoint;
        CurrentClimbableSurfaceNormal += TracedHitResult.ImpactNormal;
    }

    CurrentClimbableSurfaceLocation /= ClimbableSurfacesTracedResults.Num();
    CurrentClimbableSurfaceNormal = CurrentClimbableSurfaceNormal.GetSafeNormal();
}

bool UCustomMovementComponent::ShouldStopClimbing()
{
    if (ClimbableSurfacesTracedResults.IsEmpty())
        return true;

    const float DotResult = FVector::DotProduct(CurrentClimbableSurfaceNormal, FVector::UpVector);
    const float DegreeDiff = FMath::RadiansToDegrees(FMath::Acos(DotResult));

    if (DegreeDiff <= 60.f)
    {
        return true;
    }

    return false;
}

bool UCustomMovementComponent::CheckHasReachedFloor()
{
    const FVector DownVector = -UpdatedComponent->GetUpVector();
    const FVector StartOffset = DownVector * 50.f;

    const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
    const FVector End = Start + DownVector;

    TArray<FHitResult> PossibleFloorHits = DoCapsuleTraceMultiByObject(Start, End);

    if (PossibleFloorHits.IsEmpty())
        return false;

    for (const FHitResult &PossibleFloorHit : PossibleFloorHits)
    {
        const bool bFloorReached =
            FVector::Parallel(-PossibleFloorHit.ImpactNormal, FVector::UpVector) &&
            GetUnrotatedClimbVelocity().Z < -10.f;

        if (bFloorReached)
        {
            return true;
        }
    }

    return false;
}

FQuat UCustomMovementComponent::GetClimbRotation(float DeltaTime)
{
    const FQuat CurrentQuat = UpdatedComponent->GetComponentQuat();

    if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
    {
        return CurrentQuat;
    }

    const FQuat TargetQuat = FRotationMatrix::MakeFromX(-CurrentClimbableSurfaceNormal).ToQuat();

    return FMath::QInterpTo(CurrentQuat, TargetQuat, DeltaTime, 5.f);
}

void UCustomMovementComponent::SnapMovementToClimbableSurfaces(float DeltaTime)
{
    const FVector ComponentForward = UpdatedComponent->GetForwardVector();
    const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();

    const FVector ProjectedCharacterToSurface =
        (CurrentClimbableSurfaceLocation - ComponentLocation).ProjectOnTo(ComponentForward);

    const FVector SnapVector = -CurrentClimbableSurfaceNormal * ProjectedCharacterToSurface.Length();

    UpdatedComponent->MoveComponent(
        SnapVector * DeltaTime * MaxClimbSpeed,
        UpdatedComponent->GetComponentQuat(),
        true);
}

TArray<FHitResult> UCustomMovementComponent::GetClimbableSurfaces()
{
    const FVector &StartOffset = UpdatedComponent->GetForwardVector() * 30.f;
    const FVector &Start = UpdatedComponent->GetComponentLocation() + StartOffset;
    const FVector &End = Start + UpdatedComponent->GetForwardVector();
    ClimbableSurfacesTracedResults = DoCapsuleTraceMultiByObject(Start, End);
    return ClimbableSurfacesTracedResults;
}

FHitResult UCustomMovementComponent::TraceFromEyeHeight(float TraceDistance, float TraceStartOffset)
{
    const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
    const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset);
    const FVector Start = ComponentLocation + EyeHeightOffset;
    const FVector End = Start + UpdatedComponent->GetForwardVector() * TraceDistance;

    return DoLineTraceSingleByObject(Start, End);
}

void UCustomMovementComponent::PlayClimbMontage(UAnimMontage *MontageToPlay)
{
    if (!MontageToPlay)
        return;
    if (!OwningPlayerAnimInstance)
        return;
    if (OwningPlayerAnimInstance->IsAnyMontagePlaying())
        return;

    OwningPlayerAnimInstance->Montage_Play(MontageToPlay);
}

void UCustomMovementComponent::OnClimbMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    if (Montage == IdleToClimbMontage || Montage == ClimbDownLedgeMontage)
    {
        StartClimbing();
        StopMovementImmediately();
    }
    if (Montage == ClimbToTopMontage)
    {
        SetMovementMode(MOVE_Walking);
    }
}

FVector UCustomMovementComponent::GetUnrotatedClimbVelocity() const
{
    return UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), Velocity);
}
#pragma endregion
