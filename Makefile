CC=clang

# Default mode is debug
MODE ?= debug

# If the target is 'release', override MODE to release
ifeq ($(MAKECMDGOALS),release)
  MODE = release
endif

# Target files for tracking configuration changes
BUILD_DIR=build
MODE_FILE=$(BUILD_DIR)/.mode
TRACK_FILE=$(BUILD_DIR)/.cflags

LAST_MODE := $(shell cat $(MODE_FILE) 2>/dev/null)
ifneq ($(filter install,$(MAKECMDGOALS)),)
  ifneq ($(LAST_MODE),)
    MODE = $(LAST_MODE)
  endif
endif

# Set CFLAGS based on the selected mode
ifeq ($(MODE),release)
  MODE_CFLAGS = -DNDEBUG -O3
else
  MODE_CFLAGS = -g -D_SHOW_ERROR -fsanitize=address
endif

# Use pkg-config for dependencies
PKG_CONFIG ?= pkg-config
NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null || echo "")
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || echo "-lncursesw -ltinfo")

CFLAGS = $(MODE_CFLAGS) -Ilib/tree-sitter/lib/src -Ilib/tree-sitter/lib/include -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 $(NCURSES_CFLAGS) -DDATADIR=\"$(DATADIR)\"

# Write the mode and CFLAGS tracking files at parse time if they have changed
$(shell mkdir -p $(BUILD_DIR))
$(shell echo '$(MODE)' | cmp -s - $(MODE_FILE) || echo '$(MODE)' > $(MODE_FILE))
$(shell echo '$(CFLAGS)' | cmp -s - $(TRACK_FILE) || echo '$(CFLAGS)' > $(TRACK_FILE))

executable=al # lsp_test test_line test_file

# C sources files
SRC_MODULES= \
	src/data-management/encoding/utf8.c \
	src/data-management/encoding/utf16.c \
	src/data-management/utf_8_extractor.c \
	src/advanced/shared.c \
	src/data-management/file_structure.c \
	src/data-management/file_management.c \
	src/utils/tools.c \
	src/utils/logger.c \
	src/io-management/io_manager.c \
	src/terminal/key_management.c \
	src/utils/clipboard_manager.c \
	src/io-management/viewport_history.c \
	src/data-management/state_control.c \
	src/terminal/term_handler.c \
	src/terminal/kitty_protocol.c \
	src/io-management/io_explorer.c \
	src/advanced/lsp/lsp_client.c \
	src/advanced/tree-sitter/tree_manager.c \
	src/advanced/tree-sitter/tree_query.c \
	src/advanced/tree-sitter/tree_sitter_highlighter.c \
	src/advanced/theme.c \
	src/terminal/highlight.c \
	src/terminal/click_handler.c \
	src/terminal/windows/few.c \
	src/terminal/windows/edw.c \
	src/terminal/windows/ofw.c \
	src/terminal/windows/pow.c \
	src/terminal/windows/tpw.c \
	src/terminal/windows/popups/search_popup.c \
	src/terminal/windows/popups/quit_popup.c \
	src/terminal/windows/popups/notification_popup.c \
	src/terminal/windows/widgets/text_box.c \
	src/terminal/graphics_tools.c \
	src/terminal/windows/popups/language_popup.c \
	src/config/config.c \
	src/config/language_feature.c \
	src/io-management/workspace_settings.c \
	src/advanced/lsp/lsp_handler.c \
	src/advanced/lsp/lsp_notification_dispatcher.c \
	src/advanced/lsp/lsp_response_dispatcher.c \
	src/advanced/lsp/lsp_highlighter.c \
	src/advanced/lsp/lsp-features/lsp_completion.c \
	src/advanced/lsp/lsp-features/lsp_formatting.c \
	src/advanced/lsp/lsp-features/lsp_goto.c \
	src/advanced/lsp/lsp-features/lsp_hover.c \
	src/advanced/lsp/lsp-features/lsp_signature_help.c \
	src/advanced/lsp/lsp-features/lsp_code_action.c \
	src/advanced/lsp/lsp-features/lsp_tools.c \
	src/advanced/lsp/lsp_dispatcher.c \
	src/advanced/lsp/lsp_emitter.c \
	src/advanced/intelligence/auto_pairs.c \
	src/advanced/intelligence/search.c \
	src/advanced/intelligence/comments.c \
	src/advanced/intelligence/indentation.c \
	src/core/editor_context.c \
	src/core/features/navigation_history.c \
	src/core/editor_init.c \
	src/core/editor_state.c \
	src/core/editor_render.c \
	src/core/editor_lsp.c \
	src/core/editor_input.c \
	src/core/editor_destroy.c \
	src/environnement/setup.c

