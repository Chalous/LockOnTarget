// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved. (Modified for Gameplay Camera)

#pragma once

#include "CoreMinimal.h"
#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "GameplayCameraRotationExtension.generated.h"

class APlayerCameraManager;

/**
 * 专门为 UE5 Gameplay Camera 系统设计的锁定目标扩展。
 * 功能等价于 ControllerRotationExtension，但输出处理好的旋转给 Camera Rig 读取，
 * 而非直接调用 SetControlRotation。
 *
 * 核心职责：
 * 1. 计算平滑、预测后的目标位置（位置预测 + 临界阻尼弹簧平滑）
 * 2. 计算目标方向并应用角度偏移和限制
 * 3. 执行变速旋转插值（Ease-In）
 * 4. 输出 CalculatedTargetRotation 和 SmoothedTargetLocation 给 Camera Rig 使用
 *
 * 使用方式：
 * 在 Camera Rig 蓝图中读取 CalculatedTargetRotation，通过 Set Rotation 节点控制 Boom Arm。
 * SmoothedTargetLocation 可用于 LookAt 节点或 Auto Rotate Input 2D 的 DirectionVector。
 */
UCLASS(Blueprintable, HideCategories = Tick)
class LOCKONTARGET_API UGameplayCameraRotationExtension : public ULockOnTargetExtensionBase
{
	GENERATED_BODY()

public:

	UGameplayCameraRotationExtension();

public: // 输出数据 - Camera Rig 读取

	/**
	 * 最终计算的、包含所有插值和平滑的目标旋转。
	 *
	 * Camera Rig 应该读取此值并通过 Set Rotation 节点应用到 Boom Arm。
	 * 此 Rotator 已包含：
	 * - 位置预测后的目标方向
	 * - 振荡平滑效果
	 * - 角度偏移（Offset）
	 * - 角度限制（Clamp）
	 * - 旋转插值（Easing）
	 * - Roll 归零
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay Camera Data")
	FRotator CalculatedTargetRotation;

	/**
	 * 经过预测和平滑处理后的目标位置。
	 *
	 * 可用于 Camera Rig 中的 LookAt 节点或其他需要位置的逻辑。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay Camera Data")
	FVector SmoothedTargetLocation;

public: // 输入控制

	/** 是否在锁定目标时阻止玩家的鼠标/摇杆输入 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bBlockLookInput;

public: // 位置修正

	/** 是否使用速度预测目标位置（减少"追赶"延迟） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction")
	bool bUseLocationPrediction;

	/** 预测时间（秒），默认 0.083s ≈ 60fps 5 帧 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction", meta = (ClampMin = 0.f, ClampMax = 0.5f, Units = "s", EditCondition = "bUseLocationPrediction", EditConditionHides))
	float PredictionTime;

	/** 预测的最大角度偏差（度），防止过度预测 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction", meta = (ClampMin = 0.f, ClampMax = 30.f, Units = "deg", EditCondition = "bUseLocationPrediction", EditConditionHides))
	float MaxAngularDeviation;

	/** 是否使用振荡平滑（消除"果冻"抖动） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction")
	bool bUseOscillationSmoothing;

	/** 振荡阻尼系数，值越大响应越快但震荡越大 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction", meta = (ClampMin = 0.f, ClampMax = 20.f, EditCondition = "bUseOscillationSmoothing", EditConditionHides))
	float OscillationDampingFactor;

public: // 角度限制

	/** 垂直方向死区容忍度（度），超过此角度不旋转 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = 0.f, ClampMax = 20.f, Units = "deg"))
	float DeadZonePitchTolerance;

	/** Yaw 偏移（度），通常用于保持相机在目标一侧 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = -30.f, ClampMax = 30.f, Units = "deg"))
	float YawOffset;

	/** Yaw 限制范围（度），相对于目标方向的对称限制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = 10.f, ClampMax = 90.f, Units = "deg"))
	float YawClampRange;

	/** Pitch 偏移（度），负值让相机稍微向下看 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = -30.f, ClampMax = 30.f, Units = "deg"))
	float PitchOffset;

	/** Pitch 限制范围（度），相对于玩家→目标方向的偏移量。X 为向上偏移（负=更向上），Y 为向下偏移（正=更向下） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = -90.f, ClampMax = 90.f, Units = "deg"))
	FVector2D PitchClamp;

public: // 旋转插值

	/** 大角度变化时的插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 0.f, ClampMax = 30.f))
	float InterpolationSpeed;

	/** 角度睡眠容忍度（度），小于此角度不执行插值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 0.f, ClampMax = 10.f, Units = "deg"))
	float AngularSleepTolerance;

	/** 插值过渡范围（度），从最小速度到最大速度的过渡区间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 1.f, ClampMax = 30.f, Units = "deg"))
	float InterpEasingRange;

	/** Easing 曲线指数，控制加速过程 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 0.f, ClampMax = 5.f))
	float InterpEasingExponent;

	/** 最小插值速度，用于小角度变化的平滑 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 0.f, ClampMax = 30.f))
	float MinInterpSpeed;

private:

	// 弹簧阻尼系统状态
	FVector SpringVelocity;
	TOptional<FVector> SpringLocation;

public:

	/** 重置弹簧插值的缓存数据（切换 Socket 时调用） */
	void ResetSpringInterpData();

public:

