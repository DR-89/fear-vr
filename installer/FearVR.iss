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

// Der eingegebene oder gefundene Pfad zeigt mal auf die Installationswurzel,
// mal auf Dev, mal direkt auf Dev\Runtime\Game. Geliefert wird das
// Verzeichnis mit der GameClient.dll, sonst ein leerer String.
function PublicToolsGameDir(const Path: String): String;
var
  Suffixes: array[0..3] of String;
  I: Integer;
  Candidate: String;
begin
  Result := '';
  if Path = '' then
    exit;
  Suffixes[0] := '';
  Suffixes[1] := 'Dev\Runtime\Game';
  Suffixes[2] := 'Runtime\Game';
  Suffixes[3] := 'Game';
  for I := 0 to 3 do
  begin
    if Suffixes[I] = '' then
      Candidate := Path
    else
      Candidate := AddBackslash(Path) + Suffixes[I];
    if FileExistsBelow(Candidate, 'GameClient.dll') then
    begin
      Result := Candidate;
      exit;
    end;
  end;
end;

function LooksLikePublicTools(const Path: String): Boolean;
begin
  Result := PublicToolsGameDir(Path) <> '';
end;

function NormalisePublicToolsPath(const Path: String): String;
begin
  Result := PublicToolsGameDir(Path);
  if Result = '' then
    Result := AddBackslash(Path) + 'Dev\Runtime\Game';
end;

function JoinPath(const Left, Right: String): String;
begin
  if Left = '' then
    Result := Right
  else if Right = '' then
    Result := Left
  else
    Result := AddBackslash(Left) + Right;
end;

procedure AddPath(List: TStringList; const Path: String);
begin
  if (Path <> '') and (List.IndexOf(Path) < 0) then
    List.Add(Path);
end;

// Jedes feste Laufwerk, das der Rechner tatsaechlich hat. Die Public Tools
// und das Spiel liegen oft nicht auf C:.
procedure AddDriveRoots(List: TStringList);
var
  I: Integer;
  Root: String;
