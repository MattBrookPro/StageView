program stagectl;
{$mode objfpc}{$H+}
{
  StageView legacy console control  -  Object Pascal (Free Pascal / Delphi-compatible).

  Stands in for the kind of mature, native control application that has driven
  digital consoles for years: plain, fast, native code that just keeps compiling and
  running. It speaks the same OSC/UDP as everything else in StageView, and its
  headline feature is SCENE RECALL - pushing a stored set of channel states to the
  engine in one action, exactly what an operator does between songs or shows.

  Object Pascal's pitch in a stack like this is longevity, so this is written the
  way that code is: procedural, explicit, easy to read in ten years.

  Usage:
    stagectl                  recall the built-in "Showtime" scene
    stagectl soundcheck       recall a flat soundcheck scene (all even, nothing muted)
    stagectl silence          mute every channel
    stagectl set <ch> <0..1>  set a single channel's level
}

uses
  SysUtils, Classes, Sockets {$IFDEF WINDOWS}, WinSock2 {$ENDIF};

const
  ENGINE_HOST = '127.0.0.1';
  ENGINE_PORT = 9000;
  MAX_CH      = 8;

type
  TChannel = record
    Level: Single;   // 0..1
    Mute:  Boolean;
  end;
  TScene = record
    Name:  string;
    Count: Integer;
    Chan:  array[0..MAX_CH - 1] of TChannel;
  end;

var
  Sock:   LongInt;
  Engine: TInetSockAddr;

{ --- OSC encoding (the wire format, in Pascal) --- }

// host-order 16-bit -> network order (manual htons; no reliance on platform headers)
function ToBE16(v: Word): Word;
begin
  ToBE16 := ((v and $00FF) shl 8) or ((v and $FF00) shr 8);
end;

// host-order 32-bit -> network order (big-endian) for int32 / float32 payloads
function ToBE32(v: LongWord): LongWord;
begin
  ToBE32 := ((v and $000000FF) shl 24) or ((v and $0000FF00) shl 8)
         or ((v and $00FF0000) shr 8)  or ((v and $FF000000) shr 24);
end;

// reinterpret a Single's bit pattern as a LongWord, so we can byte-swap it
function FloatBits(f: Single): LongWord;
var u: record case Boolean of True: (s: Single); False: (l: LongWord); end;
begin
  u.s := f;
  FloatBits := u.l;
end;

// OSC strings are null-terminated and padded with nulls to a 4-byte boundary
procedure WriteOscString(St: TStream; const S: string);
var
  zero: Byte;
  i, fieldLen, pad: Integer;
begin
  zero := 0;
  if Length(S) > 0 then
    St.Write(S[1], Length(S));
  St.Write(zero, 1);                       // terminator
  fieldLen := Length(S) + 1;
  pad := (4 - (fieldLen mod 4)) mod 4;
  for i := 1 to pad do
    St.Write(zero, 1);
end;

procedure WriteBE32(St: TStream; v: LongWord);
var be: LongWord;
begin
  be := ToBE32(v);
  St.Write(be, 4);
end;

// Send "/addr" with a single float32 argument.
procedure SendFloat(const Address: string; Value: Single);
var St: TMemoryStream;
begin
  St := TMemoryStream.Create;
  try
    WriteOscString(St, Address);
    WriteOscString(St, ',f');
    WriteBE32(St, FloatBits(Value));
    fpSendTo(Sock, St.Memory, St.Size, 0, @Engine, SizeOf(Engine));
  finally
    St.Free;
  end;
end;

// Send "/addr" with a single int32 argument.
procedure SendInt(const Address: string; Value: LongInt);
var St: TMemoryStream;
begin
  St := TMemoryStream.Create;
  try
    WriteOscString(St, Address);
    WriteOscString(St, ',i');
    WriteBE32(St, LongWord(Value));
    fpSendTo(Sock, St.Memory, St.Size, 0, @Engine, SizeOf(Engine));
  finally
    St.Free;
  end;
