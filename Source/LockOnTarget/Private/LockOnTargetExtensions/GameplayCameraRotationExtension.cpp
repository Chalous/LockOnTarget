// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved. (Modified for Gameplay Camera)

#include "LockOnTargetExtensions/GameplayCameraRotationExtension.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

/**
 * 临界阻尼弹簧插值函数
 *
 * 物理公式实现：https://mathproofs.blogspot.com/2013/07/critically-damped-spring-smoothing.html
 *
 * 使用二阶弹簧系统（位置 + 速度）实现平滑插值，在响应速度和收敛性之间达到最佳平衡。
 *
 * @param Current 当前值
 * @param Target 目标值
 * @param InOutVelocity 速度（会被函数更新）
 * @param DeltaTime 帧时间
 * @param InterpSpeed 插值速度（弹簧刚度）
 * @return 插值后的新值
 */
static FVector VInterpCriticallyDamped(const FVector& Current, const FVector& Target,
	FVector& InOutVelocity, float DeltaTime, float InterpSpeed)
{
	const FVector Delta = InOutVelocity - (Current - Target) * (FMath::Square(InterpSpeed) * DeltaTime);
	InOutVelocity = Delta / FMath::Square(1.f + InterpSpeed * DeltaTime);
	return Current + InOutVelocity * DeltaTime;
}

UGameplayCameraRotationExtension::UGameplayCameraRotationExtension()
	: bBlockLookInput(true)
	, bUseLocationPrediction(true)
	, PredictionTime(0.083f)
	, MaxAngularDeviation(10.f)
	, bUseOscillationSmoothing(true)
	, OscillationDampingFactor(2.9f)
	, DeadZonePitchTolerance(12.f)
	, YawOffset(0.f)
	, YawClampRange(35.f)
	, PitchOffset(-10.f)
	, PitchClamp(-50.f, 30.f)
	, InterpolationSpeed(12.5f)
	, AngularSleepTolerance(4.75f)
	, InterpEasingRange(10.f)
	, InterpEasingExponent(1.25f)
	, MinInterpSpeed(0.65f)
	, SpringVelocity(0.f)
{
	// 在物理后 Tick，确保使用最新的物理状态
	ExtensionTick.TickGroup = TG_PostPhysics;
	ExtensionTick.bCanEverTick = true;
	ExtensionTick.bStartWithTickEnabled = false;
	ExtensionTick.bAllowTickOnDedicatedServer = false;
}

void UGameplayCameraRotationExtension::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);
	// 注意：ControllerRotationExtension 通过 AddPrerequisite 确保在 SpringArm 之前 Tick。
	// Gameplay Camera 没有 SpringArm，Extension 只更新输出变量，由 Camera Rig 的
	// Evaluator 在蓝图中读取。如果 Evaluator 在本帧 Extension Tick 之前运行，将产生 1 帧延迟。
	// TODO: 建立与 Camera Rig Evaluator 的 Tick 依赖关系以消除潜在延迟。
}

void UGameplayCameraRotationExtension::Deinitialize(ULockOnTargetComponent* Instigator)
{
	Super::Deinitialize(Instigator);
	// 清理资源
}

void UGameplayCameraRotationExtension::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);
	SetTickEnabled(true);

	if (bBlockLookInput)
	{
		if (APlayerController* const Controller = GetPlayerController())
		{
			Controller->SetIgnoreLookInput(true);
		}
	}
}

void UGameplayCameraRotationExtension::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	SetTickEnabled(false);
	ResetSpringInterpData();

	if (APlayerController* const PC = GetPlayerController())
	{
		// 恢复玩家输入
		if (bBlockLookInput)
		{
			PC->SetIgnoreLookInput(false);
		}

		// 使用最后计算的旋转重置控制器，确保解锁瞬间平滑
		if (!CalculatedTargetRotation.IsNearlyZero())
		{
			PC->SetControlRotation(CalculatedTargetRotation);
		}
	}
}

void UGameplayCameraRotationExtension::OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	Super::OnSocketChanged(CurrentTarget, NewSocket, OldSocket);
	// 切换 Socket 时重置弹簧数据，避免位置跳跃
	ResetSpringInterpData();
}