begin
  for I := Ord('C') to Ord('Z') do
  begin
    Root := Chr(I) + ':';
    if DirExists(Root + '\') then
      AddPath(List, Root);
  end;
end;

// Die Uninstall-Registry kennt den vom Nutzer frei gewaehlten Zielordner,
// egal wo er liegt. Deshalb wird sie vor allen Standardpfaden befragt.
procedure AddRegistryInstallLocations(
  List: TStringList; const NameFilter: String);
var
  RootKeys: array[0..2] of Integer;
  Keys: TArrayOfString;
  Base, DisplayName, Location: String;
  K, I: Integer;
begin
  RootKeys[0] := HKLM32;
  RootKeys[1] := HKLM64;
  RootKeys[2] := HKCU;
  Base := 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall';
  for K := 0 to 2 do
  begin
    if not RegGetSubkeyNames(RootKeys[K], Base, Keys) then
      Continue;
    for I := 0 to GetArrayLength(Keys) - 1 do
    begin
      if not RegQueryStringValue(
        RootKeys[K], Base + '\' + Keys[I], 'DisplayName', DisplayName) then
        Continue;
      if Pos(Lowercase(NameFilter), Lowercase(DisplayName)) = 0 then
        Continue;
      if RegQueryStringValue(
        RootKeys[K], Base + '\' + Keys[I], 'InstallLocation', Location) then
        AddPath(List, RemoveBackslash(Trim(Location)));
    end;
  end;
end;

function SteamRoot(): String;
var
  Value: String;
begin
  Value := '';
  if not (RegQueryStringValue(
    HKLM32, 'SOFTWARE\Valve\Steam', 'InstallPath', Value) and (Value <> '')) then
    if not (RegQueryStringValue(
      HKLM64, 'SOFTWARE\Valve\Steam', 'InstallPath', Value) and (Value <> '')) then
      if not (RegQueryStringValue(
        HKCU, 'SOFTWARE\Valve\Steam', 'SteamPath', Value) and (Value <> '')) then
        Value := ExpandConstant('{pf32}\Steam');
  // HKCU\SteamPath steht mit Schraegstrichen in der Registry.
  StringChangeEx(Value, '/', '\', True);
  Result := RemoveBackslash(Value);
end;

// Steam-Bibliotheken aus libraryfolders.vdf. Ein einzelner fester Pfad
// findet nur die Standardbibliothek und keine zweite Platte.
procedure AddSteamLibraries(List: TStringList);
var
  Root, Vdf, Line, Path: String;
  Lines: TArrayOfString;
  I, P: Integer;
begin
  Root := SteamRoot;
  AddPath(List, Root);
  Vdf := AddBackslash(Root) + 'steamapps\libraryfolders.vdf';
  if not FileExists(Vdf) then
    exit;
  if not LoadStringsFromFile(Vdf, Lines) then
    exit;
  for I := 0 to GetArrayLength(Lines) - 1 do
  begin
    Line := Lines[I];
    P := Pos('"path"', Lowercase(Line));
    if P = 0 then
      Continue;
    Line := Copy(Line, P + 6, Length(Line));
    P := Pos('"', Line);
    if P = 0 then
      Continue;
    Line := Copy(Line, P + 1, Length(Line));
    P := Pos('"', Line);
    if P = 0 then
      Continue;
    Path := Copy(Line, 1, P - 1);
    StringChangeEx(Path, '\\', '\', True);
    AddPath(List, RemoveBackslash(Path));
  end;
end;

function FindRetailRoot(): String;
var
  Libraries, Candidates, Folders, Parents, Vendors, Drives: TStringList;
  I, J, K, L: Integer;
begin
  Result := '';
  Candidates := TStringList.Create;
  Libraries := TStringList.Create;
  Folders := TStringList.Create;
  Parents := TStringList.Create;
  Vendors := TStringList.Create;
  Drives := TStringList.Create;
  try
    Folders.Add('FEAR Ultimate Shooter Edition');
    Folders.Add('FEAR');
    Folders.Add('F.E.A.R');
    Folders.Add('F.E.A.R.');
    Folders.Add('FEAR Platinum Collection');

    AddSteamLibraries(Libraries);
    for I := 0 to Libraries.Count - 1 do
      for J := 0 to Folders.Count - 1 do
        AddPath(Candidates, JoinPath(
          Libraries[I], 'steamapps\common\' + Folders[J]));

    AddRegistryInstallLocations(Candidates, 'F.E.A.R');
    AddRegistryInstallLocations(Candidates, 'FEAR');

    Parents.Add('Program Files (x86)');
    Parents.Add('Program Files');
    Parents.Add('Games');
    Parents.Add('GOG Games');
    Parents.Add('SteamLibrary\steamapps\common');
    Parents.Add('Games\steamapps\common');
    Parents.Add('');
    Vendors.Add('');
    Vendors.Add('Sierra');
    Vendors.Add('Monolith Productions');
    Vendors.Add('Vivendi Games');
    AddDriveRoots(Drives);
    for I := 0 to Drives.Count - 1 do
      for J := 0 to Parents.Count - 1 do
        for K := 0 to Vendors.Count - 1 do
          for L := 0 to Folders.Count - 1 do
            AddPath(Candidates, JoinPath(JoinPath(JoinPath(
              Drives[I] + '\', Parents[J]), Vendors[K]), Folders[L]));

    for I := 0 to Candidates.Count - 1 do
      if LooksLikeRetailRoot(Candidates[I]) then
      begin
        Result := Candidates[I];
        Break;
      end;
  finally
    Drives.Free;
    Vendors.Free;
    Parents.Free;
    Folders.Free;
    Libraries.Free;
    Candidates.Free;
  end;
end;

// Der offizielle Public-Tools-Installer schlaegt Sierra vor, laesst den
// Zielordner aber frei waehlen. Ein einzelner fester Pfad reicht deshalb
// nicht: erst Registry, dann die ueblichen Ordnernamen je Laufwerk.
function FindPublicToolsRoot(): String;
var
  Candidates, Folders, Parents, Drives: TStringList;
  I, J, K: Integer;
begin
  Result := '';
  Candidates := TStringList.Create;
  Folders := TStringList.Create;
  Parents := TStringList.Create;
  Drives := TStringList.Create;
  try
    AddRegistryInstallLocations(Candidates, 'Public Tools');
    AddRegistryInstallLocations(Candidates, 'FEAR SDK');

    Folders.Add('Sierra\FEAR Public Tools');
    Folders.Add('Sierra\F.E.A.R. Public Tools');
    Folders.Add('Sierra Entertainment\FEAR Public Tools');
    Folders.Add('Monolith Productions\FEAR Public Tools');
    Folders.Add('FEAR Public Tools');
    Folders.Add('F.E.A.R. Public Tools');
    Parents.Add('Program Files (x86)');
    Parents.Add('Program Files');
    Parents.Add('Games');
    Parents.Add('');
    AddDriveRoots(Drives);
    for I := 0 to Drives.Count - 1 do
      for J := 0 to Parents.Count - 1 do
        for K := 0 to Folders.Count - 1 do
          AddPath(Candidates, JoinPath(JoinPath(
            Drives[I] + '\', Parents[J]), Folders[K]));

    for I := 0 to Candidates.Count - 1 do
      if LooksLikePublicTools(Candidates[I]) then
      begin
        Result := Candidates[I];
        Break;
      end;
  finally
    Drives.Free;
    Parents.Free;
    Folders.Free;
    Candidates.Free;
  end;
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
  RetailPage.Values[0] := FindRetailRoot();

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
