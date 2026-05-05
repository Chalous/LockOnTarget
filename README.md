# LockOnTarget

Fast and efficient plugin for Unreal Engine which gives the ability to lock onto a Target similar to Souls-like games. The system is divided into 2 components.

* [**LockOnTargetComponent**](https://github.com/J1blCblu/LockOnTarget/wiki/2.-LockOnTargetComponent-Overview) - gives a locally controlled AActor the ability to capture a Target along with a Socket. The Target is controlled through an optional [TargetHandler](https://github.com/J1blCblu/LockOnTarget/wiki/2.2-Weighted-Target-Handler). The component may contain a set of optional **Extensions** that can be used to add custom cosmetic features such as Target indication.

* [**TargetComponent**](https://github.com/J1blCblu/LockOnTarget/wiki/3.-TargetComponent-Overview) - Represents a Target that *LockOnTargetComponent* can capture in conjunction with a Socket. It is kind of a dumping ground for anything LockOnTarget subsystems may need.


# Features

* Capture *any Actor* with a TargetComponent with multiple sockets.
* Network synchronization.
* Flexible input processing settings.
* Target filtering.
* Auto Target finding in response to events.
* Target switching in screen space.
* [Debugger](https://github.com/J1blCblu/LockOnTarget/wiki/4.-Gameplay-Debugger-Overview).
* Per Target widget customization.


# Installation

* **Source** - Clone the [repository](https://github.com/J1blCblu/LockOnTarget) to the `Plugins` folder of the project.
Optionally add the **LockOnTarget** dependency to your build.cs file. Generate project files and build the project.

* **Unreal Marketplace** - Download from the [Marketplace](https://www.unrealengine.com/marketplace/en-US/product/lock-on-target) and `install` on a specific Engine version. `Enable` the plugin in the editor.


# Documentation

* [Initial setup](https://github.com/J1blCblu/LockOnTarget/wiki/1.-Initial-Setup).
* [Updates](https://github.com/J1blCblu/LockOnTarget/releases).


# Known Issues

List of known [Issues](https://github.com/J1blCblu/LockOnTarget/issues).


# Special Thanks

 * To [mklabs](https://github.com/mklabs). His great [Targeting System](https://github.com/mklabs/ue4-targetsystemplugin) plugin was the starting point.


# License

Source code of the plugin is licensed under MIT license, and other developers are encouraged to fork the repository, open issues & pull requests to help the development.


# Extend Detail

## Weighted Target Handler 参数详解

WeightedTargetHandler 是默认的目标处理器，通过四遍筛选（PrimarySampling → Solver → Sort → SecondarySampling）找到最佳目标。

### Auto Find（自动重锁）

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `AutoFindTargetFlags` | 全部勾选 | 目标因特定原因丢失时自动寻找新目标。BitMask：Destruction / DistanceFailure / LineOfSightFailure / StateInvalidation / SocketInvalidation |

### Weights（权重）

四个权重决定候选目标评分的影响力占比。

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `DistanceWeight` | 0.725 | 距离影响。值越大越优先锁定近距离目标 |
| `DeltaAngleWeight` | 0.275 | 角度影响。值越大越优先锁定视角中心的目标 |
| `PlayerInputWeight` | 0.1 | 摇杆输入影响。仅切换目标时生效，越接近摇杆方向分越低 |
| `TargetPriorityWeight` | 0.25 | 目标优先级影响。TargetComponent::Priority (0=最高, 1=最低) |

> 最多项设为 0 则该因子不参与评分。最终权重 = PureDefaultWeight × Σ(Weight × Normalizer × Factor)，分越低排名越靠前。

### Solver（求解器）

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `PureDefaultWeight` | 1000 | 权重缩放系数，最大权重值。不影响排序仅影响 Debugger 数值 |
| `DistanceMaxFactor` | 2420cm | 距离因子饱和上限。超过此距离 Factor = 1.0 |
| `DeltaAngleMaxFactor` | 45° | 角度因子饱和上限。超过此角度 Factor = 1.0 |
| `MinimumFactorThreshold` | 0.035 | 因子最小值，防止完美候选拿 0 分导致其他因子失效 |

### Distance（距离）

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `bDistanceCheck` | true | 总开关，关闭则无视所有距离限制 |
| `DefaultCaptureRadius` | 2200cm | 可锁定的最大半径。单目标可覆盖 |
| `LostRadiusScale` | 1.1x | 释放半径 = CaptureRadius × LostRadiusScale。1.1 倍迟滞防止边界来回 |
| `NearClipRadius` | 150cm | 最小距离，防止贴脸锁定导致相机异常 |
| `CaptureRadiusScale` | 1.0x | 全局缩放，0.8 则所有距离打八折 |

### View（视锥 & 屏幕）

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `ViewConeAngle` | 42° | 视锥半角，偏离视角中心超过此值直接剔除 |
| `ViewPitchOffset` | 10° | 视角方向向下偏移，补偿玩家习惯性向上看的偏差 |
| `ViewYawOffset` | 0° | 视角方向水平偏移 |
| `bScreenCapture` | false | 开启后目标必须投影到屏幕内才算有效 |
| `ScreenOffset` | (5%, 2.5%) | 屏幕边界压缩，防止选中屏幕边缘目标 |
| `bRecentRenderCheck` | true | 目标必须近期被渲染过，防止锁定墙后目标 |
| `RecentTolerance` | 0.1s | 近期渲染判定窗口 |

### Target Switching（切换目标）

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `PlayerInputAngularRange` | 60° | 切换时候选偏离摇杆方向超过此角度直接剔除。仅切换模式生效 |

### Line Of Sight（视线检测）

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `bLineOfSightCheck` | true | 是否启用射线检测 |
| `TraceCollisionChannel` | Visibility | 射线碰撞通道 |
| `LostTargetDelay` | 3s | 遮挡宽限期。目标被挡住后不立即释放 |
| `CheckInterval` | 0.2s | 遮挡检查间隔，非每帧以节省性能 |

### 筛选流程

```
FindTarget 调用
  ├─ PrimarySampling: 剔除距离/Vision Cone/输入角度/未渲染
  ├─ Solver: 四因子加权评分
  ├─ Sort: 按权重升序
  └─ SecondarySampling: 最终检查 LoS/屏幕/自定义
       → 第一个通过者即为结果
```

## Gameplay Camera Rotation Extension

为了适配Gameplay Camera，创建了一个Gameplay Camera Rotation Extension，用于替代Controller Rotation Extension。里面的Calculated Target Rotation和Smoothed Target Location用于平滑Gameplay Camera的旋转。

## Usage
1. 拓展使用
<img width="528" height="189" alt="image" src="https://github.com/user-attachments/assets/d492bc58-6153-45f2-813b-733cacdae553" />
2. 在CDE中将Gameplay Camera Rotation Extension里的两个变量写到相机黑板中
<img width="1109" height="675" alt="image" src="https://github.com/user-attachments/assets/9fd5b745-7bfb-433d-b52d-175bdcb094dc" />
3. 在Camera Rig中设置Auto Rotate Input 2D节点
<img width="1476" height="905" alt="image" src="https://github.com/user-attachments/assets/78ca9348-10d7-4b29-939f-13b0df3b0eb8" />
4. 在Camera Rig中设置Set Rotation节点
<img width="903" height="616" alt="image" src="https://github.com/user-attachments/assets/a9d387ee-38ac-40b0-a6d9-5221d10da12b" />


## Pre Targeting Extension

预瞄准扩展提供分步锁定流程，先选定目标身体，展示所有可锁定部位，再确认锁定。

### 蓝图 API

| 方法 | 说明 |
|------|------|
| `StartPreTargeting` | 找到视线中的目标，在所有 Socket 上显示预瞄准 UI。已锁定时自动释放镜头。 |
| `CommitTarget` | 锁定当前高亮的 Socket。 |
| `CancelPreTargeting` | 取消预瞄准，隐藏 UI。已有锁定时恢复原锁定。 |

### 配置项

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `PreviewWidgetClass` | WBP_Target | 未选中时的预瞄准控件类 |
| `SelectedWidgetClass` | (空) | 视线选中时切换为此控件，为空则用透明度区分 |
| `SelectionAngleThreshold` | 25° | 视线角度阈值，相机前向与 Socket 方向夹角小于此值视为选中 |
| `DeselectedOpacity` | 0.4 | 未选中 Widget 的透明度（仅 SelectedWidgetClass 为空时生效） |
| `BodySwitchCooldown` | 0.3s | 切换到新目标身体前的冷却时间，防止视线在目标边界抖动 |

### 使用方式

1. 在 LockOnTargetComponent 的 Default Extensions 中添加 PreTargetingExtension
2. 在蓝图中调用 `StartPreTargeting` 进入预瞄准模式
3. 移动视角选择部位（自动高亮最近视线的 Socket）
4. 调用 `CommitTarget` 确认锁定，或 `CancelPreTargeting` 取消