end;

{ --- control actions --- }

procedure SetLevel(Ch: Integer; Level: Single);
begin
  SendFloat(Format('/channel/%d/level', [Ch]), Level);
end;

procedure SetMute(Ch: Integer; Mute: Boolean);
begin
  SendInt(Format('/channel/%d/mute', [Ch]), Ord(Mute));
end;

// Recall a scene: push every channel's stored level and mute to the engine.
procedure RecallScene(const Scene: TScene);
var i: Integer;
begin
  WriteLn(Format('Recalling scene "%s" (%d channels)...', [Scene.Name, Scene.Count]));
  for i := 0 to Scene.Count - 1 do
  begin
    SetLevel(i, Scene.Chan[i].Level);
    SetMute(i, Scene.Chan[i].Mute);
    WriteLn(Format('  ch %d  level %.2f  %s',
      [i, Scene.Chan[i].Level, BoolToStr(Scene.Chan[i].Mute, 'MUTE', 'on')]));
  end;
  WriteLn('Scene recalled.');
end;

{ --- built-in scenes --- }

function MakeScene(const Name: string; const Levels: array of Single): TScene;
var i: Integer;
begin
  Result.Name := Name;
  Result.Count := Length(Levels);
  for i := 0 to Result.Count - 1 do
  begin
    Result.Chan[i].Level := Levels[i];
    Result.Chan[i].Mute := False;
  end;
end;

function SceneShowtime: TScene;
begin
  // A musical balance: vocals and kick up, effects gentle.
  Result := MakeScene('Showtime', [0.82, 0.74, 0.55, 0.80, 0.62, 0.58, 0.90, 0.40]);
end;

function SceneSoundcheck: TScene;
var i: Integer;
begin
  Result := MakeScene('Soundcheck', [0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7]);
  for i := 0 to Result.Count - 1 do ; // flat; nothing muted
end;

function SceneSilence: TScene;
var i: Integer;
begin
  Result := MakeScene('Silence', [0, 0, 0, 0, 0, 0, 0, 0]);
  for i := 0 to Result.Count - 1 do
    Result.Chan[i].Mute := True;
end;

{ --- socket setup --- }

procedure OpenSocket;
{$IFDEF WINDOWS}
var wsa: TWSAData;
{$ENDIF}
begin
  {$IFDEF WINDOWS}
  if WSAStartup($0202, wsa) <> 0 then
  begin
    WriteLn('WSAStartup failed'); Halt(1);
  end;
  {$ENDIF}
  Sock := fpSocket(AF_INET, SOCK_DGRAM, 0);
  if Sock < 0 then begin WriteLn('socket() failed'); Halt(1); end;
  Engine.sin_family := AF_INET;
  Engine.sin_port := ToBE16(ENGINE_PORT);
  Engine.sin_addr := StrToNetAddr(ENGINE_HOST);
end;

{ --- entry point --- }

var
  cmd: string;
  ch: Integer;
  lvl: Single;
begin
  OpenSocket;

  if ParamCount = 0 then
    RecallScene(SceneShowtime)
  else
  begin
    cmd := LowerCase(ParamStr(1));
    if cmd = 'showtime' then
      RecallScene(SceneShowtime)
    else if cmd = 'soundcheck' then
      RecallScene(SceneSoundcheck)
    else if cmd = 'silence' then
      RecallScene(SceneSilence)
    else if (cmd = 'set') and (ParamCount >= 3) then
    begin
      ch := StrToInt(ParamStr(2));
      lvl := StrToFloat(ParamStr(3));
      SetLevel(ch, lvl);
      WriteLn(Format('Set channel %d level to %.2f', [ch, lvl]));
    end
    else
    begin
      WriteLn('usage: stagectl [showtime|soundcheck|silence|set <ch> <0..1>]');
      Halt(2);
    end;
  end;

  CloseSocket(Sock);
  {$IFDEF WINDOWS} WSACleanup; {$ENDIF}
end.
