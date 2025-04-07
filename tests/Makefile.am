TEST_ENV = @TEST_ENV@
LOG_COMPILER=$(LIBTOOL) --mode=execute $(TEST_ENV)

TESTS = test-enc-dec

EXTRA_DIST = $(TESTS)

clean-local:
	$(RM) 1.ogg zeros.raw
