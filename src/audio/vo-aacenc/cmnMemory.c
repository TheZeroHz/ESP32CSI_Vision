/*
 ** Copyright 2003-2010, VisualOn, Inc.
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */
/*******************************************************************************
	File:		cmnMemory.c

	Content:	sample code for memory operator implementation

*******************************************************************************/
#include "cmnMemory.h"

#include <stdlib.h>
#include <string.h>

#if (defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)) && defined(__has_include)
#if __has_include(<esp_heap_caps.h>)
#include <esp_heap_caps.h>
#define VOAAC_HAVE_ESP_HEAP_CAPS 1
#endif
#endif

//VO_MEM_OPERATOR		g_memOP;

#if defined(__GNUC__) || defined(__clang__)
#define VOAAC_WEAK __attribute__((weak))
#else
#define VOAAC_WEAK
#endif

#if defined(ARDUINO_ARCH_RP2040)
/* Some RP2040 cores expose an optional PSRAM allocator under this symbol. */
extern void *psram_malloc(size_t size) VOAAC_WEAK;
#endif

static void *try_psram_alloc(size_t size)
{
#if defined(VOAAC_HAVE_ESP_HEAP_CAPS)
	void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (ptr != NULL)
		return ptr;
#elif defined(ARDUINO_ARCH_RP2040)
	if (psram_malloc)
	{
		void *ptr = psram_malloc(size);
		if (ptr != NULL)
			return ptr;
	}
#endif

	return NULL;
}

VO_U32 cmnMemAlloc (VO_S32 uID,  VO_MEM_INFO * pMemInfo)
{
	if (!pMemInfo)
		return VO_ERR_INVALID_ARG;

	pMemInfo->VBuffer = try_psram_alloc((size_t)pMemInfo->Size);
	if (pMemInfo->VBuffer == NULL)
		pMemInfo->VBuffer = malloc (pMemInfo->Size);

	return 0;
}

VO_U32 cmnMemFree (VO_S32 uID, VO_PTR pMem)
{
	free (pMem);
	return 0;
}

VO_U32	cmnMemSet (VO_S32 uID, VO_PTR pBuff, VO_U8 uValue, VO_U32 uSize)
{
	memset (pBuff, uValue, uSize);
	return 0;
}

VO_U32	cmnMemCopy (VO_S32 uID, VO_PTR pDest, VO_PTR pSource, VO_U32 uSize)
{
	memcpy (pDest, pSource, uSize);
	return 0;
}

VO_U32	cmnMemCheck (VO_S32 uID, VO_PTR pBuffer, VO_U32 uSize)
{
	return 0;
}

VO_S32 cmnMemCompare (VO_S32 uID, VO_PTR pBuffer1, VO_PTR pBuffer2, VO_U32 uSize)
{
	return memcmp(pBuffer1, pBuffer2, uSize);
}

VO_U32	cmnMemMove (VO_S32 uID, VO_PTR pDest, VO_PTR pSource, VO_U32 uSize)
{
	memmove (pDest, pSource, uSize);
	return 0;
}

