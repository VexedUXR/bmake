
# bmake for Windows

## Building
You can build this either by running `msbuild`, or opening `bmake.sln` and building through Visual Studio.

## Notable Changes
- Arguments can be supplied with either `-` or `/`
- `.MAKE.PPID`, `.MAKE.UID` and `.MAKE.GID` are all set to -1
- Milliseconds used instead of microseconds in trace records
- The `.SHELL` target uses different sources
- The `VPATH` variable is treated as a normal variable  
<sub>This is because `VPATH` uses `:` as a delimiter for the paths, which is a valid character in a Windows path.
Since `VPATH` is only here for compatibility,  
there is no point in changing the delimiter.</sub>
- By default, bmake only searches for `sys.mk` in `./mk` (if neither `MAKESYSPATH` or `-m` are used)

## Status
The binary itself is functional and can be used as expected, there is still alot of work to be done for the mk files however.  
`sys.mk` basically does nothing and the rest of the mk collection is yet to be ported.
