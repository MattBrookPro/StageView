unit oscnet;
{$mode objfpc}{$H+}
{
  OSC-over-UDP for the legacy control panel - the same wire format as every other
  StageView client. Uses the Free Pascal Sockets unit for I/O (as stagectl does)
  plus WinSock for startup and a non-blocking flag, so the LCL timer can poll it.
}

interface

uses
  Sockets, WinSock2, SysUtils;

type
  TArgKind = (akInt, akFloat, akString);
  TOscArg = record
    Kind: TArgKind;
    I: LongInt;
    F: Single;
    S: string;
  end;
  TOscMsg = record
    Address: string;
    Count: Integer;
    Args: array[0..31] of TOscArg;
  end;

function OscInit(const Host: string; Port: Word): Boolean;
procedure OscClose;
procedure OscSendBare(const Addr: string);
procedure OscSendFloat(const Addr: string; V: Single);
procedure OscSendInt(const Addr: string; V: LongInt);
function OscReceive(out Msg: TOscMsg): Boolean; // non-blocking; True if a message was read

implementation

var
  Sock: LongInt = -1;
  Engine: TInetSockAddr;

{ --- byte order helpers (OSC is big-endian) --- }

function ToBE16(V: Word): Word;
begin
  Result := ((V and $00FF) shl 8) or ((V and $FF00) shr 8);
end;

function BE32(V: LongWord): LongWord;
begin
  Result := ((V and $000000FF) shl 24) or ((V and $0000FF00) shl 8)
         or ((V and $00FF0000) shr 8)  or ((V and $FF000000) shr 24);
end;

function FloatBits(F: Single): LongWord;
var u: record case Boolean of True: (s: Single); False: (l: LongWord); end;
begin
  u.s := F;
  Result := u.l;
end;

function BitsFloat(L: LongWord): Single;
var u: record case Boolean of True: (s: Single); False: (l: LongWord); end;
begin
  u.l := L;
  Result := u.s;
end;

{ --- encoding into a send buffer --- }

procedure PutOscString(var Buf: array of Byte; var Pos: Integer; const S: string);
var i, fieldLen, pad: Integer;
begin
  for i := 1 to Length(S) do begin Buf[Pos] := Byte(S[i]); Inc(Pos); end;
  Buf[Pos] := 0; Inc(Pos);                    // terminator
  fieldLen := Length(S) + 1;
  pad := (4 - (fieldLen mod 4)) mod 4;
  for i := 1 to pad do begin Buf[Pos] := 0; Inc(Pos); end;
end;

procedure PutBE32(var Buf: array of Byte; var Pos: Integer; V: LongWord);
var be: LongWord;
begin
  be := BE32(V);
  Move(be, Buf[Pos], 4);
  Inc(Pos, 4);
end;

procedure SendBuf(var Buf: array of Byte; Len: Integer);
begin
  if Sock >= 0 then
    fpSendTo(Sock, @Buf[0], Len, 0, @Engine, SizeOf(Engine));
end;

procedure OscSendBare(const Addr: string);
var Buf: array[0..255] of Byte; Pos: Integer;
begin
  Pos := 0;
  PutOscString(Buf, Pos, Addr);
  PutOscString(Buf, Pos, ',');
  SendBuf(Buf, Pos);
end;

procedure OscSendFloat(const Addr: string; V: Single);
var Buf: array[0..255] of Byte; Pos: Integer;
begin
  Pos := 0;
  PutOscString(Buf, Pos, Addr);
  PutOscString(Buf, Pos, ',f');
  PutBE32(Buf, Pos, FloatBits(V));
  SendBuf(Buf, Pos);
end;

procedure OscSendInt(const Addr: string; V: LongInt);
var Buf: array[0..255] of Byte; Pos: Integer;
begin
  Pos := 0;
  PutOscString(Buf, Pos, Addr);
  PutOscString(Buf, Pos, ',i');
  PutBE32(Buf, Pos, LongWord(V));
  SendBuf(Buf, Pos);
end;

{ --- decoding a received datagram --- }

function ReadOscString(const Buf: array of Byte; Len: Integer; var Pos: Integer): string;
var startPos, fieldLen: Integer;
begin
  startPos := Pos;
  while (Pos < Len) and (Buf[Pos] <> 0) do Inc(Pos);
  SetLength(Result, Pos - startPos);
  if Pos > startPos then Move(Buf[startPos], Result[1], Pos - startPos);
  fieldLen := (Pos - startPos) + 1;            // include the null
  Pos := startPos + fieldLen + ((4 - (fieldLen mod 4)) mod 4);
end;

function ReadBE32(const Buf: array of Byte; var Pos: Integer): LongWord;
var raw: LongWord;
begin
  Move(Buf[Pos], raw, 4);
  Inc(Pos, 4);
  Result := BE32(raw);
end;

function DecodeOsc(const Buf: array of Byte; Len: Integer; out Msg: TOscMsg): Boolean;
var Pos, i: Integer; Tags: string;
begin
  Result := False;
  Pos := 0;
  Msg.Count := 0;
  Msg.Address := ReadOscString(Buf, Len, Pos);
  if (Length(Msg.Address) = 0) or (Msg.Address[1] <> '/') then Exit;
  Tags := ReadOscString(Buf, Len, Pos);
  if (Length(Tags) = 0) or (Tags[1] <> ',') then Exit;
  for i := 2 to Length(Tags) do
  begin
    if Msg.Count > High(Msg.Args) then Break;
    case Tags[i] of
      'i': begin Msg.Args[Msg.Count].Kind := akInt;
                 Msg.Args[Msg.Count].I := LongInt(ReadBE32(Buf, Pos)); Inc(Msg.Count); end;
      'f': begin Msg.Args[Msg.Count].Kind := akFloat;
                 Msg.Args[Msg.Count].F := BitsFloat(ReadBE32(Buf, Pos)); Inc(Msg.Count); end;
      's': begin Msg.Args[Msg.Count].Kind := akString;
                 Msg.Args[Msg.Count].S := ReadOscString(Buf, Len, Pos); Inc(Msg.Count); end;
    else
      Exit; // unknown tag
    end;
  end;
  Result := True;
end;

{ --- socket lifecycle --- }

function OscInit(const Host: string; Port: Word): Boolean;
var wsa: TWSAData; nb: u_long;
begin
  Result := False;
  if WSAStartup($0202, wsa) <> 0 then Exit;
  Sock := fpSocket(AF_INET, SOCK_DGRAM, 0);
  if Sock < 0 then Exit;
  nb := 1;
  ioctlsocket(Sock, LongInt(FIONBIO), @nb);     // non-blocking receive
  Engine.sin_family := AF_INET;
  Engine.sin_port := ToBE16(Port);
  Engine.sin_addr := StrToNetAddr(Host);
  Result := True;
end;

procedure OscClose;
begin
  if Sock >= 0 then
  begin
    CloseSocket(Sock);
    Sock := -1;
    WSACleanup;
  end;
end;

function OscReceive(out Msg: TOscMsg): Boolean;
var Buf: array[0..4095] of Byte; n: LongInt; fromAddr: TInetSockAddr; fromLen: LongInt;
begin
  Result := False;
  if Sock < 0 then Exit;
  fromLen := SizeOf(fromAddr);
  n := fpRecvFrom(Sock, @Buf[0], SizeOf(Buf), 0, @fromAddr, @fromLen);
  if n <= 0 then Exit;                          // -1 / WSAEWOULDBLOCK when nothing waiting
  Result := DecodeOsc(Buf, n, Msg);
end;

end.