void UGameplayCameraRotationExtension::ResetSpringInterpData()
{
	SpringLocation.Reset();
	SpringVelocity = FVector::ZeroVector;
}

void UGameplayCameraRotationExtension::Update(float DeltaTime)
{
	if (GetLockOnTargetComponent()->IsTargetLocked())
	{
		APlayerController* const PC = GetPlayerController();
		if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
		{
			// 计算完整的目标旋转并缓存到输出变量
			// Camera Rig 将在下一帧读取此值
			CalculatedTargetRotation = CalcTargetRotation(PC->PlayerCameraManager, DeltaTime);
		}
	}
}

FRotator UGameplayCameraRotationExtension::CalcTargetRotation_Implementation(
	const APlayerCameraManager* CameraManager, float DeltaTime)
{
	// 1. 获取当前相机旋转作为插值基准
	const FRotator CurrentRotation = CameraManager->GetCameraRotation();

	// 2. 获取目标的原始位置
	const FVector InitialTargetLocation = GetTargetFocusLocation();
	FVector TargetLocation = InitialTargetLocation;

	const AActor* const OwnerActor = GetLockOnTargetComponent()->GetOwner();
	const float Distance2D = (OwnerActor->GetActorLocation() - InitialTargetLocation).Size2D();
	const float CollisionRadius = OwnerActor->GetSimpleCollisionRadius();

	// 3. 对目标位置进行修正（预测 + 振荡平滑）
	TargetLocation = GetCorrectedTargetLocation(InitialTargetLocation, Distance2D, DeltaTime);

	// 4. 限制预测距离不超过目标与玩家的距离（避免越过玩家）
	const float MaxOffsetLength = FMath::Max(Distance2D - CollisionRadius, 0.f);
	const FVector FinalOffset = TargetLocation - InitialTargetLocation;
	TargetLocation = InitialTargetLocation + FinalOffset.GetClampedToMaxSize2D(MaxOffsetLength);

	// 5. 死区检测：垂直角度过大或距离过近时不旋转
	{
		const FVector ToTarget = TargetLocation - OwnerActor->GetActorLocation();
		const float ToTargetPitch = FMath::Atan2(ToTarget.Z, ToTarget.Size2D());
		const float DeadZoneMaxPitch = FMath::DegreesToRadians(90.f - DeadZonePitchTolerance);

		if (Distance2D < CollisionRadius || FMath::Abs(ToTargetPitch) > DeadZoneMaxPitch)
		{
			// 不更新输出，保持当前值
			return CalculatedTargetRotation;
		}
	}

	// 6. 保存平滑后的目标位置供 Camera Rig 使用
	SmoothedTargetLocation = TargetLocation;

	// 7. 计算目标方向（使用相机实际位置作为观察点）
	const FVector ViewLocation = CameraManager->GetCameraLocation();
	FRotator OutRotation = GetTargetRotation(ViewLocation, TargetLocation, CurrentRotation);

	// 8. 执行旋转插值
	OutRotation = InterpTargetRotation(OutRotation, CurrentRotation, DeltaTime);

	return OutRotation;
}

FVector UGameplayCameraRotationExtension::GetCorrectedTargetLocation(
	const FVector& TargetLocation, float Distance2D, float DeltaTime)
{
	const auto* const LockOnComponent = GetLockOnTargetComponent();
	const AActor* const TargetActor = LockOnComponent->GetTargetActor();
	FVector OutLocation = TargetLocation;

	// 位置预测：基于速度预测目标未来的位置
	if (bUseLocationPrediction && PredictionTime > 0.f)
	{
		const AActor* OwnerActor = LockOnComponent->GetOwner();
		// 使用相对速度（目标速度 - 玩家速度）
		FVector Velocity = TargetActor->GetVelocity() - OwnerActor->GetVelocity();
		Velocity *= FMath::Min(PredictionTime, 0.5f);

		// 限制预测距离，避免过度预测导致相机穿模
		const float MaxLength = Distance2D * FMath::Tan(FMath::DegreesToRadians(MaxAngularDeviation));
		OutLocation += Velocity.GetClampedToMaxSize(MaxLength);
	}

	// 振荡平滑：在目标相对空间中使用弹簧阻尼系统
	// 这样可以避免 Actor 自身移动导致的相机"果冻"抖动
	if (bUseOscillationSmoothing)
	{
		const FVector TargetActorLocation = TargetActor->GetActorLocation();

		// 初始化弹簧状态
		if (!SpringLocation.IsSet())
		{
			SpringLocation = TargetLocation - TargetActorLocation;
		}

		// 获取相对于目标 Actor 的位置
		FVector RelativeLocation = OutLocation - TargetActorLocation;

		// 执行弹簧插值（跳过极小变化以提高性能）
		if (!RelativeLocation.Equals(SpringLocation.GetValue(), 1e-2f))
		{
			RelativeLocation = VInterpCriticallyDamped(
				SpringLocation.GetValue(), RelativeLocation,
				SpringVelocity, DeltaTime, OscillationDampingFactor);
			OutLocation = TargetActorLocation + RelativeLocation;
			SpringLocation = RelativeLocation;
		}
	}

	return OutLocation;
}

