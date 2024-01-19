# Tests and test-specific variables.

# Everything in TESTS will be run
TESTS+=	cmd-errors
TESTS+=	cmd-errors-jobs
TESTS+=	cmd-errors-lint
TESTS+=	cmd-interrupt
TESTS+=	ternary
TESTS+=	varmisc
TESTS+=	archive-suffix
TESTS+=	compat-error
TESTS+=	meta-cmd-cmp
TESTS+=	cmdline-undefined
TESTS+=	comment
TESTS+=	cond-cmp-numeric
TESTS+=	cond-cmp-numeric-eq
TESTS+=	cond-cmp-numeric-ge
TESTS+=	cond-cmp-numeric-gt
TESTS+=	cond-cmp-numeric-le
TESTS+=	cond-cmp-numeric-lt
TESTS+=	cond-cmp-numeric-ne
TESTS+=	cond-cmp-string
TESTS+=	cond-cmp-unary
TESTS+=	cond-eof
TESTS+=	cond-func
TESTS+=	cond-func-commands
TESTS+=	cond-func-defined
TESTS+=	cond-func-empty
TESTS+=	cond-func-exists
TESTS+=	cond-func-make
TESTS+=	cond-func-target
TESTS+=	cond-late
TESTS+=	cond-op
TESTS+=	cond-op-and
TESTS+=	cond-op-and-lint
TESTS+=	cond-op-not
TESTS+=	cond-op-or
TESTS+=	cond-op-or-lint
TESTS+=	cond-op-parentheses
TESTS+=	cond-short
TESTS+=	cond-token-number
TESTS+=	cond-token-plain
TESTS+=	cond-token-string
TESTS+=	cond-token-var
TESTS+=	cond-undef-lint
TESTS+=	cond-func-make-main
TESTS+=	job-flags
TESTS+=	job-output-long-lines
TESTS+=	jobs-empty-commands
TESTS+=	jobs-empty-commands-error
TESTS+=	jobs-error-indirect
TESTS+=	jobs-error-nested
TESTS+=	jobs-error-nested-make
TESTS+=	objdir-writable
TESTS+=	opt
TESTS+=	opt-chdir
TESTS+=	opt-debug
TESTS+=	opt-debug-cond
TESTS+=	opt-debug-curdir
TESTS+=	opt-debug-errors
TESTS+=	opt-debug-errors-jobs
TESTS+=	opt-debug-file
TESTS+=	opt-debug-for
TESTS+=	opt-debug-graph1
TESTS+=	opt-debug-graph2
TESTS+=	opt-debug-graph3
TESTS+=	opt-debug-hash
TESTS+=	opt-debug-lint
TESTS+=	opt-debug-loud
TESTS+=	opt-debug-parse
TESTS+=	opt-debug-var
TESTS+=	opt-define
TESTS+=	opt-env
TESTS+=	opt-ignore
TESTS+=	opt-jobs
TESTS+=	opt-jobs-internal
TESTS+=	opt-keep-going
TESTS+=	opt-keep-going-indirect
TESTS+=	opt-keep-going-multiple
TESTS+=	opt-m-include-dir
TESTS+=	opt-no-action
TESTS+=	opt-no-action-runflags
TESTS+=	opt-no-action-touch
TESTS+=	opt-query
TESTS+=	opt-raw
TESTS+=	opt-silent
TESTS+=	opt-tracefile
TESTS+=	opt-var-expanded
TESTS+=	opt-var-literal
TESTS+=	opt-version
TESTS+=	opt-warnings-as-errors
TESTS+=	opt-where-am-i
TESTS+=	opt-x-reduce-exported
TESTS+=	order

# These override the default -k
ARGS.cond-func-make=	via-cmdline
ARGS.jobs-error-indirect=	
ARGS.jobs-error-nested=	
ARGS.jobs-error-nested-make=	

# These have priority over the ones in ${CHANGE}
CHANGE.opt-debug-parse:= \
'${.PARSEDIR:S,\\,\\\\,g}'\\; \
'${.PARSEDIR:S,\\,\\\\,g}';<some-dir>

CHANGE.opt-debug-graph1= \
'= *[0-9]{1,};= <details omitted>'

CHANGE.opt-debug-graph2= \
${CHANGE.opt-debug-graph1} \
'..:..:.. ... .., ....;<timestamp>' \
'${.SHELL:S,\\,\\\\,g};<details omitted>'

CHANGE.opt-debug-graph3= \
${CHANGE.opt-debug-graph2}

CHANGE.opt-debug-hash= \
numEntries=[1-9][0-9]*;numEntries=<entries>

CHANGE.opt-no-action-runflags= \
'echo hide-from-output.*\n;' \
'hide-from-output ;' \
'hide-from-output;'

CHANGE.opt-where-am-i= \
'${.CURDIR:S,\\,\\\\,g};<curdir>'

CHANGE.opt-tracefile= \
'[0-9]{10,10}.[0-9]{3,3} ;' \
' [0-9]* ${.CURDIR:S,\\,\\\\,g}.*;'

# Enviroment variables for tests
ENV.opt-env= \
FROM_ENV=value-from-env

ENV.varmisc= \
FROM_ENV=env \
FROM_ENV_BEFORE=env \
FROM_ENV_AFTER=env

# Order the tests
TESTS:=	${TESTS:O}