	/**
	 * 计算目标旋转的核心函数（可被子类覆盖）
	 *
	 * 执行流程：
	 * 1. 获取目标焦点位置
	 * 2. 应用位置预测（基于相对速度）和振荡平滑（临界阻尼弹簧）
	 * 3. 死区检测：超出容差时返回上一帧的旋转值
	 * 4. 计算目标方向并应用 Pitch/Yaw 偏移和限制
	 * 5. 执行变速旋转插值（小角度慢速，大角度快速），Roll 始终归零
	 *
	 * @param CameraManager 相机管理器，用于获取当前相机旋转和位置
	 * @param DeltaTime 帧时间
	 * @return 最终的目标旋转（已插值、已限制、Roll=0）
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera Rotation")
	FRotator CalcTargetRotation(const APlayerCameraManager* CameraManager, float DeltaTime);
	virtual FRotator CalcTargetRotation_Implementation(const APlayerCameraManager* CameraManager, float DeltaTime);

	/** 获取目标的焦点位置（Socket 位置） */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera Rotation")
	FVector GetTargetFocusLocation() const;
	virtual FVector GetTargetFocusLocation_Implementation() const;

protected:

	/**
	 * 对目标位置进行修正处理
	 *
	 * @param TargetLocation 原始目标位置
	 * @param Distance2D 二维平面距离
	 * @param DeltaTime 帧时间
	 * @return 修正后的目标位置
	 */
	virtual FVector GetCorrectedTargetLocation(const FVector& TargetLocation, float Distance2D, float DeltaTime);

	/**
	 * 计算目标方向的角度（应用偏移和限制）
	 *
	 * Yaw：相对于玩家→目标方向的对称限制（±YawClampRange）
	 * Pitch：相对于玩家→目标方向的偏移限制（+PitchClamp）
	 *
	 * @param ViewLocation 观察点位置
	 * @param TargetLocation 目标位置
	 * @param CurrentRotation 当前旋转
	 * @return 应用偏移和限制后的目标旋转
	 */
	virtual FRotator GetTargetRotation(const FVector& ViewLocation, const FVector& TargetLocation, const FRotator& CurrentRotation);

	/**
	 * 对旋转进行插值平滑
	 *
	 * 使用变速插值：小角度用慢速，大角度用快速
	 *
	 * @param TargetRotation 目标旋转
	 * @param CurrentRotation 当前旋转
	 * @param DeltaTime 帧时间
	 * @return 插值后的旋转
	 */
	virtual FRotator InterpTargetRotation(const FRotator& TargetRotation, const FRotator& CurrentRotation, float DeltaTime);

protected: // Extension 生命周期

	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void Update(float DeltaTime) override;
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
	virtual void OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket) override;
};
