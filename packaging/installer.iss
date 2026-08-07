; ============================================================================
; TannoyBox — Windows installer
;
; Build the plugin in Release first, then open this file in Inno Setup
; (free, from jrsoftware.org) and press Compile. Output lands in
; packaging/output/TannoyBox-1.0.0-Windows.exe — one file to hand to someone
; who has never heard of GitHub.
;
; What it does: copies the VST3 bundle to the standard shared VST3 folder and
; the standalone app to Program Files, adds a Start Menu entry, and registers
; a proper uninstaller. It requires admin because Program Files does.
;
; It does NOT sign anything. See README.md for what that means for the person
; downloading it, and what signing would cost.
; ============================================================================

#define AppName        "TannoyBox"
#define AppVersion     "1.0.0"
#define AppPublisher   "Indelicate Instruments"
#define AppURL         "https://indelicates.xyz"
#define BuildDir       "..\build\TannoyBox_artefacts\Release"

[Setup]
AppId={{8A6F1C42-3D9E-4B77-9E21-TANNOYBOX001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
DefaultDirName={commonpf}\{#AppPublisher}\{#AppName}
DefaultGroupName={#AppPublisher}
OutputDir=output
OutputBaseFilename={#AppName}-{#AppVersion}-Windows
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
DisableProgramGroupPage=yes
LicenseFile=..\LICENCE.txt
UninstallDisplayName={#AppName} {#AppVersion}

[Types]
Name: "full";   Description: "Everything"
Name: "custom"; Description: "Choose what to install"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin";        Types: full custom; Flags: checkablealone
Name: "standalone"; Description: "Standalone app";     Types: full custom

[Files]
; The VST3 is a folder, not a file — recursesubdirs matters.
Source: "{#BuildDir}\VST3\{#AppName}.vst3\*"; \
    DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

Source: "{#BuildDir}\Standalone\{#AppName}.exe"; \
    DestDir: "{app}"; Flags: ignoreversion; Components: standalone

Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"

[Messages]
; Worth saying out loud, because the alternative is an email asking where it went.
FinishedLabel=TannoyBox is installed.%n%nThe VST3 is in the shared plugin folder. Your DAW will not see it until you rescan plugins — in most hosts that is a button in the plugin preferences.
