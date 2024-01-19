# $NetBSD: ternary.mk,v 1.2 2020/10/24 08:34:59 rillig Exp $

all:
	@echo off &\
	@FOR %i IN ("B=" "A=" "A=42") DO ${.MAKE} -f ${MAKEFILE} show -m../mk %i

show:
	@echo The answer is ${A:?known:unknown}
	@echo The answer is ${A:?$A:unknown}
	@echo The answer is ${empty(A):?empty:$A}
