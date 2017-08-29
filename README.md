# Unreal.hx Extern Generator

This project takes UHT reflection information and generates Haxe extern definitions from it.
You don't need to use this in order to benefit from the externs; They are for now included in the main
project


## Installation (4.11 and above)

This plugin should be installed in your engine `Plugins` directory. (e.g. `Plugins/UnrealHxGenerator`)
Once this is achieved, add the plugin to your project, either through the editor or by inserting the following into your `.uproject` file:

```
  "Plugins": [{
      "Name": "UnrealHxGenerator",
      "Enabled": true
    }]
```

Set the `EXTERN_OUTPUT_DIR` environment variable to the location you wish to output the generated files to, and set the `GENERATE_EXTERNS` environment variable to 1

### For downloaded engines

If you haven't built the engine yourself, you will need to build the plugin using UAT. To do that, do the following:

```sh
cd [UE-Install-Dir]
.\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=[/path/to/UnrealHxGenerator.uplugin] -Package=[TemporaryOutputDirectory]
```

Once it is built, copy `TemporaryOutputDirectory` to your `[UE-Install-Dir]/Engine/Plugins/UnrealHxGenerator`

## Installation (pre 4.11)

You will need to be able to *build unreal from sources* in order to install.
This plugin should be installed in your engine `Plugins` directory. (e.g. `Plugins/UnrealHxGenerator`)
Once this is achieved, you need to add the following line to `Engine/Source/Programs/UnrealHeaderTool/UnrealHeaderTool.Target.cs`:

```c#
AdditionalPlugins.Add("UnrealHxGenerator");
```

This should be done right after the following line:

```c#
AdditionalPlugins.Add("ScriptGeneratorPlugin");
```

Additionally, you must add the following line to `Engine/Programs/UnrealHeaderTool/Config/DefaultEngine.ini`, inside the `[Plugins]` section:

```
ProgramEnabledPlugins="UnrealHxGenerator"
```

Set the `EXTERN_OUTPUT_DIR` environment variable to the location you wish to output the generated files to.
