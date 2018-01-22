cmake_minimum_required(VERSION 3.8)
project(hdContents)
include_directories("include")

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
		main.c source/pdf/pdf-page.c include/pdf/page.h source/pdf/pdf-run.c include/pdf/interpret.h)

add_executable(hdContents ${SOURCE_FILES})