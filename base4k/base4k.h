/*   Base4K encoding for C
 *   Copyright (c) 2014 Secomba GmbH
 *   Licensed under the MIT license (https://raw.githubusercontent.com/secomba/base4k/master/LICENSE)
 *   Version: 1.0.0
 */

#ifndef __BASE4K_H__
#define __BASE4K_H__

#include <stdint.h>

namespace base4k
{
#define B4K_IN         const /* an input parameter */
#define B4K_OUT        /* an output parameter */
#define B4K_IN_OUT     /* an input and output parameter */
#define B4K_NEEDS_FREE /* the caller is responsible to free() this value after usage */
#define B4K_AUTO       0xffffffff /* string is 0 terminated */

typedef struct b4kEncodingSettings
{
    unsigned int base1Start;
} B4K_ENCODING_SETTINGS, *PB4K_ENCODING_SETTINGS;

typedef enum _b4kErrorCode
{
    B4K_SUCCESS         = 0,
    B4K_DECODING_ERROR  = 1,
    B4K_INVALID_VERSION = 2,
    B4K_MEMORY_ERROR    = 3
} b4kErrorCode;

/*  Initialize a B4K_STATE that is used for encoding.
*   Parameters:
*       * encodingSettings: A pointer to a B4K_ENCODING_SETTINGS, which will be initialized by this method
*       * version:          An integer indicating which version's encoding should be done. Can be 1 or 2.
*
*   Result: B4K_SUCCESS if successful, an error code otherwise.
*/
b4kErrorCode initialize(B4K_IN_OUT PB4K_ENCODING_SETTINGS encodingSettings, unsigned int version);

/*	Encode an uint8_t array to an unicode uint16_t representation
 *  Parameters:
 *      * encodingSettings: A pointer to a B4K_ENCODING_SETTINGS, which will dictate the encoding style
 *		* cData:	        The data to encode as uint8_t array
 *		* ccData:	        Contains the amount of bytes in cData. After returning successfully, contains the amount of
 *					        unicode code points in cEncoded. If not successful, ccData remains untouched.
 *		* cEncoded:	        A pointer to an array variable that will receive an uint16_t array containing the unicode
 *					        code points. The array is allocated with calloc(), the caller is responsible to free() them
 *					        if they are no longer required.
 *
 *	Result:	B4K_SUCCESS if successful, an error code otherwise.
 */
b4kErrorCode base4kEncode(B4K_IN PB4K_ENCODING_SETTINGS encodingSettings, B4K_IN uint8_t* cData, B4K_IN_OUT uint32_t* ccData, B4K_OUT B4K_NEEDS_FREE uint16_t** cEncoded);

/*	Decode an uint16_t array of code points to its original uint8_t representation
 *	Parameters:
 *		* cEncoded:		An array containing the unicode code points of the base4k encoded string
 *		* ccEncoded:	Contains the amount of code points in cEncoded. Set this value to B4K_AUTO if the array has a
 *						0 termination. After returning successfully, contains the amount of bytes in cDecoded.
 *						If not successful, ccEncoded remains untouched.
 *		* cDecoded:		A pointer to an array variable that will receive an uint8_t array containing the decoded data.
 *						The array is allocated with calloc(), the caller is responsible to free() them if they are
 *						no longer required.
 *
 *	Result: B4K_SUCCESS if successful, an error code otherwise.
 */
b4kErrorCode base4KDecode(B4K_IN uint16_t* cEncoded, B4K_IN_OUT uint32_t* ccEncoded, B4K_OUT B4K_NEEDS_FREE uint8_t** cDecoded);
}; // namespace

#endif
