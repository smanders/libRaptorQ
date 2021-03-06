#
# Copyright (c) 2015, Luca Fulchir<luca@fulchir.it>, All rights reserved.
#
# This file is part of "libRaptorQ".
#
# libRaptorQ is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation, either version 3
# of the License, or (at your option) any later version.
#
# libRaptorQ is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# and a copy of the GNU Lesser General Public License
# along with libRaptorQ.  If not, see <http://www.gnu.org/licenses/>.

PROJECT(libRaptorQ_Doc)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

INCLUDE(../cmake/UseLATEX.cmake)

IF (LATEX_COMPILER AND PDFLATEX_COMPILER AND MAKEINDEX_COMPILER)
ADD_LATEX_DOCUMENT(libRaptorQ.tex
    INPUTS GNU_FDL.tex
    USE_INDEX
    MANGLE_TARGET_NAMES
    NO_DEFAULT
    )

ADD_CUSTOM_TARGET(docs DEPENDS libRaptorQ_pdf)
ELSE()
ADD_CUSTOM_TARGET(docs)
ENDIF()

INSTALL(FILES ${CMAKE_BINARY_DIR}/doc/libRaptorQ.pdf DESTINATION share/doc/ OPTIONAL)

