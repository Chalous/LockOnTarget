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



