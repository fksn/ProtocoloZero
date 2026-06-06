// Fill out your copyright notice in the Description page of Project Settings.

#include "ProtocoloAnimInstance.h"
#include "ProtocoloPersonagem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UProtocoloAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	ProtocoloPersonagem = Cast<AProtocoloPersonagem>(TryGetPawnOwner());
}

void UProtocoloAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	if (ProtocoloPersonagem == nullptr)
	{
		ProtocoloPersonagem = Cast<AProtocoloPersonagem>(TryGetPawnOwner());
	}
	if (ProtocoloPersonagem == nullptr) return;

	FVector Velocity = ProtocoloPersonagem->GetVelocity(); //[cite: 1]
	Velocity.Z = 0.f; //[cite: 1]
	Speed = Velocity.Size(); //[cite: 1]

	bIsInAir = ProtocoloPersonagem->GetCharacterMovement()->IsFalling(); //[cite: 1]
	bIsAccelerating = ProtocoloPersonagem->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false; //[cite: 1]
	bWeaponEquipped = ProtocoloPersonagem->IsWeaponEquipped(); //[cite: 1]
	bIsCrouched = ProtocoloPersonagem->bIsCrouched; //[cite: 1]
	bAiming = ProtocoloPersonagem->IsAiming(); //[cite: 1]

	// --- CORREÇÃO AQUI ---
	// Só calculamos o YawOffset (Direction) se o personagem estiver realmente se movendo
	if (bIsAccelerating && Speed > 3.f)
	{
		FRotator AimRotation = ProtocoloPersonagem->GetBaseAimRotation(); //[cite: 1]
		FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(Velocity); //[cite: 1]
		Direction = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw; //[cite: 1]
	}
	else
	{
		// Se estiver parado, zera a direção para não quebrar a animação de Idle
		Direction = 0.f;
	}
	// ---------------------

	CharacterRotationLastFrame = CharacterRotation; //[cite: 1]
	CharacterRotation = ProtocoloPersonagem->GetActorRotation(); //[cite: 1]
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame); //[cite: 1]
	const float Target = Delta.Yaw / DeltaTime; //[cite: 1]
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f); //[cite: 1]
	Lean = FMath::Clamp(Interp, -90.f, 90.f); //[cite: 1]
}
