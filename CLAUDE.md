# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LockOnTarget is an Unreal Engine 5.6 plugin that provides a Souls-like target-locking system. Two core components drive it: `LockOnTargetComponent` (the lock-on controller on the player) and `TargetComponent` (marks an Actor as targetable with selectable sockets).

## Module Structure

| Module | Type | Purpose |
|--------|------|---------|
| `LockOnTarget` | Runtime | Core targeting logic, extensions, target handlers |
| `LockOnTargetDev` | DeveloperTool | Gameplay Debugger category for LockOnTarget |
| `LockOnTargetEditor` | Editor | Details customization for LockOnTargetComponent and TargetComponent |

## Architecture

### Core Components

- **`ULockOnTargetComponent`** (`LockOnTargetComponent.h:33`) — Placed on a locally-controlled Pawn. Captures/releases targets, processes player input for target switching, and hosts Extensions and a TargetHandler. Network-replicated (TargetInfo + duration sync via RPC).

- **`UTargetComponent`** (`TargetComponent.h:40`) — Placed on any Actor that can be locked onto. Stores available sockets, focus point settings, widget preferences, and priority. Not replicated. Supports multiple simultaneous lock-on "invaders" via `TInlineAllocator<3>`.

### Extension System

Extensions add optional cosmetic/behavioral features to LockOnTargetComponent. All inherit from the proxy chain:

```
UObject → ULockOnTargetExtensionProxy → ULockOnTargetExtensionBase → (concrete extensions)
                                                                     → UTargetHandlerBase → (concrete handlers)
```

`ULockOnTargetExtensionProxy` provides the shared tick infrastructure, initialization lifecycle, and callbacks (`OnTargetLocked`, `OnTargetUnlocked`, `OnSocketChanged`, `OnTargetNotFound`).

Built-in extensions in `Source/LockOnTarget/Public/LockOnTargetExtensions/`:
- `ControllerRotationExtension` — rotates the PlayerController toward the target
- **`GameplayCameraRotationExtension`** — replacement for ControllerRotationExtension designed for UE5 Gameplay Camera; computes smoothed target rotation and location that Camera Rig reads via Blueprint variables
- `PawnRotationExtension` — rotates the Pawn toward the target
- `CameraModifierExtension` — applies a camera modifier during lock-on
- `WidgetExtension` — displays a per-target UMG widget
- `TargetPreviewExtension` — preview widget for target switching

### TargetHandler System

`UTargetHandlerBase` (an ExtensionProxy subclass) handles target finding, state checking, and exception processing.

**`UWeightedTargetHandler`** is the primary implementation. It finds targets in 4 passes:
1. **PrimarySampling** — rejects invalid targets (distance, view cone, screen bounds, recent render, line of sight)
2. **Solver** — calculates weights for each target (distance, delta angle, player input direction, target priority)
3. **Sort** — ascending by weight (lowest = best)
4. **SecondarySampling** — picks the first target passing additional custom checks

Override `CalculateTargetWeight()` and `ShouldSkipTargetCustom()` to customize. Set `FFindTargetRequestParams::bGenerateDetailedResponse` to get the full scoring data via `UWeightedTargetHandlerDetailedResponse`.

### TargetManager

`UTargetManager` (`TargetManager.h:16`) is a `UWorldSubsystem` that maintains a `TSet<UTargetComponent*>` of all registered targets. Targets auto-register on `BeginPlay` and unregister on `EndPlay`.

### Key Data Types

- **`FTargetInfo`** — a `TargetComponent` + `Socket` pair, network-serializable
- **`FFindTargetRequestParams`** — input to `FindTarget()` (player input, detailed response flag, optional payload)
- **`FFindTargetRequestResponse`** — output from `FindTarget()` (found target + optional payload)
- **`FFindTargetContext`** — full context snapshot used during weighted target finding (view location, captured target, player input direction, etc.)
- **`FTargetContext`** — per-candidate data during target finding (location, direction, distance, delta angle, weight)
- **`ETargetExceptionType`** — Destruction, StateInvalidation, SocketInvalidation

### Input Processing

`LockOnTargetComponent` buffers analog input (`SwitchTargetYaw`/`SwitchTargetPitch`), compares against `InputBufferThreshold`, and triggers target switching when the threshold is exceeded. Supports input freezing (prevents rapid switching) and configurable delay after each switch.

### Network Model

- `CurrentTargetInternal` is `ReplicatedUsing = OnTargetInfoUpdated`
- `TargetingDuration` is `Replicated`
- Server RPC: `Server_UpdateTargetInfo` (reliable, with validation)
- `TargetComponent` itself is NOT replicated — use `CanBeReferencedOverNetwork()` to check

## Build

This is a standard UE plugin. Build through the Unreal project:

```bash
# Generate project files (if needed)
<UE_Root>/Engine/Build/BatchFiles/RunUAT.bat GenerateProjectFiles -project="<ProjectPath>.uproject" -game

# Build via UBT
<UE_Root>/Engine/Build/BatchFiles/RunUAT.bat BuildEditor -project="<ProjectPath>.uproject" -platform=Win64
```

Or build from the Unreal Editor directly (compile on open).

## Profiling

The plugin has custom CPU profiler tracing via the `LockOnTarget` channel. Run with:

```
-trace=default,LockOnTarget
```

Key macros defined in `LockOnTargetDefines.h`:
- `LOT_BOOKMARK(Name, ...)` — trace bookmark
- `LOT_SCOPED_EVENT(EventName)` — scoped CPU profiler event on the LockOnTarget channel

## Config

`Config/FilterPlugin.ini` provides plugin filtering configuration for Unreal Engine's plugin browser.
