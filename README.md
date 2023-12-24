
# bmake for Windows

## Building
You can build this either by running `msbuild`, or opening `bmake.sln` and building through Visual Studio.

## Notable Changes
- Arguments can be supplied with either `-` or `/`
- `.MAKE.PPID`, `.MAKE.UID` and `.MAKE.GID` are all set to -1
- Milliseconds used instead of microseconds in trace records
- The `.SHELL` target uses different sources, see [the wiki](https://github.com/VexedUXR/bmake/wiki#special-targets)
- `VPATH` is delimited with `;` instead of `:`
- By default, bmake only searches for `sys.mk` in `./mk` (if neither `MAKESYSPATH` or `-m` are used)

## Status
The binary itself is functional and can be used as expected, there is still alot of work to be done for the mk files however.  
`sys.mk` basically does nothing and the rest of the mk collection is yet to be ported.
