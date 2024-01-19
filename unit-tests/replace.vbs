' A simple script that reads input from file1 then replaces
' anything that matches the regex pattern in str1 with str2
' and writes the output to file2.

Sub quit(e)
	WScript.Echo "Usage: replace.vbs file1 file2 str1;str2 ..."
	WScript.Quit(e)
End Sub

Dim len

set args = WScript.Arguments
len = args.Length

If len < 3 Then
	Call quit(1)
End If

set reg = CreateObject("VBScript.RegExp")
set f = CreateObject("Scripting.FileSystemObject")
set out = f.OpenTextFile(args(1), 2, True)
rawout = f.OpenTextFile(args(0), 1).ReadAll

reg.Global = True

For i = 2 To len - 1
	strs = Split(Replace(args(i), "'", ""), ";")
	reg.Pattern = strs(0)
	rawout = reg.Replace(rawout, strs(1))
Next

out.Write rawout