# C Library sources
LIB_C_MODULES= \
	lib/cJSON/cJSON.c \
	lib/tree-sitter/lib/src/lib.c

# Rust Libraries (keep as .rlib)
RUST_MODULES= \
	lib/tree-sitter-c/target/release/libtree_sitter_c.rlib \
	lib/tree-sitter-python/target/release/libtree_sitter_python.rlib \
	lib/tree-sitter-java/target/release/libtree_sitter_java.rlib \
	lib/tree-sitter-cpp/target/release/libtree_sitter_cpp.rlib \
	lib/tree-sitter-c-sharp/target/release/libtree_sitter_c_sharp.rlib \
	lib/tree-sitter-make/target/release/libtree_sitter_make.rlib \
	lib/tree-sitter-css/target/release/libtree_sitter_css.rlib \
	lib/tree-sitter-dart/target/release/libtree_sitter_dart.rlib \
	lib/tree-sitter-go/target/release/libtree_sitter_go.rlib \
	lib/tree-sitter-javascript/target/release/libtree_sitter_javascript.rlib \
	lib/tree-sitter-json/target/release/libtree_sitter_json.rlib \
	lib/tree-sitter-bash/target/release/libtree_sitter_bash.rlib \
	lib/tree-sitter-markdown/target/release/libtree_sitter_md.rlib \
	lib/tree-sitter-query/target/release/libtree_sitter_tsquery.rlib \
	lib/tree-sitter-vhdl/target/release/libtree_sitter_vhdl.rlib \
	lib/tree-sitter-lua/target/release/libtree_sitter_lua.rlib \
	lib/tree-sitter-asm/target/release/libtree_sitter_asm.rlib \
	lib/tree-sitter-html/target/release/libtree_sitter_html.rlib \
	lib/tree-sitter-latex/target/release/libtree_sitter_latex.rlib \
	lib/tree-sitter-ini/target/release/libtree_sitter_ini.rlib

# Map sources to objects in BUILD_DIR
OBJECTS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRC_MODULES) $(LIB_C_MODULES))

SHARED_LIBS = $(NCURSES_LIBS)

all: $(OBJECTS) $(RUST_MODULES) $(executable)

release: all

# Make all objects depend on mode and flags tracking files to trigger rebuild on configuration change
$(OBJECTS): $(MODE_FILE) $(TRACK_FILE)

lib/tree-sitter-markdown/tree-sitter-markdown/libtree-sitter-markdown.a:
	cd lib/tree-sitter-markdown/tree-sitter-markdown/ && tree-sitter generate && $(MAKE)

lib/tree-sitter-markdown/tree-sitter-markdown-inline/libtree-sitter-markdown-inline.a:
	cd lib/tree-sitter-markdown/tree-sitter-markdown-inline/ && tree-sitter generate && $(MAKE)

lib/tree-sitter-latex/target/release/libtree_sitter_latex.rlib:
	cd lib/tree-sitter-latex && (test -f src/parser.c || tree-sitter generate) && cargo build --release

%.rlib:
	cd  $(shell echo $@ | cut -d/ -f1-2) && cargo build --release

# Compilation rule for objects
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test_line: src/test_line.c $(OBJECTS) $(RUST_MODULES)
	$(CC) $(CFLAGS) $^ -o $@ $(SHARED_LIBS)

test_file: src/test_file.c $(OBJECTS) $(RUST_MODULES)
	$(CC) $(CFLAGS) $^ -o $@ $(SHARED_LIBS)

al: src/main.c $(OBJECTS) $(RUST_MODULES)
	$(CC) $(CFLAGS) $^ -o $@ $(SHARED_LIBS)

lsp_test: src/lsp_test.c $(OBJECTS) $(RUST_MODULES)
	$(CC) $(CFLAGS) $^ -o $@ $(SHARED_LIBS)

clean:
	rm -rf $(BUILD_DIR) $(executable) lsp_test test_line test_file *.o

clean_all: clean
	find . -type d -name "target" -exec rm -rf {} +


# Installation configuration
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/alwide

# Combined install target
install: install-bin install-data

# Install the binary
install-bin: al
	install -v -D al $(DESTDIR)$(BINDIR)/al

# Install system-wide assets
install-data:
	mkdir -p $(DESTDIR)$(DATADIR)
	cp -rv assets/* $(DESTDIR)$(DATADIR)/

# Initialize user-specific configuration (run as normal user)
install-config:
	mkdir -p $(HOME)/.config/alwide
	cp -rv assets/* $(HOME)/.config/alwide

.PHONY: all release clean clean_all install install-bin install-data install-config
