SITE_URL = git@github.com:jcrd/sessiond.org

BUILDDIR ?= builddir

all: compile_commands.json sessiond.org
build: $(BUILDDIR)/sessiond

$(BUILDDIR):
	meson $(BUILDDIR)

compile_commands.json: $(BUILDDIR)
	ln -s $(BUILDDIR)/compile_commands.json compile_commands.json

$(BUILDDIR)/sessiond: $(BUILDDIR)
	ninja -C $(BUILDDIR)

sessiond.org:
	git clone $(SITE_URL) $@

clean:
	rm -fr $(BUILDDIR)
	rm -f compile_commands.json

.PHONY: all build clean
