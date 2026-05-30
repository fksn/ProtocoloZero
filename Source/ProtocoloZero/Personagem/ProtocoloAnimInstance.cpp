// Fill out your copyright notice in the Description page of Project Settings.

#include "ProtocoloAnimInstance.h"
#include "ProtocoloPersonagem.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	FVector Velocity = ProtocoloPersonagem->GetVelocity();
	Velocity.Z = 0.f;
	Speed = Velocity.Size();

	bIsInAir = ProtocoloPersonagem->GetCharacterMovement()->IsFalling();
	bIsAccelerating = ProtocoloPersonagem->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false;
}
