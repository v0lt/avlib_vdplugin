#include <string>
#include "Utils.h"

extern "C" {
#include <libavutil/error.h>
}

std::string AVError2Str(const int errnum)
{
	char errBuf[AV_ERROR_MAX_STRING_SIZE] = {};
	return std::string(av_make_error_string(errBuf, sizeof(errBuf), errnum));
}