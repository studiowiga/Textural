; Inno Setup script - installs Textural.vst3 into the standard VST3 folder
[Setup]
AppName=Textural
AppVersion=1.0.1
AppPublisher=Studio Wiga
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=Textural-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
Uninstallable=yes
UninstallDisplayName=Textural (Studio Wiga) VST3

[Files]
Source: "dist\Textural.vst3\*"; DestDir: "{commoncf64}\VST3\Textural.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
