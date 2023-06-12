MANDIR = content/man
MANPAGES = $(MANDIR)/sessiond.1 \
		   $(MANDIR)/sessionctl.1 \
		   $(MANDIR)/sessiond-inhibit.1 \
		   $(MANDIR)/sessiond.conf.5 \
		   $(MANDIR)/sessiond-hooks.5 \
		   $(MANDIR)/sessiond-dbus.8

SPHINX_FILES = sphinx/conf.py \
			   sphinx/index.rst \
			   ../python-sessiond/sessiond.py

all: $(MANPAGES:=.md) content/python.md

$(MANDIR)/%.md: ../man/%.pod
	echo -e '---\ntitle: $*\n---\n' > $@
	pod2markdown --html-encode-chars '|' $< | \
		./scripts/link_manpages.pl $(MANDIR) $(MANPAGES) >> $@

content/python.md: $(SPHINX_FILES)
	sphinx-build -M markdown sphinx _sphinx
	cp sphinx/template.md $@
	cat _sphinx/markdown/index.md >> $@
	rm -rf _sphinx

build:
	hugo --minify

serve:
	hugo serve

clean:
	rm -fr docs

.PHONY: all build serve clean
