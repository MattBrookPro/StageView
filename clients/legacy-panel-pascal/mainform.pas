unit mainform;
{$mode objfpc}{$H+}
{
  The legacy control panel - a classic native-Windows form, the kind of GUI Delphi
  built for this job for twenty years: scene-recall buttons, trackbar faders, mute
  checkboxes and progress-bar meters. Controls are created in code (no .lfm) so the
  whole thing is one readable unit. It speaks OSC to the StageView engine via oscnet.
}

interface

uses
  Classes, SysUtils, Forms, Controls, StdCtrls, ComCtrls, ExtCtrls, Graphics,
  oscnet;

type
  TChanRow = record
    Name: TLabel;
    Fader: TTrackBar;
    Pan: TTrackBar;
    Mute: TCheckBox;
    Meter: TProgressBar;
  end;

  TfrmMain = class(TForm)
  private
    FStatus: TLabel;
    FTimer: TTimer;
    FRows: array of TChanRow;
    FCount: Integer;
    FConnected: Boolean;
    FUpdating: Boolean;             // guards programmatic control changes
    procedure BuildRows(N: Integer);
    procedure DoTimer(Sender: TObject);
    procedure FaderChanged(Sender: TObject);
    procedure PanChanged(Sender: TObject);
    procedure MuteChanged(Sender: TObject);
    procedure SceneShowtime(Sender: TObject);
    procedure SceneSoundcheck(Sender: TObject);
    procedure SceneSilence(Sender: TObject);
    procedure RecallScene(const SceneName: string; const Levels: array of Single; Muted: Boolean);
  public
    constructor Create(AOwner: TComponent); override;
    destructor Destroy; override;
  end;

var
  frmMain: TfrmMain;

implementation

const
  ROW_TOP = 96;
  ROW_H   = 34;
  // Showtime balance (matches the stems: Vocal, Drums, Bass, Gtr L/R/Arr, Synth, Perc)
  SHOWTIME: array[0..7] of Single = (0.90, 0.85, 0.82, 0.66, 0.66, 0.60, 0.62, 0.58);

function MakeButton(AParent: TWinControl; const Caption: string; X, W: Integer;
  OnClick: TNotifyEvent): TButton;
begin
  Result := TButton.Create(AParent);
  Result.Parent := AParent;
  Result.Caption := Caption;
  Result.SetBounds(X, 40, W, 25);
  Result.OnClick := OnClick;
end;

procedure MakeHeader(AParent: TWinControl; X: Integer; const C: string);
begin
  with TLabel.Create(AParent) do
  begin
    Parent := AParent;
    Caption := C;
    SetBounds(X, 74, 90, 14);
  end;
end;

constructor TfrmMain.Create(AOwner: TComponent);
var title: TLabel;
begin
  inherited CreateNew(AOwner);
  Caption := 'StageView - Console Control';
  Width := 728;
  Height := 420;
  Position := poScreenCenter;
  BorderStyle := bsSingle;          // fixed-size, classic dialog feel
  Font.Name := 'MS Sans Serif';     // the authentic period typeface
  Font.Size := 8;

  title := TLabel.Create(Self);
  title.Parent := Self;
  title.Caption := 'StageView  -  Console Control';
  title.Font.Style := [fsBold];
  title.Font.Size := 11;
  title.SetBounds(12, 10, 360, 22);

  FStatus := TLabel.Create(Self);
  FStatus.Parent := Self;
  FStatus.Caption := 'offline';
  FStatus.SetBounds(490, 14, 100, 18);

  MakeButton(Self, 'Showtime',   12,  92, @SceneShowtime);
  MakeButton(Self, 'Soundcheck', 110, 92, @SceneSoundcheck);
  MakeButton(Self, 'Silence',    208, 92, @SceneSilence);

  MakeHeader(Self, 12,  'Channel');
  MakeHeader(Self, 84,  'Level');
  MakeHeader(Self, 266, 'Pan (L - R)');
  MakeHeader(Self, 392, 'Mute');
  MakeHeader(Self, 452, 'Meter');

  // Connect + subscribe; rows are built when the engine sends its channel count.
  OscInit('127.0.0.1', 9000);
  OscSendBare('/subscribe');

  FTimer := TTimer.Create(Self);
  FTimer.Interval := 50;            // 20 Hz: drain OSC + refresh meters
  FTimer.OnTimer := @DoTimer;
  FTimer.Enabled := True;
end;

destructor TfrmMain.Destroy;
begin
  OscSendBare('/unsubscribe');
  OscClose;
  inherited Destroy;
end;

