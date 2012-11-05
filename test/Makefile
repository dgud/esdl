#  Copyright (c) 2001 Dan Gudmundsson
#  See the file "license.terms" for information on usage and redistribution
#  of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
#     $Id$
#

TestMods	= testsprite testgl testbin erldemo testaudio testjoy test_glfont test_ttf test_glimg
TestTargets	= $(TestMods:%=%.beam)

# Targets
target:	$(TestTargets)

clean:
	rm -f $(TestTargets)
	rm -f *~ core erl_crash.dump

# Rules
ESRC = .
EBIN = .
ERLC = erlc
ERLINC = ../include
ERL_COMPILE_FLAGS = -pa ../../esdl/ebin

%.beam: $(ESRC)/%.erl
	$(ERLC) -W -I$(ERLINC) -I../.. -bbeam $(ERL_FLAGS) $(ERL_COMPILE_FLAGS) -o$(EBIN) $<
