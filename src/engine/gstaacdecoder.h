// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GSTAACDECODER_H
#define GSTAACDECODER_H

// Register only on Android; the decoder argument lets native tests exercise the same conversion.
bool RegisterGstAacDecoder(const char *decoder);

#endif
