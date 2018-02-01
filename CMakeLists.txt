cmake_minimum_required(VERSION 3.8)
project(hdContents)
include_directories("include")

#link zilb
link_libraries(z)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        include/hdtd/config.h
        include/hdtd/context.h
        include/hdtd/document.h
        include/hdtd/stream.h
        include/hdtd/string-util.h
        include/hdtd/system.h
        include/hdtd/version.h
        include/pdf/document.h
        include/pdf/object.h
        include/pdf/xref.h
        include/hdtd.h
        include/memento.h
        include/pdf.h
		include/pdf/parse.h
		include/hdtd/buffer.h
		include/pdf/name-table.h 
        source/hdtd/context.c
        source/hdtd/document-all.c
        source/hdtd/document.c
        source/hdtd/error.c
        source/hdtd/hdtd-imp.h
        source/hdtd/memory.c
        source/hdtd/stream-open.c
        source/hdtd/string.c
        source/pdf/pdf-imp.h
        source/pdf/pdf-xref.c
        source/hdtd/stream-read.c
        source/pdf/pdf-object.c
		source/pdf/pdf-lex.c
		source/pdf/pdf-parse.c
		source/pdf/pdf-name-table.h
		source/hdtd/buffer.c
		main.c source/pdf/pdf-page.c include/pdf/page.h source/pdf/pdf-run.c include/pdf/interpret.h source/pdf/pdf-op-run.c source/pdf/pdf-interpret.c source/pdf/pdf-stream.c include/hdtd/compressed-buffer.h include/hdtd/filter.h source/hdtd/filter-basic.c source/hdtd/compressed-buffer.c source/hdtd/filter-flate.c include/pdf/font.h include/pdf/cmap.h source/pdf/pdf-font.c source/pdf/pdf-cmap-load.c source/pdf/pdf-cmap.c source/pdf/pdf-unicode.c source/pdf/pdf-cmap-parse.c include/hdtd/store.h source/hdtd/store.c include/hdtd/hash.h source/hdtd/hash.c include/pdf/resource.h source/pdf/pdf-store.c source/pdf/pdf-repair.c)

add_executable(hdContents ${SOURCE_FILES})