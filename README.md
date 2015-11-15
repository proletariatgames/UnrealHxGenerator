# UE4Haxe Extern Generator

This project takes UHT reflection information and generates Haxe extern definitions from it.
You don't need to use this in order to benefit from the externs; They are for now included in the main
project


## Installation

You will need to be able to *build unreal from sources* in order to install.
This plugin should be installed in your engine `Plugins` directory. (e.g. `Plugins/UE4HaxeExternGenerator`)
Once this is achieved, you need to add the following line to `Engine/Source/Programs/UnrealHeaderTool/UnrealHeaderTool.Target.cs`:

```c#
AdditionalPlugins.Add("UE4HaxeExternGenerator");
```

This should be done right after the following line:

```c#
AdditionalPlugins.Add("ScriptGeneratorPlugin");
```

Additionally, you must add the following line to `Engine/Programs/UnrealHeaderTool/Config/DefaultEngine.ini`, inside the `[Plugins]` section:

```
ProgramEnabledPlugins="UE4HaxeExternGenerator"
```

Once this is done, the plugin will always be available when you compile a project that has UE4Haxe installed.
However, in order to avoid continuous recompilation of projects, the extern generator only is activated when the environment variable `GENERATE_EXTERNS` is defined.
