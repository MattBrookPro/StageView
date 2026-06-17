program legacypanel;
{$mode objfpc}{$H+}

uses
  Interfaces, // LCL widgetset
  Forms,
  mainform;

begin
  Application.Title := 'StageView Legacy Control';
  Application.Initialize;
  Application.CreateForm(TfrmMain, frmMain);
  Application.Run;
end.
