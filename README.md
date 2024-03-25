
# bmake for Windows

## Building
You can build this using the Makefile with bmake itself or manually using msbuild.

## Notable Changes
- Arguments can be supplied with either `-` or `/`
- `.MAKE.PPID`, `.MAKE.UID` and `.MAKE.GID` are all set to -1
- Milliseconds used instead of microseconds in trace records
- The `.SHELL` target uses different sources, see [the wiki](https://github.com/VexedUXR/bmake/wiki/Targets#special-targets)
- `VPATH` is delimited with `;` instead of `:`
- By default, bmake only searches for `sys.mk` in `./mk` (if neither `MAKESYSPATH` or `-m` are used)

## Status
The binary itself is functional and can be used as expected.
Some of the mk files have been ported, though some functionality is missing,
most notabaly: there is no `install` target and no support for manpages.

Up to date with bmake `20240314`
