// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_METAFILE_IMPL_H_
#define PRINTING_METAFILE_IMPL_H_

#if defined(OS_WIN)
#include "printing/emf_win.h"
#include "printing/pdf_metafile_skia.h"
#elif defined(OS_MACOSX)
#include "printing/pdf_metafile_cg_mac.h"
#elif defined(OS_POSIX)
#include "printing/pdf_metafile_cairo_linux.h"
#include "printing/pdf_metafile_skia.h"
#endif

namespace printing {

#if defined(OS_WIN)
typedef Emf NativeMetafile;
typedef PdfMetafileSkia PreviewMetafile;
#elif defined(OS_MACOSX)
typedef PdfMetafileCg NativeMetafile;
typedef PdfMetafileCg PreviewMetafile;
#elif defined(OS_POSIX)
typedef PdfMetafileCairo NativeMetafile;
typedef PdfMetafileSkia PreviewMetafile;
#endif

}  // namespace printing

#endif  // PRINTING_METAFILE_IMPL_H_