procedure TfrmMain.BuildRows(N: Integer);
var i, y: Integer;
begin
  FCount := N;
  SetLength(FRows, N);
  for i := 0 to N - 1 do
  begin
    y := ROW_TOP + i * ROW_H;
    FRows[i].Name := TLabel.Create(Self);
    FRows[i].Name.Parent := Self;
    FRows[i].Name.Caption := Format('Ch %d', [i + 1]);
    FRows[i].Name.SetBounds(12, y + 6, 70, 16);

    FRows[i].Fader := TTrackBar.Create(Self);
    FRows[i].Fader.Parent := Self;
    FRows[i].Fader.Min := 0;
    FRows[i].Fader.Max := 100;
    FRows[i].Fader.Tag := i;
    FRows[i].Fader.SetBounds(82, y, 170, 28);
    FRows[i].Fader.OnChange := @FaderChanged;

    FRows[i].Pan := TTrackBar.Create(Self);
    FRows[i].Pan.Parent := Self;
    FRows[i].Pan.Min := -100;          // -100 = hard left, 0 = centre, +100 = hard right
    FRows[i].Pan.Max := 100;
    FRows[i].Pan.Position := 0;
    FRows[i].Pan.Frequency := 100;      // a tick at centre
    FRows[i].Pan.Tag := i;
    FRows[i].Pan.SetBounds(262, y, 120, 28);
    FRows[i].Pan.OnChange := @PanChanged;

    FRows[i].Mute := TCheckBox.Create(Self);
    FRows[i].Mute.Parent := Self;
    FRows[i].Mute.Caption := 'Mute';
    FRows[i].Mute.Tag := i;
    FRows[i].Mute.SetBounds(392, y + 4, 54, 20);
    FRows[i].Mute.OnChange := @MuteChanged;

    FRows[i].Meter := TProgressBar.Create(Self);
    FRows[i].Meter.Parent := Self;
    FRows[i].Meter.Min := 0;
    FRows[i].Meter.Max := 100;
    FRows[i].Meter.SetBounds(452, y + 6, 256, 14);
  end;
end;

procedure TfrmMain.FaderChanged(Sender: TObject);
var i: Integer;
begin
  if FUpdating then Exit;
  i := TTrackBar(Sender).Tag;
  OscSendFloat(Format('/channel/%d/level', [i]), TTrackBar(Sender).Position / 100.0);
end;

procedure TfrmMain.PanChanged(Sender: TObject);
var i: Integer;
begin
  if FUpdating then Exit;
  i := TTrackBar(Sender).Tag;
  OscSendFloat(Format('/channel/%d/pan', [i]), TTrackBar(Sender).Position / 100.0);
end;

procedure TfrmMain.MuteChanged(Sender: TObject);
var i: Integer;
begin
  if FUpdating then Exit;
  i := TCheckBox(Sender).Tag;
  OscSendInt(Format('/channel/%d/mute', [i]), Ord(TCheckBox(Sender).Checked));
end;

procedure TfrmMain.RecallScene(const SceneName: string; const Levels: array of Single; Muted: Boolean);
var i: Integer; lvl: Single;
begin
  for i := 0 to FCount - 1 do
  begin
    if Length(Levels) > 0 then lvl := Levels[i mod Length(Levels)] else lvl := 0;
    OscSendFloat(Format('/channel/%d/level', [i]), lvl);
    OscSendInt(Format('/channel/%d/mute', [i]), Ord(Muted));
    FUpdating := True;             // reflect in the UI without re-sending
    FRows[i].Fader.Position := Round(lvl * 100);
    FRows[i].Mute.Checked := Muted;
    FUpdating := False;
  end;
  FStatus.Caption := 'scene: ' + SceneName;
end;

procedure TfrmMain.SceneShowtime(Sender: TObject);
begin
  if FCount > 0 then RecallScene('Showtime', SHOWTIME, False);
end;

procedure TfrmMain.SceneSoundcheck(Sender: TObject);
var flat: array[0..0] of Single;
begin
  flat[0] := 0.7;
  if FCount > 0 then RecallScene('Soundcheck', flat, False);
end;

procedure TfrmMain.SceneSilence(Sender: TObject);
var zero: array[0..0] of Single;
begin
  zero[0] := 0.0;
  if FCount > 0 then RecallScene('Silence', zero, True);
end;

procedure TfrmMain.DoTimer(Sender: TObject);
var msg: TOscMsg; parts: TStringArray; idx, k: Integer;
begin
  // Drain everything waiting this tick.
  while OscReceive(msg) do
  begin
    if not FConnected then
    begin
      FConnected := True;
      FStatus.Caption := 'connected';
    end;

    if msg.Address = '/stage/channels' then
    begin
      if (FCount = 0) and (msg.Count > 0) then BuildRows(msg.Args[0].I);
    end
    else if msg.Address = '/meters' then
    begin
      for k := 0 to msg.Count - 1 do
        if k <= High(FRows) then
          FRows[k].Meter.Position := Round(msg.Args[k].F * 100);
    end
    else if Copy(msg.Address, 1, 9) = '/channel/' then
    begin
      parts := msg.Address.Split('/');     // '', 'channel', '<n>', 'param', ...
      if (Length(parts) >= 4) and (msg.Count > 0) then
      begin
        idx := StrToIntDef(parts[2], -1);
        if (idx >= 0) and (idx <= High(FRows)) then
        begin
          FUpdating := True;
          if parts[3] = 'name' then
            FRows[idx].Name.Caption := msg.Args[0].S
          else if parts[3] = 'level' then
            FRows[idx].Fader.Position := Round(msg.Args[0].F * 100)
          else if parts[3] = 'pan' then
            FRows[idx].Pan.Position := Round(msg.Args[0].F * 100)
          else if parts[3] = 'mute' then
            FRows[idx].Mute.Checked := msg.Args[0].I <> 0;
          FUpdating := False;
        end;
      end;
    end;
  end;
end;

end.
