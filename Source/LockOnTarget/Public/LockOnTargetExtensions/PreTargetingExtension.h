// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "LockOnTargetTypes.h"
#include "PreTargetingExtension.generated.h"

class UWidgetComponent;
class UUserWidget;
class UTargetComponent;

/**
 * 预瞄准扩展：先选定敌人身体，展示其所有可锁定部位，再确认锁定。
 *
 * 使用方式（蓝图）：
 * 1. StartPreTargeting() — 找到视线中的目标身体，为所有 Socket 显示预瞄准 UI
 * 2. 玩家移动视角选择部位（自动高亮最近视线的 Socket）
 * 3. CommitTarget() — 锁定高亮的 Socket
 *    或 CancelPreTargeting() — 取消预瞄准
 */
UCLASS(Blueprintable, HideCategories = Tick)
class LOCKONTARGET_API UPreTargetingExtension : public ULockOnTargetExtensionBase
{
	GENERATED_BODY()

public:

	UPreTargetingExtension();

public: /** 配置 */

	/** 预瞄准控件的 Widget 类。为空则复用 WidgetExtension 的默认控件。 */
	UPROPERTY(EditDefaultsOnly, Category = "Pre Targeting")
	TSoftClassPtr<UUserWidget> PreviewWidgetClass;

	/** 视线角度阈值（度），相机前向与 Socket 方向的夹角小于此值视为"正在看" */
	UPROPERTY(EditDefaultsOnly, Category = "Pre Targeting", meta = (ClampMin = 1.f, ClampMax = 90.f, Units = "deg"))
	float SelectionAngleThreshold;

	/** 未选中 Widget 的透明度 */
	UPROPERTY(EditDefaultsOnly, Category = "Pre Targeting", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float DeselectedOpacity;

public: /** 蓝图 API */

	/** 开始预瞄准：找到视线中的目标，在其所有 Socket 上显示预瞄准 UI */
	UFUNCTION(BlueprintCallable, Category = "Pre Targeting")
	void StartPreTargeting();

	/** 确认锁定当前高亮的 Socket */
	UFUNCTION(BlueprintCallable, Category = "Pre Targeting")
	void CommitTarget();

	/** 取消预瞄准，隐藏所有 UI */
	UFUNCTION(BlueprintCallable, Category = "Pre Targeting")
	void CancelPreTargeting();

	/** 是否处于预瞄准状态 */
	UFUNCTION(BlueprintPure, Category = "Pre Targeting")
	bool IsPreTargeting() const { return bIsPreTargeting; }

	/** 当前高亮的 Socket 名称（NAME_None 表示无选中） */
	UFUNCTION(BlueprintPure, Category = "Pre Targeting")
	FName GetSelectedSocket() const { return SelectedSocket; }

	/** 预瞄准的目标组件 */
	UFUNCTION(BlueprintPure, Category = "Pre Targeting")
	UTargetComponent* GetPreTargetComponent() const { return CachedTargetComponent; }

	/** 所有预瞄准 Widget 的映射表（Socket → WidgetComponent） */
	UFUNCTION(BlueprintPure, Category = "Pre Targeting")
	const TMap<FName, UWidgetComponent*>& GetPreviewWidgets() const { return PreviewWidgets; }

protected: /** 生命周期 */

	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void Update(float DeltaTime) override;

private:

	/** 找到视线中最佳的目标身体 */
	UTargetComponent* FindBestTargetBody() const;

	/** 在目标的所有 Socket 上创建预瞄准 Widget */
	void CreatePreviewWidgets();

	/** 销毁所有预瞄准 Widget */
	void DestroyPreviewWidgets();

	/** 根据视线方向更新当前选中的 Socket */
	void UpdateSelection();

	/** 根据选中状态更新 Widget 外观 */
	void UpdateSelectionVisuals();

	bool bIsPreTargeting = false;
	bool bHadPreviousLock = false;
	FTargetInfo CachedPreviousLock;
	TObjectPtr<UTargetComponent> CachedTargetComponent = nullptr;
	FName SelectedSocket = NAME_None;
	TMap<FName, UWidgetComponent*> PreviewWidgets;
};
