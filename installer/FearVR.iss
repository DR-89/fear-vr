#ifndef PackageDir
  #error PackageDir must point to a prepared F.E.A.R. VR release package
#endif

#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

#ifndef AppVersion
  #define AppVersion "dev"
#endif

#define AppName "F.E.A.R. VR"
#define AppPublisher "F.E.A.R. VR contributors"
#define AppExeName "FearVR-Setup.exe"

[Setup]
AppId={{B65AC146-A725-46D0-96EB-15B470B4AC62}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\FearVR-Installer
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=FearVR-Setup-{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Uninstallable=no
CreateAppDir=no
SetupLogging=yes

[Files]
Source: "{#PackageDir}\*"; DestDir: "{tmp}\FearVRPackage"; Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
var
  RetailPage: TInputDirWizardPage;
  PublicToolsPage: TInputDirWizardPage;
  InstallPage: TInputDirWizardPage;
  OptionsPage: TInputOptionWizardPage;
  SummaryPage: TOutputMsgWizardPage;
  ResultPage: TOutputMsgMemoWizardPage;

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
  Result := FileExistsBelow(Path, 'FEAR.exe');
end;

function LooksLikePublicTools(const Path: String): Boolean;
begin
  Result :=
    FileExistsBelow(Path, 'GameClient.dll') or
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
  Candidate := ExpandConstant('{pf32}\Steam\steamapps\common\FEAR Ultimate Shooter Edition');
  if LooksLikeRetailRoot(Candidate) then
  begin
    Result := Candidate;
    exit;
  end;

  Candidate := 'C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition';
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
  Candidate := ExpandConstant('{pf32}\Monolith Productions\FEAR Public Tools');
  if LooksLikePublicTools(Candidate) then
  begin
    Result := Candidate;
    exit;
  end;

  Result := '';
end;

procedure InitializeWizard;
var
  RetailDefault: String;
  ToolsDefault: String;
begin
  RetailDefault := FindSteamFearRoot();
  ToolsDefault := FindPublicToolsRoot();

  RetailPage := CreateInputDirPage(
    wpSelectDir,
    'Locate F.E.A.R. 1.08',
    'Select the retail F.E.A.R. installation folder.',
    'Choose the folder containing FEAR.exe. The original game installation is read but not modified.',
    False,
    '');
  RetailPage.Add('F.E.A.R. folder:');
  RetailPage.Values[0] := RetailDefault;

  PublicToolsPage := CreateInputDirPage(
    RetailPage.ID,
    'Locate F.E.A.R. Public Tools 1.08',
    'Select the Public Tools installation folder.',
    'Choose the Public Tools root or its Dev\Runtime\Game folder. Required proprietary modules are copied from your local installation.',
    False,
    '');
  PublicToolsPage.Add('Public Tools folder:');
  PublicToolsPage.Values[0] := ToolsDefault;

  InstallPage := CreateInputDirPage(
    PublicToolsPage.ID,
    'Choose the F.E.A.R. VR installation folder',
    'The mod is installed separately from the retail game.',
    'Choose where the isolated playable F.E.A.R. VR installation should be created.',
    False,
    '');
  InstallPage.Add('Install folder:');
  InstallPage.Values[0] := AddBackslash(GetEnv('USERPROFILE')) + 'FearVR';

  OptionsPage := CreateInputOptionPage(
    InstallPage.ID,
    'Installation options',
    'Choose optional installation behaviour.',
    'The normal defaults are recommended.',
    True,
    False);
  OptionsPage.Add('Create a desktop shortcut');
  OptionsPage.Add('Clean the existing installation before staging');
  OptionsPage.SelectedValueIndex := 0;
  OptionsPage.Values[0] := True;
  OptionsPage.Values[1] := False;

  SummaryPage := CreateOutputMsgPage(
    OptionsPage.ID,
    'Ready to install',
    'Review the selected installation settings.',
    'The installer will validate the selected folders and run the existing F.E.A.R. VR installation script.');

  ResultPage := CreateOutputMsgMemoPage(
    SummaryPage.ID,
    'Installation complete',
    'F.E.A.R. VR was installed successfully.',
    'You can now launch it using the desktop shortcut or play.ps1 in the installation folder.',
    '');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  if CurPageID = RetailPage.ID then
  begin
    if not LooksLikeRetailRoot(RetailPage.Values[0]) then
    begin
      MsgBox('FEAR.exe was not found in the selected retail folder.', mbError, MB_OK);
      Result := False;
      exit;
    end;
  end;

  if CurPageID = PublicToolsPage.ID then
  begin
    if not LooksLikePublicTools(PublicToolsPage.Values[0]) then
    begin
      MsgBox('GameClient.dll was not found in the selected Public Tools folder or its Dev\Runtime\Game subfolder.', mbError, MB_OK);
      Result := False;
      exit;
    end;
  end;

  if CurPageID = InstallPage.ID then
  begin
    if CompareText(InstallPage.Values[0], RetailPage.Values[0]) = 0 then
    begin
      MsgBox('The F.E.A.R. VR install folder must be separate from the retail F.E.A.R. folder.', mbError, MB_OK);
      Result := False;
      exit;
    end;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
var
  ShortcutText: String;
  CleanText: String;
begin
  if CurPageID = SummaryPage.ID then
  begin
    if OptionsPage.Values[0] then ShortcutText := 'Yes' else ShortcutText := 'No';
    if OptionsPage.Values[1] then CleanText := 'Yes' else CleanText := 'No';

    SummaryPage.MsgLabel.Caption :=
      'Retail F.E.A.R.:' + #13#10 + RetailPage.Values[0] + #13#10#13#10 +
      'Public Tools:' + #13#10 + NormalisePublicToolsPath(PublicToolsPage.Values[0]) + #13#10#13#10 +
      'F.E.A.R. VR install folder:' + #13#10 + InstallPage.Values[0] + #13#10#13#10 +
      'Desktop shortcut: ' + ShortcutText + #13#10 +
      'Clean installation: ' + CleanText;
  end;
end;

function RunInstallerScript(var OutputText: String): Boolean;
var
  ScriptPath: String;
  PowerShellPath: String;
  Params: String;
  ResultCode: Integer;
  LogPath: String;
  LogContents: AnsiString;
begin
  ScriptPath := ExpandConstant('{tmp}\FearVRPackage\tools\install.ps1');
  PowerShellPath := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
  LogPath := ExpandConstant('{tmp}\FearVR-install.log');

  if not FileExists(ScriptPath) then
  begin
    OutputText := 'The release package does not contain tools\install.ps1.';
    Result := False;
    exit;
  end;

  Params :=
    '-NoLogo -NoProfile -ExecutionPolicy Bypass -File ' + Quote(ScriptPath) +
    ' -RetailRoot ' + Quote(RetailPage.Values[0]) +
    ' -PublicToolsGame ' + Quote(NormalisePublicToolsPath(PublicToolsPage.Values[0])) +
    ' -InstallDir ' + Quote(InstallPage.Values[0]) +
    ' -NonInteractive';

  if not OptionsPage.Values[0] then
    Params := Params + ' -NoShortcut';

  if OptionsPage.Values[1] then
    Params := Params + ' -Clean';

  Params := '-Command "& ' + Quote(PowerShellPath) + ' ' + Params +
    ' *>&1 | Tee-Object -FilePath ' + Quote(LogPath) + '; exit $LASTEXITCODE"';

  Result := Exec(PowerShellPath, Params, ExpandConstant('{tmp}\FearVRPackage'), SW_HIDE, ewWaitUntilTerminated, ResultCode);

  if FileExists(LogPath) and LoadStringFromFile(LogPath, LogContents) then
    OutputText := LogContents
  else
    OutputText := 'The installer returned exit code ' + IntToStr(ResultCode) + '.';

  Result := Result and (ResultCode = 0);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  OutputText: String;
begin
  WizardForm.StatusLabel.Caption := 'Installing F.E.A.R. VR...';
  WizardForm.ProgressGauge.Style := npbstMarquee;

  if RunInstallerScript(OutputText) then
  begin
    ResultPage.RichEditViewer.Text := OutputText;
    Result := '';
  end
  else
  begin
    Result :=
      'F.E.A.R. VR could not be installed.' + #13#10#13#10 +
      OutputText;
  end;
end;