FRotator UGameplayCameraRotationExtension::GetTargetRotation(
	const FVector& ViewLocation, const FVector& InTargetLocation, const FRotator& CurrentRotation)
{
	// 1. 计算朝向目标的基础旋转
	FRotator OutRotation = (InTargetLocation - ViewLocation).ToOrientationRotator();

	// 2. 应用角度偏移
	OutRotation.Pitch += PitchOffset;
	OutRotation.Yaw += YawOffset;

	// 3. 应用角度限制
	{
		// Yaw 限制：相对于目标方向对称限制
		const FVector Pivot = GetLockOnTargetComponent()->GetOwner()->GetActorLocation();
		const FVector ToTarget = InTargetLocation - Pivot;
		const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
		OutRotation.Yaw = FMath::ClampAngle(
			OutRotation.Yaw,
			TargetYaw - YawClampRange,
			TargetYaw + YawClampRange);

		// Pitch 限制：根据当前状态动态调整睡眠容忍度
		float PitchMinClamped = PitchClamp.X;
		float PitchMaxClamped = PitchClamp.Y;

		// 如果当前已经超出限制，暂时放宽容忍度避免卡住
		if (CurrentRotation.Pitch > PitchClamp.Y)
		{
			PitchMaxClamped -= AngularSleepTolerance;
		}
		else if (CurrentRotation.Pitch < PitchClamp.X)
		{
			PitchMinClamped += AngularSleepTolerance;
		}

		OutRotation.Pitch = FMath::ClampAngle(OutRotation.Pitch, PitchMinClamped, PitchMaxClamped);
	}

	return OutRotation;
}

FRotator UGameplayCameraRotationExtension::InterpTargetRotation(
	const FRotator& TargetRotation, const FRotator& CurrentRotation, float DeltaTime)
{
	FRotator OutRotation = CurrentRotation;

	// 计算当前旋转与目标旋转的角度差
	const float Delta = FMath::RadiansToDegrees(
		FMath::Acos(TargetRotation.Vector() | CurrentRotation.Vector()));

	// 角度睡眠：小于容忍度时不执行插值（避免微小抖动）
	if (Delta > AngularSleepTolerance)
	{
		// 计算插值进度 [0, 1]
		const float InterpEasingRangeSafe = FMath::Max(InterpEasingRange, 1.f);
		const float Alpha = FMath::Clamp(
			(Delta - AngularSleepTolerance) / InterpEasingRangeSafe, 0.f, 1.f);

		// 变速插值：小角度用慢速，大角度用快速
		const float ScaledInterpSpeed = FMath::InterpEaseIn(
			MinInterpSpeed,
			InterpolationSpeed,
			Alpha,
			InterpEasingExponent);

		// 执行旋转插值
		OutRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, ScaledInterpSpeed);
	}

	// 始终归零 Roll，保持相机水平
	OutRotation.Roll = 0.f;

	return OutRotation;
}

FVector UGameplayCameraRotationExtension::GetTargetFocusLocation_Implementation() const
{
	// 获取被锁定目标 Socket 的世界位置
	return GetLockOnTargetComponent()->GetCapturedFocusPointLocation();
}
