// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetExtensions/PreTargetingExtension.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "TargetManager.h"
#include "TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"

#include "Components/WidgetComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"

UPreTargetingExtension::UPreTargetingExtension()
	: SelectionAngleThreshold(25.f)
	, DeselectedOpacity(0.4f)
{
	ExtensionTick.TickGroup = TG_PostPhysics;
	ExtensionTick.bCanEverTick = true;
	ExtensionTick.bStartWithTickEnabled = false;
	ExtensionTick.bAllowTickOnDedicatedServer = false;
}

void UPreTargetingExtension::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);
}

void UPreTargetingExtension::Deinitialize(ULockOnTargetComponent* Instigator)
{
	if (bIsPreTargeting)
	{
		DestroyPreviewWidgets();
		bIsPreTargeting = false;
	}

	bHadPreviousLock = false;
	CachedPreviousLock = FTargetInfo();

	Super::Deinitialize(Instigator);
}

void UPreTargetingExtension::Update(float DeltaTime)
{
	if (!bIsPreTargeting || !IsValid(CachedTargetComponent))
	{
		CancelPreTargeting();
		return;
	}

	if (!CachedTargetComponent->CanBeCaptured())
	{
		CancelPreTargeting();
		return;
	}

	UpdateSelection();
}

void UPreTargetingExtension::StartPreTargeting()
{
	if (bIsPreTargeting)
	{
		return;
	}

	const bool bLocked = GetLockOnTargetComponent()->IsTargetLocked();

	if (bLocked)
	{
		// 保存当前锁定信息（取消时恢复用）
		CachedPreviousLock.TargetComponent = GetLockOnTargetComponent()->GetTargetComponent();
		CachedPreviousLock.Socket = GetLockOnTargetComponent()->GetCapturedSocket();
		bHadPreviousLock = true;

		// 解锁以释放镜头：旋转扩展的 OnTargetUnlocked 会自动
		// 调用 SetTickEnabled(false) 和 SetIgnoreLookInput(false)
		CachedTargetComponent = CachedPreviousLock.TargetComponent;
		GetLockOnTargetComponent()->ClearTargetManual();
	}
	else
	{
		bHadPreviousLock = false;
		CachedTargetComponent = FindBestTargetBody();
	}

	if (!CachedTargetComponent || CachedTargetComponent->GetSockets().IsEmpty())
	{
		CachedTargetComponent = nullptr;
		bHadPreviousLock = false;
		return;
	}

	CreatePreviewWidgets();
	bIsPreTargeting = true;
	SetTickEnabled(true);
	UpdateSelection();
	UpdateSelectionVisuals();
}

void UPreTargetingExtension::CommitTarget()
{
	if (!bIsPreTargeting || !IsValid(CachedTargetComponent))
	{
		return;
	}

	if (SelectedSocket != NAME_None && CachedTargetComponent->IsSocketValid(SelectedSocket))
	{
		AActor* const TargetActor = CachedTargetComponent->GetOwner();
		GetLockOnTargetComponent()->SetLockOnTargetManual(TargetActor, SelectedSocket);
	}

	DestroyPreviewWidgets();
	bIsPreTargeting = false;
	bHadPreviousLock = false;
	SetTickEnabled(false);
	CachedTargetComponent = nullptr;
	SelectedSocket = NAME_None;
}

void UPreTargetingExtension::CancelPreTargeting()
{
	if (!bIsPreTargeting)
	{
		return;
	}

	DestroyPreviewWidgets();
	bIsPreTargeting = false;
	SetTickEnabled(false);

	// 如果之前是锁定状态，恢复原来的锁定
	if (bHadPreviousLock && CachedPreviousLock.TargetComponent
		&& CachedPreviousLock.TargetComponent->CanBeCaptured()
		&& CachedPreviousLock.TargetComponent->IsSocketValid(CachedPreviousLock.Socket))
	{
		GetLockOnTargetComponent()->SetLockOnTargetManual(
			CachedPreviousLock.TargetComponent->GetOwner(),
			CachedPreviousLock.Socket);
	}

	bHadPreviousLock = false;
	CachedTargetComponent = nullptr;
	SelectedSocket = NAME_None;
}

