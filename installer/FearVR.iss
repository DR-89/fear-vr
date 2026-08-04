#ifndef PackageDir
  #error PackageDir must point to a prepared F.E.A.R. VR retail-overlay package
#endif

#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

#ifndef AppVersion
  #define AppVersion "dev"
#endif

#define AppName "F.E.A.R. VR"
#define AppPublisher "F.E.A.R. VR contributors"

[Setup]
AppId={{B65AC146-A725-46D0-96EB-15B470B4AC62}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={code:GetRetailRoot}
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=FearVR-Setup-{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Uninstallable=no
CreateAppDir=no
SetupLogging=yes

[Files]
Source: "{#PackageDir}\*"; DestDir: "{code:GetRetailRoot}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\F.E.A.R. VR"; Filename: "{code:GetRetailLauncher}"; WorkingDir: "{code:GetRetailRoot}"; Check: ShouldCreateShortcut

[Code]
var
  RetailPage: TInputDirWizardPage;
  PublicToolsPage: TInputDirWizardPage;
  OptionsPage: TInputOptionWizardPage;
  SummaryPage: TOutputMsgWizardPage;

function Quote(const Value: String): String;
begin
  Result := '"' + Value + '"';
end;

function FileExistsBelow(const Root, RelativePath: String): Boolean;
begin
  Result := FileExists(AddBackslash(Root) + RelativePath);
end;

function LooksLikeRetailRoot(const Path: String): Boolean;
begin
  Result := FileExistsBelow(Path, 'FEAR.exe') and
    FileExistsBelow(Path, 'Default.archcfg');
end;

function LooksLikePublicTools(const Path: String): Boolean;
begin
  Result := FileExistsBelow(Path, 'GameClient.dll') or
    FileExistsBelow(Path, 'Dev\Runtime\Game\GameClient.dll');
end;

function NormalisePublicToolsPath(const Path: String): String;
begin
  if FileExistsBelow(Path, 'GameClient.dll') then
    Result := Path
  else
    Result := AddBackslash(Path) + 'Dev\Runtime\Game';
end;

function FindSteamFearRoot(): String;
var
  Candidate: String;
begin
  Candidate := ExpandConstant(
    '{pf32}\Steam\steamapps\common\FEAR Ultimate Shooter Edition');
  if LooksLikeRetailRoot(Candidate) then
  begin
    Result := Candidate;
    exit;
  end;
  Result := '';
end;

function FindPublicToolsRoot(): String;
var
  Candidate: String;
begin
  Candidate := ExpandConstant(
    '{pf32}\Monolith Productions\FEAR Public Tools');
  if LooksLikePublicTools(Candidate) then
  begin
    Result := Candidate;
    exit;
  end;
  Result := '';
end;

function GetRetailRoot(Param: String): String;
begin
  Result := RetailPage.Values[0];
end;

function GetRetailLauncher(Param: String): String;
begin
  Result := AddBackslash(GetRetailRoot('')) + 'Start F.E.A.R. VR.cmd';
end;

function ShouldCreateShortcut(): Boolean;
begin
  Result := OptionsPage.Values[0];
end;

procedure InitializeWizard;
begin
  RetailPage := CreateInputDirPage(
    wpSelectDir,
    'Locate F.E.A.R. 1.08',
    'Select the retail F.E.A.R. installation folder.',
    'Choose the folder containing FEAR.exe and Default.archcfg. The VR overlay is installed into this folder without replacing retail game archives.',
    False,
    '');
  RetailPage.Add('F.E.A.R. folder:');
  RetailPage.Values[0] := FindSteamFearRoot();

  PublicToolsPage := CreateInputDirPage(
    RetailPage.ID,
    'Locate F.E.A.R. Public Tools 1.08',
    'Select the Public Tools installation folder.',
    'Choose the Public Tools root or its Dev\Runtime\Game folder. Proprietary modules are copied only from your local installation.',
    False,
    '');
  PublicToolsPage.Add('Public Tools folder:');
  PublicToolsPage.Values[0] := FindPublicToolsRoot();

  OptionsPage := CreateInputOptionPage(
    PublicToolsPage.ID,
    'Installation options',
    'Choose optional installation behaviour.',
    'The default is recommended.',
    False,
    False);
  OptionsPage.Add('Create a desktop shortcut');
  OptionsPage.Values[0] := True;

  SummaryPage := CreateOutputMsgPage(
    OptionsPage.ID,
    'Ready to install',
    'Review the selected folders.',
    'The installer copies the verified retail-overlay package and prepares it using your local Public Tools installation. Existing dinput8.dll or d3d9.dll proxy mods in the selected game folder will be replaced.');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = RetailPage.ID) and
     (not LooksLikeRetailRoot(RetailPage.Values[0])) then
  begin
    MsgBox(
      'FEAR.exe or Default.archcfg was not found in the selected folder.',
      mbError, MB_OK);
    Result := False;
    exit;
  end;
  if (CurPageID = PublicToolsPage.ID) and
     (not LooksLikePublicTools(PublicToolsPage.Values[0])) then
  begin
    MsgBox(
      'GameClient.dll was not found in the selected Public Tools folder or its Dev\Runtime\Game subfolder.',
      mbError, MB_OK);
    Result := False;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
var
  ShortcutText: String;
begin
  if CurPageID = SummaryPage.ID then
  begin
    if OptionsPage.Values[0] then
      ShortcutText := 'Yes'
    else
      ShortcutText := 'No';
    SummaryPage.MsgLabel.Caption :=
      'Retail F.E.A.R.:' + #13#10 + RetailPage.Values[0] + #13#10#13#10 +
      'Public Tools:' + #13#10 +
      NormalisePublicToolsPath(PublicToolsPage.Values[0]) + #13#10#13#10 +
      'Desktop shortcut: ' + ShortcutText;
  end;
end;

function RunOverlayPreparation(): Boolean;
var
  ScriptPath: String;
  PowerShellPath: String;
  Params: String;
  ResultCode: Integer;
  RetailRoot: String;
  InstallDir: String;
begin
  RetailRoot := GetRetailRoot('');
  InstallDir := AddBackslash(RetailRoot) + 'FEARVR';
  ScriptPath := AddBackslash(InstallDir) +
    'tools\prepare-overlay.ps1';
  PowerShellPath :=
    ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
  if not FileExists(ScriptPath) then
  begin
    Log('Overlay preparation script is missing: ' + ScriptPath);
    Result := False;
    exit;
  end;

  Params :=
    '-NoLogo -NoProfile -ExecutionPolicy Bypass -File ' +
    Quote(ScriptPath) +
    ' -InstallDir ' + Quote(InstallDir) +
    ' -RetailRoot ' + Quote(RetailRoot) +
    ' -PublicToolsGame ' +
    Quote(NormalisePublicToolsPath(PublicToolsPage.Values[0])) +
    ' -Force';
  Result := Exec(
    PowerShellPath, Params, RetailRoot, SW_HIDE,
    ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
  if not Result then
    Log('prepare-overlay.ps1 failed with exit code ' +
      IntToStr(ResultCode));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    WizardForm.StatusLabel.Caption :=
      'Preparing F.E.A.R. VR from the local Public Tools installation...';
    if not RunOverlayPreparation() then
      RaiseException(
        'F.E.A.R. VR files were copied, but overlay preparation failed. See the setup log for details.');
  end;
end;
