
# bmake for Windows

## Building
You can build this by running `bmake`

## Notable Changes
- Arguments can be supplied with either `-` or `/`
- `.MAKE.PPID`, `.MAKE.UID` and `.MAKE.GID` are all set to -1
- Milliseconds used instead of microseconds in trace records
- `VPATH` is delimited with `;` instead of `:`
- By default, bmake only searches for `sys.mk` in `./mk` (if neither `MAKESYSPATH` or `-m` are used)
- The `.SHELL` target uses different sources:

  name: This is the minimal specification, used to select one of the built-in shell specs; cmd and pwsh.

  path: Specifies the absolute path to the shell.

  echo: Template to echo a command.

  ignore: Template to execute a command without error checking.

  check: Template to execute a command with error checking.

  args: Arguments to be passed to the shell, this should always end with the 'exec' flag (/c).

  meta: A list of characters that are treated specially by the shell.

  special: Characters that need to be escaped in a special way, multiple characters can be added in a comma separated list. e.g a,!a,b,^b, means that 'a' can be escaped using '!a' and 'b' can be escaped using '^b'. The delimiter can be anything.

  separator: Character used by the shell to "concatenate" commands, allowing multiple commands on one line.

  escape: Character used by the shell to escape characters.

  comment: Character used by the shell for comments.

NOTE: Template here means a printf style format string.
  
  Example:
  ```
  .SHELL: name=cmd.exe path=C:\\Windows\\System32\\cmd.exe \
  echo="echo %s" ignore="%s" check="%s||exit" separator=& \
  meta=\n%&<>^| escape=^ special=\n,&echo:, args=/c
```
## Status
The binary itself is functional and can be used as expected.
Some of the mk files have been ported, though some functionality is missing,
most notabaly: there is no `install` target and no support for manpages.

Up to date with bmake `20240414`