UTargetComponent* UPreTargetingExtension::FindBestTargetBody() const
{
	// 优先使用 TargetHandler 的 FindTarget（复用距离、视锥、LoS 等全部过滤逻辑）
	if (UTargetHandlerBase* const Handler = GetLockOnTargetComponent()->GetTargetHandler())
	{
		const FFindTargetRequestResponse Response = Handler->FindTarget(FFindTargetRequestParams());
		if (Response.Target.TargetComponent)
		{
			return Response.Target.TargetComponent;
		}
	}

	// 无 TargetHandler 或未找到时，退而遍历 TargetManager
	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return nullptr;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamForward = PC->PlayerCameraManager->GetCameraRotation().Vector();

	UTargetComponent* BestTarget = nullptr;
	float BestAngle = 60.f; // 最多偏离视角 60 度

	for (UTargetComponent* Target : UTargetManager::Get(*GetWorld()).GetRegisteredTargets())
	{
		if (!Target->CanBeCaptured())
		{
			continue;
		}

		const FVector ToTarget = Target->GetSocketLocation(Target->GetDefaultSocket()) - CamLoc;
		const float DistSq = ToTarget.SizeSquared();

		if (DistSq > FMath::Square(3000.f) || DistSq < FMath::Square(100.f))
		{
			continue;
		}

		const float Angle = FMath::RadiansToDegrees(FMath::Acos(CamForward | ToTarget.GetSafeNormal()));

		if (Angle < BestAngle)
		{
			BestAngle = Angle;
			BestTarget = Target;
		}
	}

	return BestTarget;
}

void UPreTargetingExtension::CreatePreviewWidgets()
{
	DestroyPreviewWidgets();

	const TArray<FName>& Sockets = CachedTargetComponent->GetSockets();
	USceneComponent* const AttachParent = CachedTargetComponent->GetAssociatedComponent();

	if (!AttachParent)
	{
		return;
	}

	for (const FName& Socket : Sockets)
	{
		const FName WidgetName = MakeUniqueObjectName(this, UWidgetComponent::StaticClass(),
			*FString::Printf(TEXT("PreTarget_Preview_%s"), *Socket.ToString()));

		UWidgetComponent* WidgetComp = NewObject<UWidgetComponent>(this, WidgetName, RF_Transient);

		if (!WidgetComp)
		{
			continue;
		}

		WidgetComp->RegisterComponent();
		WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
		WidgetComp->SetDrawAtDesiredSize(true);
		WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		const APlayerController* const PC = GetPlayerController();
		if (PC && PC->IsLocalController())
		{
			WidgetComp->SetOwnerPlayer(PC->GetLocalPlayer());
		}

		WidgetComp->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		WidgetComp->SetRelativeLocation(CachedTargetComponent->WidgetRelativeOffset);

		// 优先用 PreviewWidgetClass，否则用 Target 的自定义控件，最后回退到默认 WBP_Target
		if (!PreviewWidgetClass.IsNull())
		{
			if (UClass* const LoadedClass = PreviewWidgetClass.Get())
			{
				WidgetComp->SetWidgetClass(LoadedClass);
			}
			else if (PreviewWidgetClass.IsPending())
			{
				UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
					PreviewWidgetClass.ToSoftObjectPath(),
					FStreamableDelegate::CreateLambda([WidgetComp, SoftPath = PreviewWidgetClass.ToSoftObjectPath()]()
					{
						if (IsValid(WidgetComp))
						{
							if (UClass* const Class = Cast<UClass>(SoftPath.ResolveObject()))
							{
								WidgetComp->SetWidgetClass(Class);
							}
						}
					}));
			}
		}
		else if (!CachedTargetComponent->CustomWidgetClass.IsNull())
		{
			if (UClass* const LoadedClass = CachedTargetComponent->CustomWidgetClass.Get())
			{
				WidgetComp->SetWidgetClass(LoadedClass);
			}
		}
		else
		{
			static const TSoftClassPtr<UUserWidget> DefaultWidgetClass(
				FSoftClassPath(FString(TEXT("/Script/UMGEditor.WidgetBlueprint'/LockOnTarget/WBP_Target.WBP_Target_C'"))));
			if (UClass* const LoadedClass = DefaultWidgetClass.Get())
			{
				WidgetComp->SetWidgetClass(LoadedClass);
			}
		}

		WidgetComp->SetVisibility(true);
		PreviewWidgets.Add(Socket, WidgetComp);
	}
}

