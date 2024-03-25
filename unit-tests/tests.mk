# Tests and test-specific variables.

# Everything in TESTS will be run
TESTS+= \
cmd-errors \
cmd-errors-jobs \
cmd-errors-lint \
ternary \
varmisc \
archive-suffix \
compat-error \
meta-cmd-cmp \
cmdline-undefined \
comment \
cond-cmp-numeric \
cond-cmp-numeric-eq \
cond-cmp-numeric-ge \
cond-cmp-numeric-gt \
cond-cmp-numeric-le \
cond-cmp-numeric-lt \
cond-cmp-numeric-ne \
cond-cmp-string \
cond-cmp-unary \
cond-eof \
cond-func \
cond-func-commands \
cond-func-defined \
cond-func-empty \
cond-func-exists \
cond-func-make \
cond-func-target \
cond-late \
cond-op \
cond-op-and \
cond-op-and-lint \
cond-op-not \
cond-op-or \
cond-op-or-lint \
cond-op-parentheses \
cond-short \
cond-token-number \
cond-token-plain \
cond-token-string \
cond-token-var \
cond-undef-lint \
cond-func-make-main \
job-flags \
job-output-long-lines \
jobs-empty-commands \
jobs-empty-commands-error \
jobs-error-indirect \
jobs-error-nested \
jobs-error-nested-make \
objdir-writable \
opt \
opt-chdir \
opt-debug \
opt-debug-cond \
opt-debug-curdir \
opt-debug-errors \
opt-debug-errors-jobs \
opt-debug-file \
opt-debug-for \
opt-debug-graph1 \
opt-debug-graph2 \
opt-debug-graph3 \
opt-debug-hash \
opt-debug-lint \
opt-debug-loud \
opt-debug-parse \
opt-debug-var \
opt-define \
opt-env \
opt-ignore \
opt-jobs \
opt-jobs-internal \
opt-keep-going \
opt-keep-going-indirect \
opt-keep-going-multiple \
opt-m-include-dir \
opt-no-action \
opt-no-action-runflags \
opt-no-action-touch \
opt-query \
opt-raw \
opt-silent \
opt-tracefile \
opt-var-expanded \
opt-var-literal \
opt-version \
opt-warnings-as-errors \
opt-where-am-i \
opt-x-reduce-exported \
order \
dep \
dep-colon \
dep-colon-bug-cross-file \
dep-double-colon \
dep-double-colon-indep \
dep-duplicate \
dep-none \
dep-op-missing \
dep-percent \
depsrc \
depsrc-end \
depsrc-exec \
depsrc-ignore \
depsrc-made \
depsrc-make \
depsrc-meta \
depsrc-notmain \
depsrc-optional \
depsrc-phony \
depsrc-recursive \
depsrc-silent \
depsrc-use \
depsrc-usebefore \
depsrc-usebefore-double-colon \
depsrc-wait \
deptgt \
deptgt-begin \
deptgt-begin-fail \
deptgt-begin-fail-indirect \
deptgt-default \
deptgt-delete_on_error \
deptgt-end \
deptgt-end-fail \
deptgt-end-fail-all \
deptgt-end-fail-indirect \
deptgt-end-jobs \
deptgt-error \
deptgt-ignore \
deptgt-main \
deptgt-makeflags \
deptgt-notparallel \
deptgt-order \
deptgt-path-suffix \
deptgt-phony \
deptgt-posix \
deptgt-silent \
deptgt-silent-jobs \
deptgt-suffixes \
dep-var \
dep-wildcards \
opt-debug-jobs
.if !defined(SSH_CLIENT)
TESTS+= \
cmd-interrupt \
deptgt-interrupt
.endif

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

CHANGE.depsrc-wait= \
'--- .* ---\n'; \
3a[123];3a 3b[123];3b 3c[123];3c

CHANGE.deptgt-makeflags= \
${TEST_MAKE:S,\\,/,g};bmake.exe

CHANGE.deptgt-suffixes= \
'(.|\n)*  \.\.\n\n';

CHANGE.opt-debug-jobs= \
\([0-9]*\);(<pid>) \
'pid [0-9]*;pid <pid>' \
'Process [0-9]*;Process <pid>' \
'JobFinish: [0-9]*;JobFinish: <pid>' \
'${.SHELL:S,\\,\\\\,g};<shell>' \
'handle [0-9A-F]*;handle <handle>'

# Enviroment variables for tests
ENV.opt-env= \
FROM_ENV=value-from-env

ENV.varmisc= \
FROM_ENV=env \
FROM_ENV_BEFORE=env \
FROM_ENV_AFTER=env

# Order the tests
TESTS:=	${TESTS:O}