void UPreTargetingExtension::DestroyPreviewWidgets()
{
	for (auto& [Socket, WidgetComp] : PreviewWidgets)
	{
		if (IsValid(WidgetComp))
		{
			WidgetComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			WidgetComp->DestroyComponent();
		}
	}

	PreviewWidgets.Empty();
}

void UPreTargetingExtension::UpdateSelection()
{
	APlayerController* const PC = GetPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamForward = PC->PlayerCameraManager->GetCameraRotation().Vector();

	FName BestSocket = NAME_None;
	float BestAngle = SelectionAngleThreshold;

	for (const auto& [Socket, WidgetComp] : PreviewWidgets)
	{
		const FVector SocketLoc = CachedTargetComponent->GetSocketLocation(Socket);
		const FVector Dir = (SocketLoc - CamLoc).GetSafeNormal();

		if (Dir.IsNearlyZero())
		{
			continue;
		}

		const float Angle = FMath::RadiansToDegrees(FMath::Acos(CamForward | Dir));

		if (Angle < BestAngle)
		{
			BestAngle = Angle;
			BestSocket = Socket;
		}
	}

	if (SelectedSocket != BestSocket)
	{
		const FName OldSocket = SelectedSocket;
		SelectedSocket = BestSocket;
		UpdateSelectionVisuals(OldSocket);
	}
}

void UPreTargetingExtension::UpdateSelectionVisuals(FName OldSelectedSocket)
{
	if (!SelectedWidgetClass.IsNull())
	{
		// 使用不同的 Widget 类区分选中/未选中
		// 将旧选中的 Socket 切回 PreviewWidgetClass
		if (OldSelectedSocket != NAME_None && OldSelectedSocket != SelectedSocket)
		{
			SetWidgetClassOnSocket(OldSelectedSocket, PreviewWidgetClass);
		}

		// 将新选中的 Socket 切成 SelectedWidgetClass
		if (SelectedSocket != NAME_None)
		{
			SetWidgetClassOnSocket(SelectedSocket, SelectedWidgetClass);
		}
	}
	else
	{
		// 未配置 SelectedWidgetClass，仅用透明度区分
		for (auto& [Socket, WidgetComp] : PreviewWidgets)
		{
			if (!IsValid(WidgetComp))
			{
				continue;
			}

			const bool bSelected = (Socket == SelectedSocket);
			WidgetComp->SetTintColorAndOpacity(bSelected
				? FLinearColor::White
				: FLinearColor(1.f, 1.f, 1.f, DeselectedOpacity));
		}
	}
}

void UPreTargetingExtension::SetWidgetClassOnSocket(FName Socket, const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	UWidgetComponent* const* Found = PreviewWidgets.Find(Socket);
	if (!Found || !IsValid(*Found))
	{
		return;
	}

	UWidgetComponent* WidgetComp = *Found;

	if (!WidgetClass.IsNull())
	{
		if (UClass* const LoadedClass = WidgetClass.Get())
		{
			WidgetComp->SetWidgetClass(LoadedClass);
		}
		else if (WidgetClass.IsPending())
		{
			UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
				WidgetClass.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([WidgetComp, SoftPath = WidgetClass.ToSoftObjectPath()]()
				{
					if (IsValid(WidgetComp))
					{
						if (UClass* const Class = Cast<UClass>(SoftPath.ResolveObject()))
						{
							WidgetComp->SetWidgetClass(Class);
						}
					}
				}));
		}
	}
	else
	{
		// 回退到默认或目标自定义控件
		if (!CachedTargetComponent->CustomWidgetClass.IsNull())
		{
			if (UClass* const LoadedClass = CachedTargetComponent->CustomWidgetClass.Get())
			{
				WidgetComp->SetWidgetClass(LoadedClass);
			}
		}
		else
		{
			static const TSoftClassPtr<UUserWidget> DefaultClass(
				FSoftClassPath(FString(TEXT("/Script/UMGEditor.WidgetBlueprint'/LockOnTarget/WBP_Target.WBP_Target_C'"))));
			if (UClass* const LoadedClass = DefaultClass.Get())
			{
				WidgetComp->SetWidgetClass(LoadedClass);
			}
		}
	}
}
